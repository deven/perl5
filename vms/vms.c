/* VMS-specific routines for perl5
 *
 * Last revised: 17-Aug-1994
 */

#include <descrip.h>
#include <dvidef.h>
#include <float.h>
#include <fscndef.h>
#include <iodef.h>
#include <jpidef.h>
#include <lib$routines.h>
#include <rms.h>
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


/***** The following two routines are temporary, and should be removed,
 * along with the corresponding #defines in vmsish.h, when TCP/IP support
 * has been added to the VMS port of perl5.  (The temporary hacks are
 * here now sho that pack can handle type N elements.)
 * - C. Bailey 16-Aug-1994
 *****/

/*{{{ unsigned short int tmp_shortflip(unsigned short int val)*/
unsigned short int
tmp_shortflip(unsigned short int val)
{
    return val << 8 | val >> 8;
}
/*}}}*/

/*
** The following routines are provided to make life easier when
** converting among VMS-style and Unix-style directory specifications.
** All will take input specifications in either VMS or Unix syntax. On
** failure, all return NULL.  If successful, the routines listed below
** return a pointer to a static buffer containing the appropriately
** reformatted spec (and, therefore, subsequent calls to that routine
** will clobber the result), while the routines of the same names with
** a _ts suffix appended will return a pointer to a mallocd string
** containing the appropriately reformatted spec.
** In all cases, only explicit syntax is altered; no check is made that
** the resulting string is valid or that the directory in question
** actually exists.
**
**   fileify_dirspec() - convert a directory spec into the name of the
**     directory file (i.e. what you can stat() to see if it's a dir).
**     The style (VMS or Unix) of the result is the same as the style
**     of the parameter passed in.
**   pathify_dirspec() - convert a directory spec into a path (i.e.
**     what you prepend to a filename to indicate what directory it's in).
**     The style (VMS or Unix) of the result is the same as the style
**     of the parameter passed in.
**   tounixpath() - convert a directory spec into a Unix-style path.
**   tovmspath() - convert a directory spec into a VMS-style path.
**   tounixspec() - convert any file spec into a Unix-style file spec.
**   tovmsspec() - convert any file spec into a VMS-style spec.
 */

