#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <sys/wait.h>

static int
not_here(s)
char *s;
{
    croak("Wait::%s not implemented on this architecture", s);
    return -1;
}

double constant(name, arg)
char *name;
int arg;
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	break;
    case 'C':
	break;
    case 'D':
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	if (strEQ(name, "WNOHANG"))
#ifdef WNOHANG
	    return WNOHANG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "WSTOPPED"))
#ifdef WSTOPPED
	    return WSTOPPED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "WUNTRACED"))
#ifdef WUNTRACED
	    return WUNTRACED;
#else
	    goto not_there;
#endif
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    case 'a':
	break;
    case 'b':
	break;
    case 'c':
	break;
    case 'd':
	break;
    case 'e':
	break;
    case 'f':
	break;
    case 'g':
	break;
    case 'h':
	break;
    case 'i':
	break;
    case 'j':
	break;
    case 'k':
	break;
    case 'l':
	break;
    case 'm':
	break;
    case 'n':
	break;
    case 'o':
	break;
    case 'p':
	break;
    case 'q':
	break;
    case 'r':
	break;
    case 's':
	break;
    case 't':
	break;
    case 'u':
	break;
    case 'v':
	break;
    case 'w':
	break;
    case 'x':
	break;
    case 'y':
	break;
    case 'z':
	break;
    case '_':
	if (strEQ(name, "_WSTOPPED"))
#ifdef _WSTOPPED
	    return _WSTOPPED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "__w_coredump"))
#ifdef __w_coredump
	    return __w_coredump;
#else
	    goto not_there;
#endif
	if (strEQ(name, "__w_retcode"))
#ifdef __w_retcode
	    return __w_retcode;
#else
	    goto not_there;
#endif
	if (strEQ(name, "__w_stopsig"))
#ifdef __w_stopsig
	    return __w_stopsig;
#else
	    goto not_there;
#endif
	if (strEQ(name, "__w_stopval"))
#ifdef __w_stopval
	    return __w_stopval;
#else
	    goto not_there;
#endif
	if (strEQ(name, "__w_termsig"))
#ifdef __w_termsig
	    return __w_termsig;
#else
	    goto not_there;
#endif
	if (strEQ(name, "__wait"))
#ifdef __wait
	    return __wait;
#else
	    goto not_there;
#endif
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

MODULE = Wait		PACKAGE = Wait

double
constant(name,arg)
	char *		name
	int		arg

