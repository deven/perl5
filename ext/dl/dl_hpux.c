/*
 * Author: Jeff Okamoto (okamoto@corp.hp.com)
 */

#ifdef __hp9000s300
#define magic hpux_magic
#define MAGIC HPUX_MAGIC
#endif
#include <dl.h>
#ifdef __hp9000s300
#undef magic
#undef MAGIC
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static int
XS_DynamicLoader_bootstrap(ix, ax, items)
register int ix;
register int ax;
register int items;
{
    if (items < 1 || items > 1) {
	croak("Usage: DynamicLoader::bootstrap(package)");
    }
    {
	char*	package = SvPV(ST(1),na);
	shl_t obj = NULL;
	int (*bootproc)();
	char tmpbuf[1024];
	char tmpbuf2[128];
	char tmpbuf3[128];
	AV *av = GvAVn(incgv);
	I32 i;

	for (i = 0; i <= AvFILL(av); i++) {
	    (void)sprintf(tmpbuf, "%s/auto/%s/%s.sl",
		SvPVx(*av_fetch(av, i, TRUE), na), package, package);
	    if (obj = shl_load(tmpbuf,
		BIND_IMMEDIATE | BIND_NONFATAL | BIND_NOSTART | BIND_VERBOSE, 0L))
		break;
	}
	if (obj == (shl_t) NULL) {
	    sprintf(tmpbuf2, "%s", Strerror(errno));
	    croak("Can't find loadable object for package %s in @INC (library %s, %s)",
		package, tmpbuf, tmpbuf2);
	}

#ifdef __hp9000s300
	sprintf(tmpbuf2, "_boot_%s", package);
#else
	sprintf(tmpbuf2, "boot_%s", package);
#endif
	i = shl_findsym(&obj, tmpbuf2, TYPE_PROCEDURE, &bootproc);
	if (i == -1) {
	    if (errno == 0) {
		croak("Shared object %s contains no %s function", tmpbuf, tmpbuf2);
	    } else {
		sprintf(tmpbuf3, "%s", Strerror(errno));
		croak("Shared object %s contains no %s function (%s)", tmpbuf, tmpbuf2, tmpbuf3);
	    }
	}
	bootproc();

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