/*{{{ char *fileify_dirspec[_ts](char *path)*/
static char *do_fileify_dirspec(dir,ts)
     char *dir;
     unsigned long int ts;
{
    static char __fileify_retbuf[NAM$C_MAXRSS+1];
    unsigned long int dirlen, retlen;
    char *retspec, *cp1, *cp2;

    if (dir == NULL) return NULL;

    dirlen = strlen(dir);
    if (cp1 = strrchr(dir,'/')) {  /* Unix-style path */
      if (dir[dirlen-1] == '/') {    /* path ends with '/'; just add .dir;1 */
        if (ts) retspec = safemalloc(dirlen+5);         /* to last element */
        else retspec == __fileify_retbuf;
        strcpy(retspec,dir);
        retspec[dirlen-1] = '\0';
      }
      else if ((cp2 = strchr(cp1,'.'))) {  /* look for explicit type */
        if (toupper(*(cp2+1)) == 'D' &&    /* Yep.  Is it .dir? */
            toupper(*(cp2+2)) == 'I' &&    /* Ignore (and remove) version */
            toupper(*(cp2+3)) == 'R') {
          retlen = cp2 - dir;
          if (ts) retspec = safemalloc(retlen+6);
          else retspec = __fileify_retbuf;
          strncpy(retspec,dir,retlen);
          retspec[retlen] = '\0';
        }
        else {  /* There's a type, and it's not .dir.  Bzzt. */
          errno = ENOTDIR;
          return NULL;
        }
      }
      else {  /* No type specified.  Just copy over the input spec */
        if (ts) retspec = safemalloc(dirlen+6);
        else retspec = __fileify_retbuf;
        strcpy(retspec,dir);
      }
      /* We've picked up everything up to the directory file name.
         Now just add the type and version, and we're set. */
      strcat(retspec,".dir;1");
      return retspec;
    }
    else {  /* VMS-style directory spec */
      char esa[NAM$C_MAXRSS+1], term;
      unsigned long int sts, cmplen;
      struct FAB dirfab = cc$rms_fab;
      struct NAM dirnam = cc$rms_nam;

      dirfab.fab$b_fns = strlen(dir);
      dirfab.fab$l_fna = dir;
      dirfab.fab$l_nam = &dirnam;
      dirnam.nam$b_ess = NAM$C_MAXRSS;
      dirnam.nam$l_esa = esa;
      dirnam.nam$b_nop = NAM$M_SYNCHK;
      if (!(sys$parse(&dirfab)&1)) {
        errno = EVMSERR;
        vaxc$errno = dirfab.fab$l_sts;
        return NULL;
      }

      if (dirnam.nam$l_fnb & NAM$M_EXP_TYPE) {  /* Was type specified? */
        /* Yep; check version while we're at it, if it's there. */
        cmplen = (dirnam.nam$l_fnb & NAM$M_EXP_VER) ? 6 : 4;
        if (strncmp(dirnam.nam$l_type,".DIR;1",cmplen)) { 
          /* Something other than .DIR[;1].  Bzzt. */
          errno = ENOTDIR;
          return NULL;
        }
        else {  /* Ok, it was .DIR[;1]; copy over everything up to the */
          retlen = dirnam.nam$l_type - esa;           /* file name. */
          if (ts) retspec = safemalloc(retlen+6);
          else retspec = __fileify_retbuf;
          strncpy(retspec,esa,retlen);
          retspec[retlen] = '\0';
        }
      }
      else {
        /* They didn't explicitly specify the directory file.  Ignore
           any file names in the input, pull off the last element of the
           directory path, and make it the file name.  If you want to
           pay attention to filenames without .dir in the input, just use
           ".DIR;1" as a default filespec for the $PARSE */
        esa[dirnam.nam$b_esl] = '\0';
        if ((cp1 = strrchr(esa,']')) == NULL) cp1 = strrchr(esa,'>');
        if (cp1 == NULL) return NULL; /* should never happen */
        term = *cp1;
        *cp1 = '\0';
        retlen = strlen(esa);
        if ((cp1 = strrchr(esa,'.')) != NULL) {
          /* There's more than one directory in the path.  Just roll back. */
          *cp1 = term;
          if (ts) retspec = safemalloc(retlen+6);
          else retspec = __fileify_retbuf;
          strcpy(retspec,esa);
        }
        else { /* This is a top-level dir.  Add the MFD to the path. */
          if (ts) retspec = safemalloc(retlen+14);
          else retspec = __fileify_retbuf;
          cp1 = esa;
          cp2 = retspec;
          while (*cp1 != ':') *(cp2++) = *(cp1++);
          strcpy(cp2,":[000000]");
          cp1 += 2;
          strcpy(cp2+9,cp1);
        }
      } 
      /* Again, we've set up the string up through the filename.  Add the
         type and version, and we're done. */
      strcat(retspec,".DIR;1");
      return retspec;
    }
}  /* end of do_fileify_dirspec() */
/*}}}*/
/* External entry points */
char *fileify_dirspec(dir) char *dir;
{ return do_fileify_dirspec(dir,0); }
char *fileify_dirspec_ts(dir) char *dir;
{ return do_fileify_dirspec(dir,1); }

/*{{{ char *pathify_dirspec[_ts](char *path)*/
static char *do_pathify_dirspec(dir,ts)
    char *dir;
    unsigned long int ts;
{
    static char __pathify_retbuf[NAM$C_MAXRSS+1];
    unsigned long int retlen;
    char *retpath, *cp1, *cp2;

    if (dir == NULL) return NULL;

    if (cp1 = strrchr(dir,'/')) {  /* Unix-style path */
      if (cp2 = strchr(cp1,'.')) {
        if (toupper(*(cp2+1)) == 'D' &&  /* They specified .dir. */
            toupper(*(cp2+2)) == 'I' &&  /* Trim it off. */
            toupper(*(cp2+3)) == 'R') {
          retlen = cp2 - dir + 1;
        }
        else {  /* Some other file type.  Bzzt. */
          errno = ENOTDIR;
          return NULL;
        }
      }
      else {  /* No file type present.  Treat the filename as a directory. */
        retlen = strlen(dir) + 1;
      }
      if (ts) retpath = safemalloc(retlen);
      else retpath = __pathify_retbuf;
      strncpy(retpath,dir,retlen-1);
      if (retpath[retlen-2] != '/') { /* If the path doesn't already end */
        retpath[retlen-1] = '/';      /* with '/', add it. */
        retpath[retlen] = '\0';
      }
      else retpath[retlen-1] = '\0';
    }
    else {  /* VMS-style directory spec */
      char esa[NAM$C_MAXRSS];
      unsigned long int sts, cmplen;
      struct FAB dirfab = cc$rms_fab;
      struct NAM dirnam = cc$rms_nam;

      dirfab.fab$b_fns = strlen(dir);
      dirfab.fab$l_fna = dir;
      dirfab.fab$l_nam = &dirnam;
      dirnam.nam$b_ess = sizeof esa;
      dirnam.nam$l_esa = esa;
      dirnam.nam$b_nop = NAM$M_SYNCHK;
      if (!(sys$parse(&dirfab)&1)) {
        errno = EVMSERR;
        vaxc$errno = dirfab.fab$l_sts;
        return NULL;
      }

      if (dirnam.nam$l_fnb & NAM$M_EXP_TYPE) {  /* Was type specified? */
        /* Yep; check version while we're at it, if it's there. */
        cmplen = (dirnam.nam$l_fnb & NAM$M_EXP_VER) ? 6 : 4;
        if (strncmp(dirnam.nam$l_type,".DIR;1",cmplen)) { 
          /* Something other than .DIR[;1].  Bzzt. */
          errno = ENOTDIR;
          return NULL;
        }
        /* OK, the type was fine.  Now pull any file name into the
           directory path. */
        if (cp1 = strrchr(esa,']')) *dirnam.nam$l_type = ']';
        else {
          cp1 = strrchr(esa,'>');
          *dirnam.nam$l_type = '>';
        }
        *cp1 = '.';
        *(dirnam.nam$l_type + 1) = '\0';
        retlen = dirnam.nam$l_type - esa + 2;
      }
      else {
        /* There wasn't a type on the input, so ignore any file names as
           well.  If you want to pay attention to filenames without .dir
           in the input, just use ".DIR;1" as a default filespec for
           the $PARSE and set retlen thus
        retlen = (dirnam.nam$b_rsl ? dirnam.nam$b_rsl : dirnam.nam$b_esl);
        */
        retlen = dirnam.nam$l_name - esa;
        esa[retlen] = '\0';
      }
      if (ts) retpath = safemalloc(retlen);
      else retpath = __pathify_retbuf;
      strcpy(retpath,esa);
    }

    return retpath;
}  /* end of do_pathify_dirspec() */
/*}}}*/
/* External entry points */
char *pathify_dirspec(dir) char *dir;
{ return do_pathify_dirspec(dir,0); }
char *pathify_dirspec_ts(dir) char *dir;
{ return do_pathify_dirspec(dir,1); }

