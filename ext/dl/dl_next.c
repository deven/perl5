/* dl_next.c
   Author:  tom@smart.bo.open.de (Thomas Neumann).
   Based on dl_sunos.c
   Changes:
   June 30 1994
   Anno Siegel (siegel@zrz.TU-Berlin.DE) with help from
   Andreas Koenig (k@franz.ww.TU-Berlin.DE)
   Modified to let DynamicLoader::bootstrap accept any number of parameters.
   The first one (required) is the package name, the other ones (optional)
   are full qualified pathnames of libraries to load along with the package.
*/
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <mach-o/rld.h>
#include <streams/streams.h>
#include <sys/file.h>

static int
XS_DynamicLoader_bootstrap(ix, ax, items)
register int ix;
register int ax;
register int items;
{
    if (items < 1) {
	croak("Usage: DynamicLoader::bootstrap(package [, lib]...)");
    }
    {
	char*	package = SvPV(ST(1),na);
	int rld_success;
	NXStream *nxerr = NXOpenFile(fileno(stderr), NX_WRITEONLY);
	int (*bootproc)();
	char tmpbuf[1024];
	char tmpbuf2[128];
	AV *av = GvAVn(incgv);
	I32 i;
	char **p;

	p = (char **) safemalloc((items + 1) * sizeof( char *));
	p[0] = tmpbuf;
	for(i=1; i<items; i++) {
	    p[i] = SvPV(ST(i+1),na);
	}
	p[items] = 0;

	for (i = 0; i <= AvFILL(av); i++) {
	    sprintf(tmpbuf, "%s/auto/%s/%s.so",
		    SvPVx(*av_fetch(av, i, TRUE), na), package, package);
	    if ( access( tmpbuf, R_OK) != 0 )
	    {
		continue;
	    }
	    if (rld_success = rld_load(nxerr, (struct mach_header **)0, p,
				       (const char *)0))
	    {
	        break;
	    }
	}
	safefree((char *) p);
	if (!rld_success) {
	    NXClose(nxerr);
	    croak("Can't find loadable object for package %s in @INC", package);

	}
	sprintf(tmpbuf2, "_boot_%s", package);
	if (!rld_lookup(nxerr, tmpbuf2, (unsigned long *)&bootproc)) {
	    NXClose(nxerr);
	    croak("Shared object %s contains no %s function", tmpbuf, tmpbuf2);
	}
	NXClose(nxerr);
	(*bootproc)();
	ST(0) = sv_mortalcopy(&sv_yes);
    }
    return ax;
}

int
boot_DynamicLoader(ix,sp,items)
int ix;
int sp;
int items;
{
    char* file = __FILE__;

    newXSUB("DynamicLoader::bootstrap", 0, XS_DynamicLoader_bootstrap, file);
}
