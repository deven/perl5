/* Assorted things to look like Unix */
#ifdef __GNUC__
#ifndef _IOLBF /* gcc's stdio.h doesn't define this */
#define _IOLBF 1
#else
#include <unixio.h>
#include <unixlib.h>
#endif
#endif
#include <file.h>  /* it's not <sys/file.h>, so don't use I_SYS_FILE */
#define unlink remove

/*
 * The following symbols are defined (or undefined) according to the RTL
 * support VMS provides for the corresponding functions.  These don't
 * appear in config.h, so they're dealt with here.
 */
#define HAS_KILL
#define HAS_WAIT

/*  The VMS C RTL has vfork() but not fork().  Both actually work in a way
 *  that's somewhere between Unix vfork() and VMS lib$spawn(), so it's
 *  probably not a good idea to use them much.  That said, we'll try to
 *  use vfork() in either case.
 */
#define fork vfork

/* Assorted fiddling with sigs . . . */
# include <signal.h>
#define ABORT() abort()

/* There's a struct in sv.h which includes a DIR *, so . . . */
#ifndef I_DIRENT
#define DIR void
#endif

/* This is what times() returns, but <times.h> calls it tbuffer_t on VMS */

struct tms {
  clock_t tms_utime;    /* user time */
  clock_t tms_stime;    /* system time - always 0 on VMS */
  clock_t tms_cutime;   /* user time, children */
  clock_t tms_cstime;   /* system time, children - always 0 on VMS */
};

/* VMS doesn't use a real sys_nerr, but we need this when scanning for error
 * messages in text strings . . .
 */

#define sys_nerr EVMSERR  /* EVMSERR is as high as we can go. */

/* Look up new %ENV values on the fly */
#define DYNAMIC_ENV_FETCH 1
#define ENV_HV_NAME "%EnV%VmS%"

/* Use our own stat() clones, which handle Unix-style directory names */
/* #define stat(name,bufptr) flex_stat(name,bufptr) */
#define fstat(name,bufptr) flex_fstat(name,bufptr)

/* Prototypes for functions unique to vms.c.  Don't include replacements
 * for routines in the mainline source files excluded by #ifndef VMS;
 * their prototypes are already in proto.h.
 */
#ifndef HAS_WAITPID  /* Not a real waitpid - use only with popen from vms.c! */
unsigned long int waitpid _((unsigned long int, int *, int));
#endif
char *my_gconvert _((double, int, int, char *));
char *fileify_dirspec _((char *));
char *fileify_dirspec_ts _((char *));
char *pathify_dirspec _((char *));
char *pathify_dirspec_ts _((char *));
char *tounixspec _((char *));
char *tounixspec_ts _((char *));
char *tovmsspec _((char *));
char *tovmsspec_ts _((char *));
char *tounixpath _((char *));
char *tounixpath_ts _((char *));
char *tovmspath _((char *));
char *tovmspath_ts _((char *));
void getredirection _((int *, char ***));
int flex_fstat _((int, stat_t *));
int flex_stat _((char *, stat_t *));
/* The following two routines are temporary.
 * (See below for more information.) */
unsigned short int tmp_shortflip _((unsigned short int));
unsigned long int tmp_longflip _((unsigned long int));


/***** The following four #defines are temporary, and should be removed,
 * along with the corresponding routines in vms.c, when TCP/IP support
 * is integrated into the VMS port of perl5. (The temporary hacks are
 * here for now so pack can handle type N elements.)
 * - C. Bailey  26-Aug-1994
 *****/
#define htons(us) tmp_shortflip(us)
#define ntohs(us) tmp_shortflip(us)
#define htonl(ul) tmp_longflip(ul)
#define ntohl(ul) tmp_longflip(ul)

/* Allow us to make use of the -D switch to perl - eventually this should
   be moved out to Descrip.MMS */
#define DEBUGGING 1