/*{{{ char *tounixspec[_ts](char *path)*/
static char *do_tounixspec(char *spec, int ts)
{
  static char __tounixspec_retbuf[NAM$C_MAXRSS+1];
  char *dirend, *rslt, *cp1, *cp2, *cp3, tmp[NAM$C_MAXRSS+1];
  int devlen, dirlen;

  if (spec == NULL || *spec == '\0') return NULL;
  if (ts) rslt = safemalloc(NAM$C_MAXRSS+1);
  else rslt = __tounixspec_retbuf;
  if (strchr(spec,'/') != NULL) {
    strcpy(rslt,spec);
    return rslt;
  }

  cp1 = rslt;
  cp2 = spec;
  dirend = strrchr(spec,']');
  if (dirend == NULL) dirend = strrchr(spec,'>');
  if (dirend == NULL) dirend = strchr(spec,':');
  if (dirend == NULL) {
    strcpy(rslt,spec);
    return rslt;
  }
  if (*cp2 != '[') {
    *(cp1++) = '/';
  }
  else {  /* the VMS spec begins with directories */
    cp2++;
    if (*cp2 == '-') {
      while (*cp2 == '-') {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '/';
        cp2++;
      }
      if (*cp2 != '.' && *cp2 != ']' && *cp2 != '>') { /* we don't allow */
        if (ts) free(rslt);                            /* filespecs like */
        errno = EVMSERR; vaxc$errno = RMS$_SYN;        /* [--foo.bar] */
        return NULL;
      }
      cp2++;
    }
    else if ( *(cp2) != '.') { /* add the implied device into the Unix spec */
      *(cp1++) = '/';
      if (getcwd(tmp,sizeof tmp,1) == NULL) {
        if (ts) free(rslt);
        return NULL;
      }
      do {
        cp3 = tmp;
        while (*cp3 != ':' && *cp3) cp3++;
        *(cp3++) = '\0';
        if (strchr(cp3,']') != NULL) break;
      } while (((cp3 = getenv(tmp)) != NULL) && strcpy(tmp,cp3));
      cp3 = tmp;
      while (*cp3) *(cp1++) = *(cp3++);
      *(cp1++) = '/';
      if ((devlen = strlen(tmp)) + (dirlen = strlen(cp2)) + 1 > NAM$C_MAXRSS) {
        if (ts) free(rslt);
        errno = ERANGE;
        return NULL;
      }
    }
    else cp2++;
  }
  for (; cp2 <= dirend; cp2++) {
    if (*cp2 == ':') {
      *(cp1++) = '/';
      if (*(cp2+1) == '[') cp2++;
    }
    else if (*cp2 == ']' || *cp2 == '>') *(cp1++) = '/';
    else if (*cp2 == '.') {
      *(cp1++) = '/';
      while (*(cp2+1) == ']' || *(cp2+1) == '>' ||
             *(cp2+1) == '[' || *(cp2+1) == '<') cp2++;
    }
    else if (*cp2 == '-') {
      if (*(cp2-1) == '[' || *(cp2-1) == '<' || *(cp2-1) == '.') {
        while (*cp2 == '-') {
          cp2++;
          *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '/';
        }
        if (*cp2 != '.' && *cp2 != ']' && *cp2 != '>') { /* we don't allow */
          if (ts) free(rslt);                            /* filespecs like */
          errno = EVMSERR; vaxc$errno = RMS$_SYN;        /* [--foo.bar] */
          return NULL;
        }
        cp2++;
      }
      else *(cp1++) = *cp2;
    }
    else *(cp1++) = *cp2;
  }
  while (*cp2) *(cp1++) = *(cp2++);
  *cp1 = '\0';

  return rslt;

}  /* end of do_tounixspec() */
/*}}}*/
/* External entry points */
char *tounixspec(char *spec) { return do_tounixspec(spec,0); }
char *tounixspec_ts(char *spec) { return do_tounixspec(spec,1); }

