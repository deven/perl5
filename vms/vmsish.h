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
#define HAS_ALARM
#define HAS_KILL
#define HAS_WAIT

/* HAS_IOCTL:
 *	This symbol, if defined, indicates that the ioctl() routine is
 *	available to set I/O characteristics
 */
#undef	HAS_IOCTL		/**/
 
/* HAS_UTIME:
 *	This symbol, if defined, indicates that the routine utime() is
 *	available to update the access and modification times of files.
 */
#undef HAS_UTIME		/**/

/* I_SYS_PARAM:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/param.h>.
 */
#undef I_SYS_PARAM 		/**/
  
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

/* Allow us to make use of the -D switch to perl - eventually this should
   be moved out to Descrip.MMS */
#define DEBUGGING 1
