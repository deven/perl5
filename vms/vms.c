/* VMS-specific routines for perl5
 *
 * Last revised: 17-Aug-1994
 */

#include <descrip.h>
#include <jpidef.h>
#include <lib$routines.h>
#include <ssdef.h>
#include <starlet.h>

#include "EXTERN.h"
#include "perl.h"

static unsigned long int sts;

#define _cksts(call) \
  if (!(sts=(call))&1) { \
    errno = EVMSERR; vaxc$errno = sts; \
    croak("fatal error at %s, line %d",__FILE__,__LINE__); \
  } else { 1; }

/*{{{ void  my_setenv(char *lnm, char *eqv)*/
void
my_setenv(char *lnm,char *eqv)
/* Define a supervisor-mode logical name in the process table.
 * In the future we'll add tables, attribs, and acmodes,
 * probably through a different call.
 */
{
#include <descrip.h>
#include <lib$routines.h>
    $DESCRIPTOR(tabdsc,"LNM$PROCESS");
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            eqvdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};

    lnmdsc.dsc$w_length = strlen(lnm);
    lnmdsc.dsc$a_pointer = lnm;
    eqvdsc.dsc$w_length = strlen(eqv);
    eqvdsc.dsc$a_pointer = eqv;

    _cksts(lib$set_logical(&lnmdsc,&eqvdsc,&tabdsc,0,0));

}  /* end of my_setenv() */
/*}}}*/

/*{{{  my_popen and my_pclose*/
struct pipe_details
{
    struct pipe_details *next;
    FILE *fp;
    int pid;
    int mailbox_number;  /* probably dont need this */
    unsigned long int completion;
};

static struct pipe_details *open_pipes = NULL;
static $DESCRIPTOR(nl_desc, "NL:");
static $DESCRIPTOR(cmd_desc, "");
static char mbx_buffer[80];
static $DESCRIPTOR(mbx_desc, mbx_buffer);
static int mbx_number=0;  /* should use time or something to generate mbx ? */
static int waitpid_asleep = 0;

static void
popen_completion_ast(unsigned long int unused)
{
  if (waitpid_asleep) {
    waitpid_asleep = 0;
    sys$wake(0,0);
  }
}

/*{{{  FILE *my_popen(char *cmd, char *mode)*/
FILE *
my_popen(char *cmd, char *mode)
{
    unsigned long int chan;
    unsigned long int flags=1;  /* nowait - gnu c doesn't allow &1 */
    struct pipe_details *info;

    New(7001,info,1,struct pipe_details);

    info->mailbox_number=mbx_number;
    info->completion=0;  /* I assume this will remain 0 until terminates */
        
    /* create mailbox */
    mbx_desc.dsc$w_length=sprintf(mbx_buffer, "PERL_MAILBOX_%d", mbx_number++);
    _cksts(sys$crembx(0, &chan, 0,0,0,0,&mbx_desc));

    /* open a FILE* onto it */
    info->fp=fopen(mbx_buffer, mode);

    /* give up other channel onto it */
    _cksts(sys$dassgn(chan));

    if (!info->fp)
        return Nullfp;
        
    cmd_desc.dsc$w_length=strlen(cmd);
    cmd_desc.dsc$a_pointer=cmd;

    if (strcmp(mode,"r")==0) {
      _cksts(lib$spawn(&cmd_desc, &nl_desc, &mbx_desc, &flags,
                     0  /* name */, &info->pid, &info->completion,
                     0, popen_completion_ast,0,0,0,0));
    }
    else {
      _cksts(lib$spawn(&cmd_desc, &mbx_desc, 0 /* sys$output */,
                     0  /* name */, &info->pid, &info->completion));
    }

    info->next=open_pipes;  /* prepend to list */
    open_pipes=info;
        
    return info->fp;
}
/*}}}*/

/*{{{  I32 my_pclose(FILE *fp)*/
I32 my_pclose(FILE *fp)
{
    struct pipe_details *info, *last = NULL;
    unsigned long int abort = SS$_TIMEOUT;
    
    for (info = open_pipes; info != NULL; last = info, info = info->next)
        if (info->fp == fp) break;

    if (info == NULL)
      /* get here => no such pipe open */
      croak("my_pclose() - no such pipe open ???");

    if (!info->completion) { /* Tap them gently on the shoulder . . .*/
      _cksts(sys$forcex(&info->pid,0,&abort));
      sleep(1);
    }
    if (!info->completion)  /* We tried to be nice . . . */
      _cksts(sys$delprc(&info->pid));
    
    fclose(info->fp);
    /* remove from list of open pipes */
    if (last) last->next = info->next;
    else open_pipes = NULL;
    free(info);

    return info->completion;
}  /* end of my_pclose() */

#ifndef HAS_WAITPID
/* sort-of waitpid; use only with popen() */
/*{{{unsigned long int waitpid(unsigned long int pid, int *statusp, int flags)*/
unsigned long int
waitpid(unsigned long int pid, int *statusp, int flags)
{
    struct pipe_details *info;
    unsigned long int abort = SS$_TIMEOUT;
    
    for (info = open_pipes; info != NULL; info = info->next)
        if (info->pid == pid) break;

    if (info != NULL) {  /* we know about this child */
      while (!info->completion) {
        waitpid_asleep = 1;
        sys$hiber();
      }

      *statusp = info->completion;
      return pid;
    }
    else {  /* we haven't heard of this child */
      $DESCRIPTOR(intdsc,"0 00:00:01");
      unsigned long int ownercode = JPI$_OWNER, ownerpid, mypid;
      unsigned long int interval[2];

      _cksts(lib$getjpi(&ownercode,&pid,0,&ownerpid,0,0));
      _cksts(lib$getjpi(&ownercode,0,0,&mypid,0,0));
      if (ownerpid != mypid)
        croak("pid %d not a child",pid);

      _cksts(sys$bintim(&intdsc,interval));
      while ((sts=lib$getjpi(&ownercode,&pid,0,&ownerpid,0,0)) & 1) {
        _cksts(sys$schdwk(0,0,interval,0));
        _cksts(sys$hiber());
      }
      _cksts(sts);

      /* There's no easy way to find the termination status a child we're
       * not aware of beforehand.  If we're really interested in the future,
       * we can go looking for a termination mailbox, or chase after the
       * accounting record for the process.
       */
      *statusp = 0;
      return pid;
    }
                    
}  /* end of waitpid() */
#endif   
/*}}}*/
/*}}}*/
/*}}}*/