/*{{{ char *tovmsspec[_ts](char *path)*/
static char *do_tovmsspec(char *path, int ts) {
  static char __tovmsspec_retbuf[NAM$C_MAXRSS+1];
  char *rslt, *dirend, *cp1, *cp2;

  if (path == NULL || *path == '\0') return NULL;
  if (ts) rslt = safemalloc(strlen(path)+1);
  else rslt = __tovmsspec_retbuf;
  if (strchr(path,']') != NULL || strchr(path,'>') != NULL ||
      (dirend = strrchr(path,'/')) == NULL) {
    strcpy(rslt,path);
    return rslt;
  }
  cp1 = rslt;
  cp2 = path;
  if (*cp2 == '/') {
    while (*(++cp2) != '/' && *cp2) *(cp1++) = *cp2;
    *(cp1++) = ':';
    *(cp1++) = '[';
    cp2++;
    }
  else {
    *(cp1++) = '[';
    *(cp1++) = '.';
  }
  for (; cp2 < dirend; cp2++) *(cp1++) = (*cp2 == '/') ? '.' : *cp2;
  *(cp1++) = ']';
  cp2++;
  while (*cp2) *(cp1++) = *(cp2++);
  *cp1 = '\0';

  return rslt;

}  /* end of do_tovmsspec() */
/*}}}*/
/* External entry points */
char *tovmsspec(char *path) { return do_tovmsspec(path,0); }
char *tovmsspec_ts(char *path) { return do_tovmsspec(path,1); }

/*{{{ char *tovmspath[_ts](char *path)*/
static char *do_tovmspath(char *path, int ts) {
  static char __tovmspath_retbuf[NAM$C_MAXRSS+1];
  char *pathified, *vmsified;

  if (path == NULL || *path == '\0') return NULL;
  if ((pathified = do_pathify_dirspec(path,1)) == NULL) return NULL;
  vmsified = do_tovmsspec(pathified,1);
  free(pathified);
  if (vmsified == NULL) return NULL;
  if (ts) return vmsified;
  else {
    strcpy(__tovmspath_retbuf,vmsified);
    free(vmsified);
    return __tovmspath_retbuf;
  }

}  /* end of do_tovmspath() */
/*}}}*/
/* External entry points */
char *tovmspath(char *path) { return do_tovmspath(path,0); }
char *tovmspath_ts(char *path) { return do_tovmspath(path,1); }


/*{{{ char *tounixpath[_ts](char *path)*/
static char *do_tounixpath(char *path, int ts) {
  static char __tounixpath_retbuf[NAM$C_MAXRSS+1];
  char *pathified, *unixified;

  if (path == NULL || *path == '\0') return NULL;
  if ((pathified = do_pathify_dirspec(path,1)) == NULL) return NULL;
  unixified = do_tounixspec(pathified,1);
  free(pathified);
  if (unixified == NULL) return NULL;
  if (ts) return unixified;
  else {
    strcpy(__tounixpath_retbuf,unixified);
    free(unixified);
    return __tounixpath_retbuf;
  }

}  /* end of do_tounixpath() */
/*}}}*/
/* External entry points */
char *tounixpath(char *path) { return do_tounixpath(path,0); }
char *tounixpath_ts(char *path) { return do_tounixpath(path,1); }

/*
 * @(#)argproc.c 2.2 94/08/16	Mark Pizzolato (mark@infocomm.com)
 *
 *****************************************************************************
 *                                                                           *
 *  Copyright (C) 1989-1994 by                                               *
 *  Mark Pizzolato - INFO COMM, Danville, California  (510) 837-5600         *
 *                                                                           *
 *  Permission is hereby  granted for the reproduction of this software,     *
 *  on condition that this copyright notice is included in the reproduction, *
 *  and that such reproduction is not for purposes of profit or material     *
 *  gain.                                                                    *
 *                                                                           *
 *  27-Aug-1994 Modified for inclusion in perl5                              *
 *              by Charles Bailey  bailey@genetics.upenn.edu                 *
 *****************************************************************************
 */

