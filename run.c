#include "EXTERN.h"
#include "perl.h"

/*
 * "Until it joins some larger way/Where many paths and errands meet."  --Bilbo
 */

char **watchaddr = 0;
char *watchok;

#ifndef DEBUGGING

int
run() {
    SAVEI32(runlevel);
    runlevel++;

    while ( op = (*op->op_ppaddr)() ) ;
    return 0;
}

#else

int
run() {
    if (!op) {
	warn("NULL OP IN RUN");
	return 0;
    }

    SAVEI32(runlevel);
    runlevel++;

    do {
	if (debug) {
	    if (watchaddr != 0 && *watchaddr != watchok)
		fprintf(stderr, "WARNING: %lx changed from %lx to %lx\n",
		    (long)watchaddr, (long)watchok, (long)*watchaddr);
	    DEBUG_s(debstack());
	    DEBUG_t(debop(op));
	}
    } while ( op = (*op->op_ppaddr)() );
    return 0;
}

#endif

I32
debop(op)
OP *op;
{
    SV *sv;
    deb("%s", op_name[op->op_type]);
    switch (op->op_type) {
    case OP_CONST:
	fprintf(stderr, "(%s)", SvPEEK(cSVOP->op_sv));
	break;
    case OP_GVSV:
    case OP_GV:
	if (cGVOP->op_gv) {
	    sv = NEWSV(0,0);
	    gv_fullname(sv, cGVOP->op_gv);
	    fprintf(stderr, "(%s)", SvPV(sv, na));
	    SvREFCNT_dec(sv);
	}
	else
	    fprintf(stderr, "(NULL)");
	break;
    default:
	break;
    }
    fprintf(stderr, "\n");
    return 0;
}

void
watch(addr)
char **addr;
{
    watchaddr = addr;
    watchok = *addr;
    fprintf(stderr, "WATCHING, %lx is currently %lx\n",
	(long)watchaddr, (long)watchok);
}