/*
 * getredirection() is intended to aid in porting C programs
 * to VMS (Vax-11 C).  The native VMS environment does not support 
 * '>' and '<' I/O redirection, or command line wild card expansion, 
 * or a command line pipe mechanism using the '|' AND background 
 * command execution '&'.  All of these capabilities are provided to any
 * C program which calls this procedure as the first thing in the 
 * main program.
 * The piping mechanism will probably work with almost any 'filter' type
 * of program.  With suitable modification, it may useful for other
 * portability problems as well.
 *
 * Author:  Mark Pizzolato	mark@infocomm.com
 */
struct list_item
    {
    struct list_item *next;
    char *value;
    };

static void add_item(struct list_item **head,
		     struct list_item **tail,
		     char *value,
		     int *count);

static void expand_wild_cards(char *item,
			      struct list_item **head,
			      struct list_item **tail,
			      int *count);

static int background_process(int argc, char **argv);

static void pipe_and_fork(char **cmargv);

/*{{{ void getredirection(int *ac, char ***av)*/
void
getredirection(int *ac, char ***av)
/*
 * Process vms redirection arg's.  Exit if any error is seen.
 * If getredirection() processes an argument, it is erased
 * from the vector.  getredirection() returns a new argc and argv value.
 * In the event that a background command is requested (by a trailing "&"),
 * this routine creates a background subprocess, and simply exits the program.
 *
 * Warning: do not try to simplify the code for vms.  The code
 * presupposes that getredirection() is called before any data is
 * read from stdin or written to stdout.
 *
 * Normal usage is as follows:
 *
 *	main(argc, argv)
 *	int		argc;
 *    	char		*argv[];
 *	{
 *		getredirection(&argc, &argv);
 *	}
 */
{
    int			argc = *ac;	/* Argument Count	  */
    char		**argv = *av;	/* Argument Vector	  */
    char		*ap;   		/* Argument pointer	  */
    int	       		j;		/* argv[] index		  */
    extern int		errno;		/* Last vms i/o error 	  */
    int			item_count = 0;	/* Count of Items in List */
    struct list_item 	*list_head = 0;	/* First Item in List	    */
    struct list_item	*list_tail;	/* Last Item in List	    */
    char 		*in = NULL;	/* Input File Name	    */
    char 		*out = NULL;	/* Output File Name	    */
    char 		*outmode = "w";	/* Mode to Open Output File */
    char 		*err = NULL;	/* Error File Name	    */
    char 		*errmode = "w";	/* Mode to Open Error File  */
    int			cmargc = 0;    	/* Piped Command Arg Count  */
    char		**cmargv = NULL;/* Piped Command Arg Vector */
    stat_t		statbuf;	/* fstat buffer		    */

    /*
     * First handle the case where the last thing on the line ends with
     * a '&'.  This indicates the desire for the command to be run in a
     * subprocess, so we satisfy that desire.
     */
    ap = argv[argc-1];
    if (0 == strcmp("&", ap))
	exit(background_process(--argc, argv));
    if ('&' == ap[strlen(ap)-1])
	{
	ap[strlen(ap)-1] = '\0';
	exit(background_process(argc, argv));
	}
    /*
     * Now we handle the general redirection cases that involve '>', '>>',
     * '<', and pipes '|'.
     */
    for (j = 0; j < argc; ++j)
	{
	if (0 == strcmp("<", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		errno = EINVAL;
		croak("No input file");
		}
	    in = argv[++j];
	    continue;
	    }
	if ('<' == *(ap = argv[j]))
	    {
	    in = 1 + ap;
	    continue;
	    }
	if (0 == strcmp(">", ap))
	    {
	    if (j+1 >= argc)
		{
		errno = EINVAL;
		croak("No input file");
		}
	    out = argv[++j];
	    continue;
	    }
	if ('>' == *ap)
	    {
	    if ('>' == ap[1])
		{
		outmode = "a";
		if ('\0' == ap[2])
		    out = argv[++j];
		else
		    out = 2 + ap;
		}
	    else
		out = 1 + ap;
	    if (j >= argc)
		{
		errno = EINVAL;
		croak("No output file");
		}
	    continue;
	    }
	if (('2' == *ap) && ('>' == ap[1]))
	    {
	    if ('>' == ap[2])
		{
		errmode = "a";
		if ('\0' == ap[3])
		    err = argv[++j];
		else
		    err = 3 + ap;
		}
	    else
		if ('\0' == ap[2])
		    err = argv[++j];
		else
		    err = 1 + ap;
	    if (j >= argc)
		{
		errno = EINVAL;
		croak("No error file");
		}
	    continue;
	    }
	if (0 == strcmp("|", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		errno = EPIPE;
		croak("No command into which to pipe");
		}
	    cmargc = argc-(j+1);
	    cmargv = &argv[j+1];
	    argc = j;
	    continue;
	    }
	if ('|' == *(ap = argv[j]))
	    {
	    ++argv[j];
	    cmargc = argc-j;
	    cmargv = &argv[j];
	    argc = j;
	    continue;
	    }
	expand_wild_cards(ap, &list_head, &list_tail, &item_count);
	}
    /*
     * Allocate and fill in the new argument vector, Some Unix's terminate
     * the list with an extra null pointer.
     */
    New(7002, argv, item_count+1, char *);
    *av = argv;
    for (j = 0; j < item_count; ++j, list_head = list_head->next)
	argv[j] = list_head->value;
    *ac = item_count;
    if (cmargv != NULL)
	{
	if (out != NULL)
	    {
	    errno = EINVAL;
	    croak("'|' and '>' may not both be specified on command line");
	    }
	pipe_and_fork(cmargv);
	}
	
    /* Check for input from a pipe (mailbox) */

    if (1 == isapipe(0))
	{
	char mbxname[L_tmpnam];
	int bufsize;
	int dvi_item = DVI$_DEVBUFSIZ;
	$DESCRIPTOR(mbxnam, "");
	$DESCRIPTOR(mbxdevnam, "");

	/* Input from a pipe, reopen it in binary mode to disable	*/
	/* carriage control processing.	 				*/

	if (in != NULL)
	    {
	    errno = EINVAL;
	    croak("'|' and '<' may not both be specified on command line");
	    }
	fgetname(stdin, mbxname);
	mbxnam.dsc$a_pointer = mbxname;
	mbxnam.dsc$w_length = strlen(mbxnam.dsc$a_pointer);	
	lib$getdvi(&dvi_item, 0, &mbxnam, &bufsize, 0, 0);
	mbxdevnam.dsc$a_pointer = mbxname;
	mbxdevnam.dsc$w_length = sizeof(mbxname);
	dvi_item = DVI$_DEVNAM;
	lib$getdvi(&dvi_item, 0, &mbxnam, 0, &mbxdevnam, &mbxdevnam.dsc$w_length);
	mbxdevnam.dsc$a_pointer[mbxdevnam.dsc$w_length] = '\0';
	errno = 0;
	freopen(mbxname, "rb", stdin);
	if (errno != 0)
	    {
	    croak("Error reopening pipe (name: %s) in binary mode",mbxname);
	    }
	}
    if ((in != NULL) && (NULL == freopen(in, "r", stdin, "mbc=32", "mbf=2")))
	{
	croak("Can't open input file %s",in);
	}
    if ((out != NULL) && (NULL == freopen(out, outmode, stdout, "mbc=32", "mbf=2")))
	{	
	croak("Can't open output file %s",out);
	}
    if ((err != NULL) && (NULL == freopen(err, errmode, stderr, "mbc=32", "mbf=2")))
	{	
	croak("Can't open error file %s",err);
	}
#ifdef ARGPROC_DEBUG
    fprintf(stderr, "Arglist:\n");
    for (j = 0; j < *ac;  ++j)
	fprintf(stderr, "argv[%d] = '%s'\n", j, argv[j]);
#endif
}  /* end of getredirection() */
/*}}}*/

static void add_item(struct list_item **head,
		     struct list_item **tail,
		     char *value,
		     int *count)
{
    if (*head == 0)
	{
	New(7003,*head,1,struct list_item);
	*tail = *head;
	}
    else {
	New(7004,(*tail)->next,1,struct list_item);
	*tail = (*tail)->next;
	}
    (*tail)->value = value;
    ++(*count);
}

static void expand_wild_cards(char *item,
			      struct list_item **head,
			      struct list_item **tail,
			      int *count)
{
int expcount = 0;
int context = 0;
int status;
int status_value;
char *had_version;
char *had_device;
int had_directory;
char *devdir;
$DESCRIPTOR(filespec, "");
$DESCRIPTOR(defaultspec, "SYS$DISK:[]*.*;");
$DESCRIPTOR(resultspec, "");

    if (strcspn(item, "*%") == strlen(item))
	{
	add_item(head, tail, item, count);
	return;
	}
    resultspec.dsc$b_dtype = DSC$K_DTYPE_T;
    resultspec.dsc$b_class = DSC$K_CLASS_D;
    resultspec.dsc$a_pointer = NULL;
    filespec.dsc$a_pointer = item;
    filespec.dsc$w_length = strlen(filespec.dsc$a_pointer);
    /*
     * Only return version specs, if the caller specified a version
     */
    had_version = strchr(item, ';');
    /*
     * Only return device and directory specs, if the caller specifed either.
     */
    had_device = strchr(item, ':');
    had_directory = (NULL != strchr(item, '[')) || (NULL != strchr(item, '<'));
    while (1 == (1&lib$find_file(&filespec, &resultspec, &context,
    				 &defaultspec, 0, &status_value, &0)))
	{
	char *string;
	char *c;

	Newc(7005,string,1,resultspec.dsc$w_length+1,char *);
	strncpy(string, resultspec.dsc$a_pointer, resultspec.dsc$w_length);
	string[resultspec.dsc$w_length] = '\0';
	if (NULL == had_version)
	    *((char *)strrchr(string, ';')) = '\0';
	if ((!had_directory) && (had_device == NULL))
	    {
	    if (NULL == (devdir = strrchr(string, ']')))
		devdir = strrchr(string, '>');
	    strcpy(string, devdir + 1);
	    }
	/*
	 * Be consistent with what the C RTL has already done to the rest of
	 * the argv items and lowercase all of these names.
	 */
	for (c = string; *c; ++c)
	    if (isupper(*c))
		*c = tolower(*c);
	add_item(head, tail, string, count);
	++expcount;
	}
    if (expcount == 0)
	add_item(head, tail, item, count);
    lib$sfree1_dd(&resultspec);
    lib$find_file_end(&context);
}

static int child_st[2];/* Event Flag set when child process completes	*/

static short child_chan;/* I/O Channel for Pipe Mailbox		*/

static exit_handler(int *status)
{
short iosb[4];

    if (0 == child_st[0])
	{
#ifdef ARGPROC_DEBUG
	fprintf(stderr, "Waiting for Child Process to Finish . . .\n");
#endif
	fflush(stdout);	    /* Have to flush pipe for binary data to	*/
			    /* terminate properly -- <tp@mccall.com>	*/
	sys$qiow(0, child_chan, IO$_WRITEOF, iosb, 0, 0, 0, 0, 0, 0, 0, 0);
	sys$dassgn(child_chan);
	fclose(stdout);
	sys$synch(0, child_st);
	}
    return(1);
}

#include syidef		/* System Information Definitions	*/

static void sig_child(int chan)
{
#ifdef ARGPROC_DEBUG
    fprintf(stderr, "Child Completion AST\n");
#endif
    if (child_st[0] == 0)
	child_st[0] = 1;
}

static struct exit_control_block
    {
    struct exit_control_block *flink;
    int	(*exit_routine)();
    int arg_count;
    int *status_address;
    int exit_status;
    } exit_block =
    {
    0,
    exit_handler,
    1,
    &exit_block.exit_status,
    0
    };

static void pipe_and_fork(char **cmargv)
{
    char subcmd[2048];
    $DESCRIPTOR(cmddsc, "");
    static char mbxname[64];
    $DESCRIPTOR(mbxdsc, mbxname);
    short iosb[4];
    int status;
    int pid, j;
    short dvi_item = DVI$_DEVNAM;
    int mbxsize;
    short syi_item = SYI$_MAXBUF;

    strcpy(subcmd, cmargv[0]);
    for (j = 1; NULL != cmargv[j]; ++j)
	{
	strcat(subcmd, " \"");
	strcat(subcmd, cmargv[j]);
	strcat(subcmd, "\"");
	}
    cmddsc.dsc$a_pointer = subcmd;
    cmddsc.dsc$w_length = strlen(cmddsc.dsc$a_pointer);
    /*
     * Get the SYSGEN parameter MAXBUF, and the smaller of it and BUFSIZ as
     * the size of the 'pipe' mailbox.
     */
    vaxc$errno = lib$getsyi(&syi_item, &mbxsize, 0, 0, 0, 0);
    if (0 == (1&vaxc$errno))
	{
 	errno = EVMSERR;
	croak("Can't get SYSGEN parameter value for MAXBUF");
	}
    if (mbxsize > BUFSIZ)
	mbxsize = BUFSIZ;
    if (0 == (1&(vaxc$errno = sys$crembx(0, &child_chan, mbxsize, mbxsize, 0, 0, 0))))
	{
	errno = EVMSERR;
	croak("Can't create pipe mailbox");
	}
    vaxc$errno = lib$getdvi(&dvi_item, &child_chan, NULL, NULL, &mbxdsc, &mbxdsc);
    if (0 == (1&vaxc$errno))
	{
	errno = EVMSERR;
	croak("Can't get pipe mailbox device name");
	}
    mbxname[mbxdsc.dsc$w_length] = '\0';
#ifdef ARGPROC_DEBUG
    fprintf(stderr, "Pipe Mailbox Name = '%s'\n", mbxdsc.dsc$a_pointer);
    fprintf(stderr, "Sub Process Command = '%s'\n", cmddsc.dsc$a_pointer);
#endif
    if (0 == (1&(vaxc$errno = lib$spawn(&cmddsc, &mbxdsc, 0, &1,
    					0, &pid, child_st, &0, sig_child,
    					&child_chan))))
	{
	errno = EVMSERR;
	croak("Can't spawn subprocess");
	}
#ifdef ARGPROC_DEBUG
    fprintf(stderr, "Subprocess's Pid = %08X\n", pid);
#endif
    sys$dclexh(&exit_block);
    if (NULL == freopen(mbxname, "wb", stdout))
	{
	croak("Can't open pipe mailbox for output");
	}
}

static int background_process(int argc, char **argv)
{
char command[2048] = "$";
$DESCRIPTOR(value, "");
static $DESCRIPTOR(cmd, "BACKGROUND$COMMAND");
static $DESCRIPTOR(null, "NLA0:");
static $DESCRIPTOR(pidsymbol, "SHELL_BACKGROUND_PID");
char pidstring[80];
$DESCRIPTOR(pidstr, "");
int pid;

    strcat(command, argv[0]);
    while (--argc)
	{
	strcat(command, " \"");
	strcat(command, *(++argv));
	strcat(command, "\"");
	}
    value.dsc$a_pointer = command;
    value.dsc$w_length = strlen(value.dsc$a_pointer);
    if (0 == (1&(vaxc$errno = lib$set_symbol(&cmd, &value))))
	{
	errno = EVMSERR;
	croak("Can't create symbol for subprocess command");
	}
    if ((0 == (1&(vaxc$errno = lib$spawn(&cmd, &null, 0, &17, 0, &pid)))) &&
	(vaxc$errno != 0x38250))
	{
	errno = EVMSERR;
	croak("Can't spawn subprocess");
	}
    if (vaxc$errno == 0x38250) /* We must be BATCH, so retry */
	if (0 == (1&(vaxc$errno = lib$spawn(&cmd, &null, 0, &1, 0, &pid))))
	    {
	    errno = EVMSERR;
	    croak("Can't spawn subprocess");
	    }
#ifdef ARGPROC_DEBUG
    fprintf(stderr, "%s\n", command);
#endif
    sprintf(pidstring, "%08X", pid);
    fprintf(stderr, "%s\n", pidstring);
    pidstr.dsc$a_pointer = pidstring;
    pidstr.dsc$w_length = strlen(pidstr.dsc$a_pointer);
    lib$set_symbol(&pidsymbol, &pidstr);
    return(SS$_NORMAL);
}
/*}}}*/
/***** End of code taken from Mark Pizzolato's argproc.c package *****/

/*
 * flex_stat, flex_fstat
 * basic stat, but gets it right when asked to stat
 * a Unix-style path ending in a directory name (e.g. dir1/dir2/dir3)
 */

/*{{{ int flex_fstat(int fd, struct stat *statbuf)*/
int
flex_fstat(int fd, struct stat *statbuf)
{
  char fspec[NAM$C_MAXRSS+1];

  if (!getname(fd,fspec)) return -1;
  return flex_stat(fspec,statbuf);

}  /* end of flex_fstat() */
    char *fileified;
/*}}}*/

/*{{{ int flex_stat(char *fspec, struct stat *statbuf)*/
flex_stat(char *fspec, struct stat *statbuf)
{
    int retval,myretval;
    struct stat tmpbuf;

    fileified = fileify_dirspec_ts(fspec);
    if (!fileified) myretval = -1;
    else {
      myretval = stat(fileify_dirspec(fspec),&tmpbuf);
      safefree(fileified);
    }
    retval = stat(fspec,statbuf);
    if (!myretval) {
      if (retval == -1) {
        *statbuf = tmpbuf;
        retval = 0;
      }
      else if (!retval) {
        statbuf->st_mode &= ~S_IFDIR;
        statbuf->st_mode |= tmpbuf.st_mode & S_IFDIR;
      }
    }
    return retval;

}  /* end of flex_stat() */
/*}}}*/

/*{{{ unsigned long int tmp_longflip(unsigned long int val)*/
unsigned long int
tmp_longflip(unsigned long int val)
{
    unsigned long int scratch = val;
    unsigned char savbyte, *tmp;

    tmp = (unsigned char *) &scratch;
    savbyte = tmp[0]; tmp[0] = tmp[3]; tmp[3] = savbyte;
    savbyte = tmp[1]; tmp[1] = tmp[2]; tmp[2] = savbyte;

    return scratch;
}
/*}}}*/
