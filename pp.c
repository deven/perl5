/* $RCSfile: pp.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:03 $
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	pp.c, v $
 */

/*
 * "It's a big house this, and very peculiar.  Always a bit more to discover,
 * and no knowing what you'll find around a corner.  And Elves, sir!" --Samwise
 */

#include "EXTERN.h"
#include "perl.h"

#ifndef WORD_ALIGN
#define WORD_ALIGN sizeof(U16)
#endif

/* Omit this -- it causes too much grief on mixed systems.
#ifdef I_UNISTD
#include <unistd.h>
#endif
*/

#ifdef HAS_SOCKET
# include <sys/socket.h>
# include <netdb.h>
# ifndef ENOTSOCK
#  ifdef I_NET_ERRNO
#   include <net/errno.h>
#  endif
# endif
#endif

#ifdef HAS_SELECT
#ifdef I_SYS_SELECT
#ifndef I_SYS_TIME
#include <sys/select.h>
#endif
#endif
#endif

#ifdef HOST_NOT_FOUND
extern int h_errno;
#endif

#ifdef HAS_PASSWD
# ifdef I_PWD
#  include <pwd.h>
# else
    struct passwd *getpwnam _((char *));
    struct passwd *getpwuid _((Uid_t));
# endif
  struct passwd *getpwent _((void));
#endif

#ifdef HAS_GROUP
# ifdef I_GRP
#  include <grp.h>
# else
    struct group *getgrnam _((char *));
    struct group *getgrgid _((Gid_t));
# endif
    struct group *getgrent _((void));
#endif

#ifdef I_UTIME
#include <utime.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

#ifdef HAS_GETPGRP2
#   define getpgrp getpgrp2
#endif

#ifdef HAS_SETPGRP2
#   define setpgrp setpgrp2
#endif

#ifdef HAS_GETPGRP2
#   define getpgrp getpgrp2
#endif

#ifdef HAS_SETPGRP2
#   define setpgrp setpgrp2
#endif

#ifdef HAS_GETPGRP2
#   define getpgrp getpgrp2
#endif

#ifdef HAS_SETPGRP2
#   define setpgrp setpgrp2
#endif

static void doencodes _((SV *sv, char *s, I32 len));
static OP *doeval _((void));
static OP *dofindlabel _((OP *op, char *label, OP **opstack));
static OP *doform _((CV *cv, GV *gv, OP *retop));
#if !defined(HAS_MKDIR) || !defined(HAS_RMDIR)
static int dooneliner _((char *cmd, char *filename));
#endif
static void doparseform _((SV *sv));
static I32 dopoptoeval _((I32 startingblock));
static I32 dopoptolabel _((char *label));
static I32 dopoptoloop _((I32 startingblock));
static I32 dopoptosub _((I32 startingblock));
static void save_lines _((AV *array, SV *sv));
static int sortcmp _((const void *, const void *));
static int sortcv _((const void *, const void *));

/* Nothing. */

PP(pp_null)
{
    return NORMAL;
}

PP(pp_stub)
{
    dSP;
    if (GIMME != G_ARRAY) {
	XPUSHs(&sv_undef);
    }
    RETURN;
}

PP(pp_scalar)
{
    return NORMAL;
}

/* Pushy stuff. */

PP(pp_pushmark)
{
    PUSHMARK(stack_sp);
    return NORMAL;
}

PP(pp_wantarray)
{
    dSP;
    I32 cxix;
    EXTEND(SP, 1);

    cxix = dopoptosub(cxstack_ix);
    if (cxix < 0)
	RETPUSHUNDEF;

    if (cxstack[cxix].blk_gimme == G_ARRAY)
	RETPUSHYES;
    else
	RETPUSHNO;
}

PP(pp_const)
{
    dSP;
    XPUSHs(cSVOP->op_sv);
    RETURN;
}

PP(pp_gvsv)
{
    dSP;
    EXTEND(sp,1);
    if (op->op_private & OPpLVAL_INTRO)
	PUSHs(save_scalar(cGVOP->op_gv));
    else
	PUSHs(GvSV(cGVOP->op_gv));
    RETURN;
}

PP(pp_gv)
{
    dSP;
    XPUSHs((SV*)cGVOP->op_gv);
    RETURN;
}

PP(pp_padsv)
{
    dSP; dTARGET;
    XPUSHs(TARG);
    if (op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(curpad[op->op_targ]);
    RETURN;
}

PP(pp_padav)
{
    dSP; dTARGET;
    if (op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(curpad[op->op_targ]);
    EXTEND(SP, 1);
    if (op->op_flags & OPf_REF) {
	PUSHs(TARG);
	RETURN;
    }
    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	EXTEND(SP, maxarg);
	Copy(AvARRAY((AV*)TARG), SP+1, maxarg, SV*);
	SP += maxarg;
    }
    else {
	SV* sv = sv_newmortal();
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	sv_setiv(sv, maxarg);
	PUSHs(sv);
    }
    RETURN;
}

PP(pp_padhv)
{
    dSP; dTARGET;
    XPUSHs(TARG);
    if (op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(curpad[op->op_targ]);
    if (op->op_flags & OPf_REF)
	RETURN;
    if (GIMME == G_ARRAY) { /* array wanted */
	RETURNOP(do_kv(ARGS));
    }
    else {
	SV* sv = sv_newmortal();
	if (HvFILL((HV*)TARG)) {
	    sprintf(buf, "%d/%d", HvFILL((HV*)TARG), HvMAX((HV*)TARG)+1);
	    sv_setpv(sv, buf);
	}
	else
	    sv_setiv(sv, 0);
	SETs(sv);
	RETURN;
    }
}

PP(pp_padany)
{
    DIE("NOT IMPL LINE %d",__LINE__);
}

PP(pp_pushre)
{
    dSP;
    XPUSHs((SV*)op);
    RETURN;
}

/* Translations. */

PP(pp_rv2gv)
{
    dSP; dTOPss;
    
    if (SvROK(sv)) {
      wasref:
	sv = SvRV(sv);
	if (SvTYPE(sv) != SVt_PVGV)
	    DIE("Not a GLOB reference");
    }
    else {
	if (SvTYPE(sv) != SVt_PVGV) {
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv)) {
		if (op->op_flags & OPf_REF ||
		    op->op_private & HINT_STRICT_REFS)
		    DIE(no_usym, "a symbol");
		RETSETUNDEF;
	    }
	    if (op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, "a symbol");
	    sv = (SV*)gv_fetchpv(SvPV(sv, na), TRUE, SVt_PVGV);
	}
    }
    if (op->op_private & OPpLVAL_INTRO) {
	GP *ogp = GvGP(sv);

	SSCHECK(3);
	SSPUSHPTR(sv);
	SSPUSHPTR(ogp);
	SSPUSHINT(SAVEt_GP);

	if (op->op_flags & OPf_SPECIAL) {
	    GvGP(sv)->gp_refcnt++;		/* will soon be assigned */
	    GvFLAGS(sv) |= GVf_INTRO;
	}
	else {
	    GP *gp;
	    Newz(602,gp, 1, GP);
	    GvGP(sv) = gp;
	    GvREFCNT(sv) = 1;
	    GvSV(sv) = NEWSV(72,0);
	    GvLINE(sv) = curcop->cop_line;
	    GvEGV(sv) = sv;
	}
    }
    SETs(sv);
    RETURN;
}

PP(pp_sv2len)
{
    dSP; dTARGET;
    dPOPss;
    PUSHi(sv_len(sv));
    RETURN;
}

PP(pp_rv2sv)
{
    dSP; dTOPss;

    if (SvROK(sv)) {
      wasref:
	sv = SvRV(sv);
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	case SVt_PVHV:
	case SVt_PVCV:
	    DIE("Not a SCALAR reference");
	}
    }
    else {
	GV *gv = sv;
	if (SvTYPE(gv) != SVt_PVGV) {
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv)) {
		if (op->op_flags & OPf_REF ||
		    op->op_private & HINT_STRICT_REFS)
		    DIE(no_usym, "a SCALAR");
		RETSETUNDEF;
	    }
	    if (op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, "a SCALAR");
	    gv = (SV*)gv_fetchpv(SvPV(sv, na), TRUE, SVt_PV);
	}
	sv = GvSV(gv);
    }
    if (op->op_flags & OPf_MOD) {
	if (op->op_private & OPpLVAL_INTRO)
	    sv = save_scalar((GV*)TOPs);
	else if (op->op_private & (OPpDEREF_HV|OPpDEREF_AV)) {
	    if (SvGMAGICAL(sv))
		mg_get(sv);
	    if (!SvOK(sv)) {
		SvUPGRADE(sv, SVt_RV);
		SvRV(sv) = (op->op_private & OPpDEREF_HV ?
			    (SV*)newHV() : (SV*)newAV());
		SvROK_on(sv);
		SvSETMAGIC(sv);
	    }
	}
    }
    SETs(sv);
    RETURN;
}

PP(pp_av2arylen)
{
    dSP;
    AV *av = (AV*)TOPs;
    SV *sv = AvARYLEN(av);
    if (!sv) {
	AvARYLEN(av) = sv = NEWSV(0,0);
	sv_upgrade(sv, SVt_IV);
	sv_magic(sv, (SV*)av, '#', Nullch, 0);
    }
    SETs(sv);
    RETURN;
}

PP(pp_pos)
{
    dSP; dTARGET; dPOPss;
    
    if (op->op_flags & OPf_MOD) {
	LvTYPE(TARG) = '<';
	LvTARG(TARG) = sv;
	PUSHs(TARG);	/* no SvSETMAGIC */
	RETURN;
    }
    else {
	STRLEN pos = 0;
	MAGIC* mg; 
	if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	    mg = mg_find(sv, 'g');
	    if (mg && mg->mg_len >= 0) {
		PUSHi(mg->mg_len + curcop->cop_arybase);
		RETURN;
	    }
	}
	RETPUSHUNDEF;
    }
}

PP(pp_rv2cv)
{
    dSP;
    GV *gv;
    HV *stash;

    /* We always try to add a non-existent subroutine in case of AUTOLOAD. */
    CV *cv = sv_2cv(TOPs, &stash, &gv, TRUE);

    SETs((SV*)cv);
    RETURN;
}

PP(pp_anoncode)
{
    dSP;
    XPUSHs(cSVOP->op_sv);
    RETURN;
}

PP(pp_srefgen)
{
    dSP; dTOPss;
    SV* rv;
    rv = sv_newmortal();
    sv_upgrade(rv, SVt_RV);
    if (SvPADTMP(sv))
	sv = newSVsv(sv);
    else {
	SvTEMP_off(sv);
	SvREFCNT_inc(sv);
    }
    SvRV(rv) = sv;
    SvROK_on(rv);
    SETs(rv);
    RETURN;
} 

PP(pp_refgen)
{
    dSP; dMARK;
    SV* sv;
    SV* rv;
    if (GIMME != G_ARRAY) {
	MARK[1] = *SP;
	SP = MARK + 1;
    }
    while (MARK < SP) {
	sv = *++MARK;
	rv = sv_newmortal();
	sv_upgrade(rv, SVt_RV);
	if (SvPADTMP(sv))
	    sv = newSVsv(sv);
	else {
	    SvTEMP_off(sv);
	    SvREFCNT_inc(sv);
	}
	SvRV(rv) = sv;
	SvROK_on(rv);
	*MARK = rv;
    }
    RETURN;
}

PP(pp_ref)
{
    dSP; dTARGET;
    SV *sv;
    char *pv;

    sv = POPs;
    if (!sv || !SvROK(sv))
	RETPUSHUNDEF;

    sv = SvRV(sv);
    pv = sv_reftype(sv,TRUE);
    PUSHp(pv, strlen(pv));
    RETURN;
}

PP(pp_bless)
{
    dSP;
    HV *stash;

    if (MAXARG == 1)
	stash = curcop->cop_stash;
    else
	stash = gv_stashsv(POPs, TRUE);

    (void)sv_bless(TOPs, stash);
    RETURN;
}

/* Pushy I/O. */

PP(pp_backtick)
{
    dSP; dTARGET;
    FILE *fp;
    char *tmps = POPp;
    TAINT_PROPER("``");
    fp = my_popen(tmps, "r");
    if (fp) {
	sv_setpv(TARG, "");	/* note that this preserves previous buffer */
	if (GIMME == G_SCALAR) {
	    while (sv_gets(TARG, fp, SvCUR(TARG)) != Nullch)
		/*SUPPRESS 530*/
		;
	    XPUSHs(TARG);
	}
	else {
	    SV *sv;

	    for (;;) {
		sv = NEWSV(56, 80);
		if (sv_gets(sv, fp, 0) == Nullch) {
		    SvREFCNT_dec(sv);
		    break;
		}
		XPUSHs(sv_2mortal(sv));
		if (SvLEN(sv) - SvCUR(sv) > 20) {
		    SvLEN_set(sv, SvCUR(sv)+1);
		    Renew(SvPVX(sv), SvLEN(sv), char);
		}
	    }
	}
	statusvalue = my_pclose(fp);
    }
    else {
	statusvalue = -1;
	if (GIMME == G_SCALAR)
	    RETPUSHUNDEF;
    }

    RETURN;
}

OP *
do_readline()
{
    dSP; dTARGETSTACKED;
    register SV *sv;
    STRLEN tmplen = 0;
    STRLEN offset;
    FILE *fp;
    register IO *io = GvIO(last_in_gv);
    register I32 type = op->op_type;

    fp = Nullfp;
    if (io) {
	fp = IoIFP(io);
	if (!fp) {
	    if (IoFLAGS(io) & IOf_ARGV) {
		if (IoFLAGS(io) & IOf_START) {
		    IoFLAGS(io) &= ~IOf_START;
		    IoLINES(io) = 0;
		    if (av_len(GvAVn(last_in_gv)) < 0) {
			SV *tmpstr = newSVpv("-", 1); /* assume stdin */
			av_push(GvAVn(last_in_gv), tmpstr);
		    }
		}
		fp = nextargv(last_in_gv);
		if (!fp) { /* Note: fp != IoIFP(io) */
		    (void)do_close(last_in_gv, FALSE); /* now it does*/
		    IoFLAGS(io) |= IOf_START;
		}
	    }
	    else if (type == OP_GLOB) {
		SV *tmpcmd = NEWSV(55, 0);
		SV *tmpglob = POPs;
		ENTER;
		SAVEFREESV(tmpcmd);
#ifdef VMS /* expand the wildcards right here, rather than opening a pipe, */
           /* since spawning off a process is a real performance hit */
		{
#include <descrip.h>
#include <lib$routines.h>
#include <nam.h>
#include <rmsdef.h>
		    char rslt[NAM$C_MAXRSS+1+sizeof(unsigned short int)] = {'\0','\0'};
		    char *rstr = rslt + sizeof(unsigned short int), *begin, *end, *cp;
		    char tmpfnam[L_tmpnam] = "SYS$SCRATCH:";
		    FILE *tmpfp;
		    STRLEN i;
		    struct dsc$descriptor_s wilddsc
		       = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
		    struct dsc$descriptor_vs rsdsc
		       = {sizeof rslt, DSC$K_DTYPE_VT, DSC$K_CLASS_VS, rslt};
		    unsigned long int cxt = 0, sts = 0, ok = 1, hasdir = 0, hasver = 0;

		    /* We could find out if there's an explicit dev/dir or version
		       by peeking into lib$find_file's internal context at
		       ((struct NAM *)((struct FAB *)cxt)->fab$l_nam)->nam$l_fnb
		       but that's unsupported, so I don't want to do it now and
		       have it bite someone in the future. */
		    strcat(tmpfnam,tmpnam(NULL));
		    cp = SvPV(tmpglob,i);
		    for (; i; i--) {
		       if (cp[i] == ';') hasver = 1;
		       if (cp[i] == '.') {
		           if (sts) hasver = 1;
		           else sts = 1;
		       }
		       if (cp[i] == ']' || cp[i] == '>' || cp[i] == ':') {
		           hasdir = 1;
		           break;
		       }
		    }
		    if ((tmpfp = fopen(tmpfnam,"w+","fop=dlt")) != NULL) {
		        wilddsc.dsc$a_pointer = SvPVX(tmpglob);
		        wilddsc.dsc$w_length = (unsigned short int) SvCUR(tmpglob);
		        while ((sts = lib$find_file(&wilddsc,&rsdsc,&cxt,
		                                    NULL,NULL,NULL,NULL))&1) {
		            end = rstr + (unsigned long int) *rslt;
		            if (!hasver) while (*end != ';') end--;
		            *(end++) = '\n';  *end = '\0';
		            if (hasdir) begin = rstr;
		            else {
		                begin = end;
		                while (*(--begin) != ']' && *begin != '>') ;
		                ++begin;
		            }
		            if (!(ok = fputs(begin,tmpfp) != EOF)) break;
		        }
		        (void)lib$find_file_end(&cxt);
		        if (ok && sts != RMS$_NMF) ok = 0;
		        if (!ok) {
		            fp = NULL;
		        }
		        else {
		           rewind(tmpfp);
		           IoTYPE(io) = '<';
		           IoIFP(io) = fp = tmpfp;
		        }
		    }
		}
#else /* !VMS */
#ifdef DOSISH
		sv_setpv(tmpcmd, "perlglob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, " |");
#else
#ifdef CSH
		sv_setpvn(tmpcmd, cshname, cshlen);
		sv_catpv(tmpcmd, " -cf 'set nonomatch; glob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, "'|");
#else
		sv_setpv(tmpcmd, "echo ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, "|tr -s ' \t\f\r' '\\012\\012\\012\\012'|");
#endif /* !CSH */
#endif /* !MSDOS */
		(void)do_open(last_in_gv, SvPVX(tmpcmd), SvCUR(tmpcmd),Nullfp);
		fp = IoIFP(io);
#endif /* !VMS */
		LEAVE;
	    }
	}
	else if (type == OP_GLOB)
	    SP--;
    }
    if (!fp) {
	if (dowarn)
	    warn("Read on closed filehandle <%s>", GvENAME(last_in_gv));
	if (GIMME == G_SCALAR) {
	    SvOK_off(TARG);
	    PUSHTARG;
	}
	RETURN;
    }
    if (GIMME == G_ARRAY) {
	sv = sv_2mortal(NEWSV(57, 80));
	offset = 0;
    }
    else {
	sv = TARG;
	SvUPGRADE(sv, SVt_PV);
	tmplen = SvLEN(sv);	/* remember if already alloced */
	if (!tmplen)
	    Sv_Grow(sv, 80);	/* try short-buffering it */
	if (type == OP_RCATLINE)
	    offset = SvCUR(sv);
	else
	    offset = 0;
    }
    for (;;) {
	if (!sv_gets(sv, fp, offset)) {
	    clearerr(fp);
	    if (IoFLAGS(io) & IOf_ARGV) {
		fp = nextargv(last_in_gv);
		if (fp)
		    continue;
		(void)do_close(last_in_gv, FALSE);
		IoFLAGS(io) |= IOf_START;
	    }
	    else if (type == OP_GLOB) {
		(void)do_close(last_in_gv, FALSE);
	    }
	    if (GIMME == G_SCALAR) {
		SvOK_off(TARG);
		PUSHTARG;
	    }
	    RETURN;
	}
	IoLINES(io)++;
	XPUSHs(sv);
	if (tainting) {
	    tainted = TRUE;
	    SvTAINT(sv); /* Anything from the outside world...*/
	}
	if (type == OP_GLOB) {
	    char *tmps;

	    if (SvCUR(sv) > 0)
		SvCUR(sv)--;
	    if (*SvEND(sv) == rschar)
		*SvEND(sv) = '\0';
	    else
		SvCUR(sv)++;
	    for (tmps = SvPVX(sv); *tmps; tmps++)
		if (!isALPHA(*tmps) && !isDIGIT(*tmps) &&
		    strchr("$&*(){}[]'\";\\|?<>~`", *tmps))
			break;
	    if (*tmps && stat(SvPVX(sv), &statbuf) < 0) {
		POPs;		/* Unmatched wildcard?  Chuck it... */
		continue;
	    }
	}
	if (GIMME == G_ARRAY) {
	    if (SvLEN(sv) - SvCUR(sv) > 20) {
		SvLEN_set(sv, SvCUR(sv)+1);
		Renew(SvPVX(sv), SvLEN(sv), char);
	    }
	    sv = sv_2mortal(NEWSV(58, 80));
	    continue;
	}
	else if (!tmplen && SvLEN(sv) - SvCUR(sv) > 80) {
	    /* try to reclaim a bit of scalar space (only on 1st alloc) */
	    if (SvCUR(sv) < 60)
		SvLEN_set(sv, 80);
	    else
		SvLEN_set(sv, SvCUR(sv)+40);	/* allow some slop */
	    Renew(SvPVX(sv), SvLEN(sv), char);
	}
	RETURN;
    }
}

PP(pp_glob)
{
    OP *result;
    ENTER;
    SAVEINT(rschar);
    SAVEINT(rslen);

    SAVESPTR(last_in_gv);	/* We don't want this to be permanent. */
    last_in_gv = (GV*)*stack_sp--;

    rslen = 1;
#ifdef DOSISH
    rschar = 0;
#else
#ifdef CSH
    rschar = 0;
#else
    rschar = '\n';
#endif	/* !CSH */
#endif	/* !MSDOS */
    result = do_readline();
    LEAVE;
    return result;
}

PP(pp_readline)
{
    last_in_gv = (GV*)(*stack_sp--);
    return do_readline();
}

PP(pp_indread)
{
    last_in_gv = gv_fetchpv(SvPVx(GvSV((GV*)(*stack_sp--)), na), TRUE,SVt_PVIO);
    return do_readline();
}

PP(pp_rcatline)
{
    last_in_gv = cGVOP->op_gv;
    return do_readline();
}

PP(pp_regcmaybe)
{
    return NORMAL;
}

PP(pp_regcomp) {
    dSP;
    register PMOP *pm = (PMOP*)cLOGOP->op_other;
    register char *t;
    SV *tmpstr;
    STRLEN len;

    tmpstr = POPs;
    t = SvPV(tmpstr, len);

    if (pm->op_pmregexp) {
	regfree(pm->op_pmregexp);
	pm->op_pmregexp = Null(REGEXP*);	/* crucial if regcomp aborts */
    }

    pm->op_pmregexp = regcomp(t, t + len, pm->op_pmflags);

    if (!pm->op_pmregexp->prelen && curpm)
	pm = curpm;
    else if (strEQ("\\s+", pm->op_pmregexp->precomp))
	pm->op_pmflags |= PMf_WHITE;

    if (pm->op_pmflags & PMf_KEEP) {
	if (!(pm->op_pmflags & PMf_FOLD))
	    scan_prefix(pm, pm->op_pmregexp->precomp,
		pm->op_pmregexp->prelen);
	pm->op_pmflags &= ~PMf_RUNTIME;	/* no point compiling again */
	hoistmust(pm);
	cLOGOP->op_first->op_next = op->op_next;
	/* XXX delete push code? */
    }
    RETURN;
}

PP(pp_match)
{
    dSP; dTARG;
    register PMOP *pm = cPMOP;
    register char *t;
    register char *s;
    char *strend;
    I32 global;
    I32 safebase;
    char *truebase;
    register REGEXP *rx = pm->op_pmregexp;
    I32 gimme = GIMME;
    STRLEN len;

    if (op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = GvSV(defgv);
	EXTEND(SP,1);
    }
    s = SvPV(TARG, len);
    strend = s + len;
    if (!s)
	DIE("panic: do_match");

    if (pm->op_pmflags & PMf_USED) {
	if (gimme == G_ARRAY)
	    RETURN;
	RETPUSHNO;
    }

    if (!rx->prelen && curpm) {
	pm = curpm;
	rx = pm->op_pmregexp;
    }
    truebase = t = s;
    if (global = pm->op_pmflags & PMf_GLOBAL) {
	rx->startp[0] = 0;
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg && mg->mg_len >= 0)
		rx->endp[0] = rx->startp[0] = s + mg->mg_len; 
	}
    }
    safebase = (gimme == G_ARRAY) || global;
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(multiline);
	multiline = pm->op_pmflags & PMf_MULTILINE;
    }

play_it_again:
    if (global && rx->startp[0]) {
	t = s = rx->endp[0];
	if (s > strend)
	    goto nope;
    }
    if (pm->op_pmshort) {
	if (pm->op_pmflags & PMf_SCANFIRST) {
	    if (SvSCREAM(TARG)) {
		if (screamfirst[BmRARE(pm->op_pmshort)] < 0)
		    goto nope;
		else if (!(s = screaminstr(TARG, pm->op_pmshort)))
		    goto nope;
		else if (pm->op_pmflags & PMf_ALL)
		    goto yup;
	    }
	    else if (!(s = fbm_instr((unsigned char*)s,
	      (unsigned char*)strend, pm->op_pmshort)))
		goto nope;
	    else if (pm->op_pmflags & PMf_ALL)
		goto yup;
	    if (s && rx->regback >= 0) {
		++BmUSEFUL(pm->op_pmshort);
		s -= rx->regback;
		if (s < t)
		    s = t;
	    }
	    else
		s = t;
	}
	else if (!multiline) {
	    if (*SvPVX(pm->op_pmshort) != *s ||
	      bcmp(SvPVX(pm->op_pmshort), s, pm->op_pmslen) ) {
		if (pm->op_pmflags & PMf_FOLD) {
		    if (ibcmp((U8*)SvPVX(pm->op_pmshort), (U8*)s, pm->op_pmslen) )
			goto nope;
		}
		else
		    goto nope;
	    }
	}
	if (--BmUSEFUL(pm->op_pmshort) < 0) {
	    SvREFCNT_dec(pm->op_pmshort);
	    pm->op_pmshort = Nullsv;	/* opt is being useless */
	}
    }
    if (!rx->nparens && !global) {
	gimme = G_SCALAR;			/* accidental array context? */
	safebase = FALSE;
    }
    if (regexec(rx, s, strend, truebase, 0,
      SvSCREAM(TARG) ? TARG : Nullsv,
      safebase)) {
	curpm = pm;
	if (pm->op_pmflags & PMf_ONCE)
	    pm->op_pmflags |= PMf_USED;
	goto gotcha;
    }
    else
	goto ret_no;
    /*NOTREACHED*/

  gotcha:
    if (gimme == G_ARRAY) {
	I32 iters, i, len;

	iters = rx->nparens;
	if (global && !iters)
	    i = 1;
	else
	    i = 0;
	EXTEND(SP, iters + i);
	for (i = !i; i <= iters; i++) {
	    PUSHs(sv_newmortal());
	    /*SUPPRESS 560*/
	    if ((s = rx->startp[i]) && rx->endp[i] ) {
		len = rx->endp[i] - s;
		if (len > 0)
		    sv_setpvn(*SP, s, len);
	    }
	}
	if (global) {
	    truebase = rx->subbeg;
	    if (rx->startp[0] && rx->startp[0] == rx->endp[0])
		++rx->endp[0];
	    goto play_it_again;
	}
	RETURN;
    }
    else {
	if (global) {
	    MAGIC* mg = 0;
	    if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG))
		mg = mg_find(TARG, 'g');
	    if (!mg) {
		sv_magic(TARG, (SV*)0, 'g', Nullch, 0);
		mg = mg_find(TARG, 'g');
	    }
	    mg->mg_len = rx->startp[0] ? rx->endp[0] - truebase : -1;
	}
	RETPUSHYES;
    }

yup:
    ++BmUSEFUL(pm->op_pmshort);
    curpm = pm;
    if (pm->op_pmflags & PMf_ONCE)
	pm->op_pmflags |= PMf_USED;
    if (global) {
	rx->subbeg = truebase;
	rx->subend = strend;
	rx->startp[0] = s;
	rx->endp[0] = s + SvCUR(pm->op_pmshort);
	goto gotcha;
    }
    if (sawampersand) {
	char *tmps;

	if (rx->subbase)
	    Safefree(rx->subbase);
	tmps = rx->subbase = nsavestr(t, strend-t);
	rx->subbeg = tmps;
	rx->subend = tmps + (strend-t);
	tmps = rx->startp[0] = tmps + (s - t);
	rx->endp[0] = tmps + SvCUR(pm->op_pmshort);
    }
    RETPUSHYES;

nope:
    if (pm->op_pmshort)
	++BmUSEFUL(pm->op_pmshort);

ret_no:
    if (global) {
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg)
		mg->mg_len = -1;
	}
    }
    if (gimme == G_ARRAY)
	RETURN;
    RETPUSHNO;
}

PP(pp_subst)
{
    dSP; dTARG;
    register PMOP *pm = cPMOP;
    PMOP *rpm = pm;
    register SV *dstr;
    register char *s;
    char *strend;
    register char *m;
    char *c;
    register char *d;
    STRLEN clen;
    I32 iters = 0;
    I32 maxiters;
    register I32 i;
    bool once;
    char *orig;
    I32 safebase;
    register REGEXP *rx = pm->op_pmregexp;
    STRLEN len;

    if (pm->op_pmflags & PMf_CONST)	/* known replacement string? */
	dstr = POPs;
    if (op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = GvSV(defgv);
	EXTEND(SP,1);
    }
    s = SvPV_force(TARG, len);
    if (!pm || !s)
	DIE("panic: do_subst");

    strend = s + len;
    maxiters = (strend - s) + 10;

    if (!rx->prelen && curpm) {
	pm = curpm;
	rx = pm->op_pmregexp;
    }
    safebase = ((!rx || !rx->nparens) && !sawampersand);
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(multiline);
	multiline = pm->op_pmflags & PMf_MULTILINE;
    }
    orig = m = s;
    if (pm->op_pmshort) {
	if (pm->op_pmflags & PMf_SCANFIRST) {
	    if (SvSCREAM(TARG)) {
		if (screamfirst[BmRARE(pm->op_pmshort)] < 0)
		    goto nope;
		else if (!(s = screaminstr(TARG, pm->op_pmshort)))
		    goto nope;
	    }
	    else if (!(s = fbm_instr((unsigned char*)s, (unsigned char*)strend,
	      pm->op_pmshort)))
		goto nope;
	    if (s && rx->regback >= 0) {
		++BmUSEFUL(pm->op_pmshort);
		s -= rx->regback;
		if (s < m)
		    s = m;
	    }
	    else
		s = m;
	}
	else if (!multiline) {
	    if (*SvPVX(pm->op_pmshort) != *s ||
	      bcmp(SvPVX(pm->op_pmshort), s, pm->op_pmslen) ) {
		if (pm->op_pmflags & PMf_FOLD) {
		    if (ibcmp((U8*)SvPVX(pm->op_pmshort), (U8*)s, pm->op_pmslen) )
			goto nope;
		}
		else
		    goto nope;
	    }
	}
	if (--BmUSEFUL(pm->op_pmshort) < 0) {
	    SvREFCNT_dec(pm->op_pmshort);
	    pm->op_pmshort = Nullsv;	/* opt is being useless */
	}
    }
    once = !(rpm->op_pmflags & PMf_GLOBAL);
    if (rpm->op_pmflags & PMf_CONST) {	/* known replacement string? */
	c = SvPV(dstr, clen);
	if (clen <= rx->minlen) {
					/* can do inplace substitution */
	    if (regexec(rx, s, strend, orig, 0,
	      SvSCREAM(TARG) ? TARG : Nullsv, safebase)) {
		if (rx->subbase) 	/* oops, no we can't */
		    goto long_way;
		d = s;
		curpm = pm;
		SvSCREAM_off(TARG);	/* disable possible screamer */
		if (once) {
		    m = rx->startp[0];
		    d = rx->endp[0];
		    s = orig;
		    if (m - s > strend - d) {	/* faster to shorten from end */
			if (clen) {
			    Copy(c, m, clen, char);
			    m += clen;
			}
			i = strend - d;
			if (i > 0) {
			    Move(d, m, i, char);
			    m += i;
			}
			*m = '\0';
			SvCUR_set(TARG, m - s);
			SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			RETURN;
		    }
		    /*SUPPRESS 560*/
		    else if (i = m - s) {	/* faster from front */
			d -= clen;
			m = d;
			sv_chop(TARG, d-i);
			s += i;
			while (i--)
			    *--d = *--s;
			if (clen)
			    Copy(c, m, clen, char);
			SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			RETURN;
		    }
		    else if (clen) {
			d -= clen;
			sv_chop(TARG, d);
			Copy(c, d, clen, char);
			SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			RETURN;
		    }
		    else {
			sv_chop(TARG, d);
			SvPOK_only(TARG);
			SvSETMAGIC(TARG);
			PUSHs(&sv_yes);
			RETURN;
		    }
		    /* NOTREACHED */
		}
		do {
		    if (iters++ > maxiters)
			DIE("Substitution loop");
		    m = rx->startp[0];
		    /*SUPPRESS 560*/
		    if (i = m - s) {
			if (s != d)
			    Move(s, d, i, char);
			d += i;
		    }
		    if (clen) {
			Copy(c, d, clen, char);
			d += clen;
		    }
		    s = rx->endp[0];
		} while (regexec(rx, s, strend, orig, s == m,
		    Nullsv, TRUE));	/* (don't match same null twice) */
		if (s != d) {
		    i = strend - s;
		    SvCUR_set(TARG, d - SvPVX(TARG) + i);
		    Move(s, d, i+1, char);		/* include the Null */
		}
		SvPOK_only(TARG);
		SvSETMAGIC(TARG);
		PUSHs(sv_2mortal(newSViv((I32)iters)));
		RETURN;
	    }
	    PUSHs(&sv_no);
	    RETURN;
	}
    }
    else
	c = Nullch;
    if (regexec(rx, s, strend, orig, 0,
      SvSCREAM(TARG) ? TARG : Nullsv, safebase)) {
    long_way:
	dstr = NEWSV(25, sv_len(TARG));
	sv_setpvn(dstr, m, s-m);
	curpm = pm;
	if (!c) {
	    register CONTEXT *cx;
	    PUSHSUBST(cx);
	    RETURNOP(cPMOP->op_pmreplroot);
	}
	do {
	    if (iters++ > maxiters)
		DIE("Substitution loop");
	    if (rx->subbase && rx->subbase != orig) {
		m = s;
		s = orig;
		orig = rx->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0];
	    sv_catpvn(dstr, s, m-s);
	    s = rx->endp[0];
	    if (clen)
		sv_catpvn(dstr, c, clen);
	    if (once)
		break;
	} while (regexec(rx, s, strend, orig, s == m, Nullsv,
	    safebase));
	sv_catpvn(dstr, s, strend - s);
	sv_replace(TARG, dstr);
	SvPOK_only(TARG);
	SvSETMAGIC(TARG);
	PUSHs(sv_2mortal(newSViv((I32)iters)));
	RETURN;
    }
    PUSHs(&sv_no);
    RETURN;

nope:
    ++BmUSEFUL(pm->op_pmshort);
    PUSHs(&sv_no);
    RETURN;
}

PP(pp_substcont)
{
    dSP;
    register PMOP *pm = (PMOP*) cLOGOP->op_other;
    register CONTEXT *cx = &cxstack[cxstack_ix];
    register SV *dstr = cx->sb_dstr;
    register char *s = cx->sb_s;
    register char *m = cx->sb_m;
    char *orig = cx->sb_orig;
    register REGEXP *rx = pm->op_pmregexp;

    if (cx->sb_iters++) {
	if (cx->sb_iters > cx->sb_maxiters)
	    DIE("Substitution loop");

	sv_catsv(dstr, POPs);
	if (rx->subbase)
	    Safefree(rx->subbase);
	rx->subbase = cx->sb_subbase;

	/* Are we done */
	if (cx->sb_once || !regexec(rx, s, cx->sb_strend, orig,
				s == m, Nullsv, cx->sb_safebase))
	{
	    SV *targ = cx->sb_targ;
	    sv_catpvn(dstr, s, cx->sb_strend - s);
	    sv_replace(targ, dstr);
	    SvPOK_only(targ);
	    SvSETMAGIC(targ);
	    PUSHs(sv_2mortal(newSViv((I32)cx->sb_iters - 1)));
	    POPSUBST(cx);
	    RETURNOP(pm->op_next);
	}
    }
    if (rx->subbase && rx->subbase != orig) {
	m = s;
	s = orig;
	cx->sb_orig = orig = rx->subbase;
	s = orig + (m - s);
	cx->sb_strend = s + (cx->sb_strend - m);
    }
    cx->sb_m = m = rx->startp[0];
    sv_catpvn(dstr, s, m-s);
    cx->sb_s = rx->endp[0];
    cx->sb_subbase = rx->subbase;

    rx->subbase = Nullch;	/* so recursion works */
    RETURNOP(pm->op_pmreplstart);
}

PP(pp_trans)
{
    dSP; dTARG;
    SV *sv;

    if (op->op_flags & OPf_STACKED)
	sv = POPs;
    else {
	sv = GvSV(defgv);
	EXTEND(SP,1);
    }
    TARG = NEWSV(27,0);
    PUSHi(do_trans(sv, op));
    RETURN;
}

/* Lvalue operators. */

PP(pp_sassign)
{
    dSP; dPOPTOPssrl;
    if (op->op_private & OPpASSIGN_BACKWARDS) {
	SV *temp;
	temp = left; left = right; right = temp;
    }
    if (tainting && tainted && (!SvGMAGICAL(left) || !SvSMAGICAL(left) ||
				!mg_find(left, 't'))) {
	TAINT_NOT;
    }
    SvSetSV(right, left);
    SvSETMAGIC(right);
    SETs(right);
    RETURN;
}

PP(pp_aassign)
{
    dSP;
    SV **lastlelem = stack_sp;
    SV **lastrelem = stack_base + POPMARK;
    SV **firstrelem = stack_base + POPMARK + 1;
    SV **firstlelem = lastrelem + 1;

    register SV **relem;
    register SV **lelem;

    register SV *sv;
    register AV *ary;

    HV *hash;
    I32 i;
    int magic;

    delaymagic = DM_DELAY;		/* catch simultaneous items */

    /* If there's a common identifier on both sides we have to take
     * special care that assigning the identifier on the left doesn't
     * clobber a value on the right that's used later in the list.
     */
    if (op->op_private & OPpASSIGN_COMMON) {
        for (relem = firstrelem; relem <= lastrelem; relem++) {
            /*SUPPRESS 560*/
            if (sv = *relem)
                *relem = sv_mortalcopy(sv);
        }
    }

    relem = firstrelem;
    lelem = firstlelem;
    ary = Null(AV*);
    hash = Null(HV*);
    while (lelem <= lastlelem) {
	sv = *lelem++;
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	    ary = (AV*)sv;
	    magic = SvSMAGICAL(ary) != 0;
	    
	    av_clear(ary);
	    i = 0;
	    while (relem <= lastrelem) {	/* gobble up all the rest */
		sv = NEWSV(28,0);
		assert(*relem);
		sv_setsv(sv,*relem);
		*(relem++) = sv;
		(void)av_store(ary,i++,sv);
		if (magic)
		    mg_set(sv);
	    }
	    break;
	case SVt_PVHV: {
		char *tmps;
		SV *tmpstr;

		hash = (HV*)sv;
		magic = SvSMAGICAL(hash) != 0;
		hv_clear(hash);

		while (relem < lastrelem) {	/* gobble up all the rest */
		    STRLEN len;
		    if (*relem)
			sv = *(relem++);
		    else
			sv = &sv_no, relem++;
		    tmps = SvPV(sv, len);
		    tmpstr = NEWSV(29,0);
		    if (*relem)
			sv_setsv(tmpstr,*relem);	/* value */
		    *(relem++) = tmpstr;
		    (void)hv_store(hash,tmps,len,tmpstr,0);
		    if (magic)
			mg_set(tmpstr);
		}
	    }
	    break;
	default:
	    if (SvTHINKFIRST(sv)) {
		if (SvREADONLY(sv) && curcop != &compiling) {
		    if (sv != &sv_undef && sv != &sv_yes && sv != &sv_no)
			DIE(no_modify);
		    if (relem <= lastrelem)
			relem++;
		    break;
		}
		if (SvROK(sv))
		    sv_unref(sv);
	    }
	    if (relem <= lastrelem) {
		sv_setsv(sv, *relem);
		*(relem++) = sv;
	    }
	    else
		sv_setsv(sv, &sv_undef);
	    SvSETMAGIC(sv);
	    break;
	}
    }
    if (delaymagic & ~DM_DELAY) {
	if (delaymagic & DM_UID) {
#ifdef HAS_SETRESUID
	    (void)setresuid(uid,euid,(Uid_t)-1);
#else /* not HAS_SETRESUID */
#ifdef HAS_SETREUID
	    (void)setreuid(uid,euid);
#else /* not HAS_SETREUID */
#ifdef HAS_SETRUID
	    if ((delaymagic & DM_UID) == DM_RUID) {
		(void)setruid(uid);
		delaymagic =~ DM_RUID;
	    }
#endif /* HAS_SETRUID */
#endif /* HAS_SETRESUID */
#ifdef HAS_SETEUID
	    if ((delaymagic & DM_UID) == DM_EUID) {
		(void)seteuid(uid);
		delaymagic =~ DM_EUID;
	    }
#endif /* HAS_SETEUID */
	    if (delaymagic & DM_UID) {
		if (uid != euid)
		    DIE("No setreuid available");
		(void)setuid(uid);
	    }
#endif /* not HAS_SETREUID */
	    uid = (int)getuid();
	    euid = (int)geteuid();
	}
	if (delaymagic & DM_GID) {
#ifdef HAS_SETRESGID
	    (void)setresgid(gid,egid,(Gid_t)-1);
#else /* not HAS_SETREGID */
#ifdef HAS_SETREGID
	    (void)setregid(gid,egid);
#else /* not HAS_SETREGID */
#endif /* not HAS_SETRESGID */
#ifdef HAS_SETRGID
	    if ((delaymagic & DM_GID) == DM_RGID) {
		(void)setrgid(gid);
		delaymagic =~ DM_RGID;
	    }
#endif /* HAS_SETRGID */
#ifdef HAS_SETRESGID
	    (void)setresgid(gid,egid,(Gid_t)-1);
#else /* not HAS_SETREGID */
#ifdef HAS_SETEGID
	    if ((delaymagic & DM_GID) == DM_EGID) {
		(void)setegid(gid);
		delaymagic =~ DM_EGID;
	    }
#endif /* HAS_SETEGID */
	    if (delaymagic & DM_GID) {
		if (gid != egid)
		    DIE("No setregid available");
		(void)setgid(gid);
	    }
#endif /* not HAS_SETRESGID */
#endif /* not HAS_SETREGID */
	    gid = (int)getgid();
	    egid = (int)getegid();
	}
	tainting |= (euid != uid || egid != gid);
    }
    delaymagic = 0;
    if (GIMME == G_ARRAY) {
	if (ary || hash)
	    SP = lastrelem;
	else
	    SP = firstrelem + (lastlelem - firstlelem);
	RETURN;
    }
    else {
	SP = firstrelem;
	for (relem = firstrelem; relem <= lastrelem; ++relem) {
	    if (SvOK(*relem)) {
		dTARGET;
		
		SETi(lastrelem - firstrelem + 1);
		RETURN;
	    }
	}
	RETSETUNDEF;
    }
}

PP(pp_schop)
{
    dSP; dTARGET;
    do_chop(TARG, TOPs);
    SETTARG;
    RETURN;
}

PP(pp_chop)
{
    dSP; dMARK; dTARGET;
    while (SP > MARK)
	do_chop(TARG, POPs);
    PUSHTARG;
    RETURN;
}

PP(pp_ssafechop)
{
    dSP; dTARGET;
    SETi(do_safechop(TOPs));
    RETURN;
}

PP(pp_safechop)
{
    dSP; dMARK; dTARGET;
    register I32 count = 0;
    
    while (SP > MARK)
	count += do_safechop(POPs);
    PUSHi(count);
    RETURN;
}

PP(pp_defined)
{
    dSP;
    register SV* sv;

    sv = POPs;
    if (!sv || !SvANY(sv))
	RETPUSHNO;
    switch (SvTYPE(sv)) {
    case SVt_PVAV:
	if (AvMAX(sv) >= 0)
	    RETPUSHYES;
	break;
    case SVt_PVHV:
	if (HvARRAY(sv))
	    RETPUSHYES;
	break;
    case SVt_PVCV:
	if (CvROOT(sv) || CvXSUB(sv))
	    RETPUSHYES;
	break;
    default:
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvOK(sv))
	    RETPUSHYES;
    }
    RETPUSHNO;
}

PP(pp_undef)
{
    dSP;
    SV *sv;

    if (!op->op_private)
	RETPUSHUNDEF;

    sv = POPs;
    if (!sv)
	RETPUSHUNDEF;

    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv))
	    RETPUSHUNDEF;
	if (SvROK(sv))
	    sv_unref(sv);
    }

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_PVAV:
	av_undef((AV*)sv);
	break;
    case SVt_PVHV:
	hv_undef((HV*)sv);
	break;
    case SVt_PVCV:
	sub_generation++;
	cv_undef((CV*)sv);
	break;
    default:
	if (sv != GvSV(defgv)) {
	    if (SvPOK(sv) && SvLEN(sv)) {
		SvOOK_off(sv);
		Safefree(SvPVX(sv));
		SvPV_set(sv, Nullch);
		SvLEN_set(sv, 0);
	    }
	    SvOK_off(sv);
	    SvSETMAGIC(sv);
	}
    }

    RETPUSHUNDEF;
}

PP(pp_study)
{
    dSP; dTARGET;
    register unsigned char *s;
    register I32 pos;
    register I32 ch;
    register I32 *sfirst;
    register I32 *snext;
    I32 retval;
    STRLEN len;

    s = (unsigned char*)(SvPV(TARG, len));
    pos = len;
    if (lastscream)
	SvSCREAM_off(lastscream);
    lastscream = TARG;
    if (pos <= 0) {
	retval = 0;
	goto ret;
    }
    if (pos > maxscream) {
	if (maxscream < 0) {
	    maxscream = pos + 80;
	    New(301, screamfirst, 256, I32);
	    New(302, screamnext, maxscream, I32);
	}
	else {
	    maxscream = pos + pos / 4;
	    Renew(screamnext, maxscream, I32);
	}
    }

    sfirst = screamfirst;
    snext = screamnext;

    if (!sfirst || !snext)
	DIE("do_study: out of memory");

    for (ch = 256; ch; --ch)
	*sfirst++ = -1;
    sfirst -= 256;

    while (--pos >= 0) {
	ch = s[pos];
	if (sfirst[ch] >= 0)
	    snext[pos] = sfirst[ch] - pos;
	else
	    snext[pos] = -pos;
	sfirst[ch] = pos;

	/* If there were any case insensitive searches, we must assume they
	 * all are.  This speeds up insensitive searches much more than
	 * it slows down sensitive ones.
	 */
	if (sawi)
	    sfirst[fold[ch]] = pos;
    }

    SvSCREAM_on(TARG);
    retval = 1;
  ret:
    XPUSHs(sv_2mortal(newSViv((I32)retval)));
    RETURN;
}

PP(pp_preinc)
{
    dSP;
    sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_predec)
{
    dSP;
    sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_postinc)
{
    dSP; dTARGET;
    sv_setsv(TARG, TOPs);
    sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    if (!SvOK(TARG))
	sv_setiv(TARG, 0);
    SETs(TARG);
    return NORMAL;
}

PP(pp_postdec)
{
    dSP; dTARGET;
    sv_setsv(TARG, TOPs);
    sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    SETs(TARG);
    return NORMAL;
}

/* Ordinary operators. */

PP(pp_pow)
{
    dSP; dATARGET; tryAMAGICbin(pow,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( pow( left, right) );
      RETURN;
    }
}

PP(pp_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( left * right );
      RETURN;
    }
}

PP(pp_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN); 
    {
      dPOPnv;
      if (value == 0.0)
	DIE("Illegal division by zero");
#ifdef SLOPPYDIVIDE
      /* insure that 20./5. == 4. */
      {
	double x;
	I32    k;
	x =  POPn;
	if ((double)(I32)x     == x &&
	    (double)(I32)value == value &&
	    (k = (I32)x/(I32)value)*(I32)value == (I32)x) {
	    value = k;
	} else {
	    value = x/value;
	}
      }
#else
      value = POPn / value;
#endif
      PUSHn( value );
      RETURN;
    }
}

PP(pp_modulo)
{
    dSP; dATARGET; tryAMAGICbin(mod,opASSIGN);
    {
      register unsigned long tmpulong;
      register long tmplong;
      I32 value;

      tmpulong = (unsigned long) POPn;
      if (tmpulong == 0L)
	DIE("Illegal modulus zero");
      value = TOPn;
      if (value >= 0.0)
	value = (I32)(((unsigned long)value) % tmpulong);
      else {
	tmplong = (long)value;
	value = (I32)(tmpulong - ((-tmplong - 1) % tmpulong)) - 1;
      }
      SETi(value);
      RETURN;
    }
}

PP(pp_repeat)
{
    dSP; dATARGET;
    register I32 count = POPi;
    if (GIMME == G_ARRAY && op->op_private & OPpREPEAT_DOLIST) {
	dMARK;
	I32 items = SP - MARK;
	I32 max;

	max = items * count;
	MEXTEND(MARK, max);
	if (count > 1) {
	    while (SP > MARK) {
		if (*SP)
		    SvTEMP_off((*SP));
		SP--;
	    }
	    MARK++;
	    repeatcpy((char*)(MARK + items), (char*)MARK,
		items * sizeof(SV*), count - 1);
	    SP += max;
	}
	else if (count <= 0)
	    SP -= items;
    }
    else {	/* Note: mark already snarfed by pp_list */
	SV *tmpstr;
	STRLEN len;

	tmpstr = POPs;
	if (TARG == tmpstr && SvTHINKFIRST(tmpstr)) {
	    if (SvREADONLY(tmpstr) && curcop != &compiling)
		DIE("Can't x= to readonly value");
	    if (SvROK(tmpstr))
		sv_unref(tmpstr);
	}
	SvSetSV(TARG, tmpstr);
	SvPV_force(TARG, len);
	if (count >= 1) {
	    SvGROW(TARG, (count * len) + 1);
	    if (count > 1)
		repeatcpy(SvPVX(TARG) + len, SvPVX(TARG), len, count - 1);
	    SvCUR(TARG) *= count;
	    *SvEND(TARG) = '\0';
	    SvPOK_only(TARG);
	}
	else
	    sv_setsv(TARG, &sv_no);
	PUSHTARG;
    }
    RETURN;
}

PP(pp_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( left + right );
      RETURN;
    }
}

PP(pp_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN); 
    {
      dPOPTOPnnrl;
      SETn( left - right );
      RETURN;
    }
}

PP(pp_concat)
{
    dSP; dATARGET; dPOPTOPssrl;
    STRLEN len;
    char *s;
    if (TARG != left) {
	s = SvPV(left,len);
	sv_setpvn(TARG,s,len);
    }
    s = SvPV(right,len);
    sv_catpvn(TARG,s,len);
    SETTARG;
    RETURN;
}

PP(pp_stringify)
{
    dSP; dTARGET;
    STRLEN len;
    char *s;
    s = SvPV(TOPs,len);
    sv_setpvn(TARG,s,len);
    SETTARG;
    RETURN;
}

PP(pp_left_shift)
{
    dSP; dATARGET; tryAMAGICbin(lshift,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left << right );
      RETURN;
    }
}

PP(pp_right_shift)
{
    dSP; dATARGET; tryAMAGICbin(rshift,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left >> right );
      RETURN;
    }
}

PP(pp_lt)
{
    dSP; tryAMAGICbinSET(lt,0); 
    {
      dPOPnv;
      SETs((TOPn < value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_gt)
{
    dSP; tryAMAGICbinSET(gt,0); 
    {
      dPOPnv;
      SETs((TOPn > value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_le)
{
    dSP; tryAMAGICbinSET(le,0); 
    {
      dPOPnv;
      SETs((TOPn <= value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_ge)
{
    dSP; tryAMAGICbinSET(ge,0); 
    {
      dPOPnv;
      SETs((TOPn >= value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_eq)
{
    dSP; tryAMAGICbinSET(eq,0); 
    {
      dPOPnv;
      SETs((TOPn == value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_ne)
{
    dSP; tryAMAGICbinSET(ne,0); 
    {
      dPOPnv;
      SETs((TOPn != value) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0); 
    {
      dPOPTOPnnrl;
      I32 value;

      if (left > right)
	value = 1;
      else if (left < right)
	value = -1;
      else
	value = 0;
      SETi(value);
      RETURN;
    }
}

PP(pp_slt)
{
    dSP; tryAMAGICbinSET(slt,0); 
    {
      dPOPTOPssrl;
      SETs( sv_cmp(left, right) < 0 ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_sgt)
{
    dSP; tryAMAGICbinSET(sgt,0); 
    {
      dPOPTOPssrl;
      SETs( sv_cmp(left, right) > 0 ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_sle)
{
    dSP; tryAMAGICbinSET(sle,0); 
    {
      dPOPTOPssrl;
      SETs( sv_cmp(left, right) <= 0 ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_sge)
{
    dSP; tryAMAGICbinSET(sge,0); 
    {
      dPOPTOPssrl;
      SETs( sv_cmp(left, right) >= 0 ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_seq)
{
    dSP; tryAMAGICbinSET(seq,0); 
    {
      dPOPTOPssrl;
      SETs( sv_eq(left, right) ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_sne)
{
    dSP; tryAMAGICbinSET(sne,0); 
    {
      dPOPTOPssrl;
      SETs( !sv_eq(left, right) ? &sv_yes : &sv_no );
      RETURN;
    }
}

PP(pp_scmp)
{
    dSP; dTARGET;  tryAMAGICbin(scmp,0);
    {
      dPOPTOPssrl;
      SETi( sv_cmp(left, right) );
      RETURN;
    }
}

PP(pp_bit_and) {
    dSP; dATARGET; tryAMAGICbin(band,opASSIGN); 
    {
      dPOPTOPssrl;
      if (SvNIOK(left) || SvNIOK(right)) {
	unsigned long value = U_L(SvNV(left));
	value = value & U_L(SvNV(right));
	SETn((double)value);
      }
      else {
	do_vop(op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_xor)
{
    dSP; dATARGET; tryAMAGICbin(bxor,opASSIGN); 
    {
      dPOPTOPssrl;
      if (SvNIOK(left) || SvNIOK(right)) {
	unsigned long value = U_L(SvNV(left));
	value = value ^ U_L(SvNV(right));
	SETn((double)value);
      }
      else {
	do_vop(op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_or)
{
    dSP; dATARGET; tryAMAGICbin(bor,opASSIGN); 
    {
      dPOPTOPssrl;
      if (SvNIOK(left) || SvNIOK(right)) {
	unsigned long value = U_L(SvNV(left));
	value = value | U_L(SvNV(right));
	SETn((double)value);
      }
      else {
	do_vop(op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    {
	dTOPss;
	if (SvNIOK(sv))
	    SETn(-SvNV(sv));
	else if (SvPOK(sv)) {
	    STRLEN len;
	    char *s = SvPV(sv, len);
	    if (isALPHA(*s) || *s == '_') {
		sv_setpvn(TARG, "-", 1);
		sv_catsv(TARG, sv);
	    }
	    else if (*s == '+' || *s == '-') {
		sv_setsv(TARG, sv);
		*SvPV_force(TARG, len) = *s == '-' ? '+' : '-';
	    }
	    else
		sv_setnv(TARG, -SvNV(sv));
	    SETTARG;
	}
    }
    RETURN;
}

PP(pp_not)
{
#ifdef OVERLOAD
    dSP; tryAMAGICunSET(not);
#endif /* OVERLOAD */
    *stack_sp = SvTRUE(*stack_sp) ? &sv_no : &sv_yes;
    return NORMAL;
}

PP(pp_complement)
{
    dSP; dTARGET; tryAMAGICun(compl); 
    {
      dTOPss;
      register I32 anum;

      if (SvNIOK(sv)) {
	SETi(  ~SvIV(sv) );
      }
      else {
	register char *tmps;
	register long *tmpl;
	STRLEN len;

	SvSetSV(TARG, sv);
	tmps = SvPV_force(TARG, len);
	anum = len;
#ifdef LIBERAL
	for ( ; anum && (unsigned long)tmps % sizeof(long); anum--, tmps++)
	    *tmps = ~*tmps;
	tmpl = (long*)tmps;
	for ( ; anum >= sizeof(long); anum -= sizeof(long), tmpl++)
	    *tmpl = ~*tmpl;
	tmps = (char*)tmpl;
#endif
	for ( ; anum > 0; anum--, tmps++)
	    *tmps = ~*tmps;

	SETs(TARG);
      }
      RETURN;
    }
}

/* integer versions of some of the above */

PP(pp_i_preinc)
{
#ifndef OVERLOAD
    dSP; dTOPiv;
    sv_setiv(TOPs, value + 1);
    SvSETMAGIC(TOPs);
#else
    dSP;
    if (SvAMAGIC(TOPs) ) {
      sv_inc(TOPs);
    } else {
      dTOPiv;
      sv_setiv(TOPs, value + 1);
      SvSETMAGIC(TOPs);
    }
#endif /* OVERLOAD */
    return NORMAL;
}

PP(pp_i_predec)
{
#ifndef OVERLOAD
    dSP; dTOPiv;
    sv_setiv(TOPs, value - 1);
    SvSETMAGIC(TOPs);
#else
    dSP;
    if (SvAMAGIC(TOPs)) {
      sv_dec(TOPs);
    } else {
      dTOPiv;
      sv_setiv(TOPs, value - 1);
      SvSETMAGIC(TOPs);
    }
#endif /* OVERLOAD */
    return NORMAL;
}

PP(pp_i_postinc)
{
    dSP; dTARGET;
    sv_setsv(TARG, TOPs);
#ifndef OVERLOAD
    sv_setiv(TOPs, SvIV(TOPs) + 1);
    SvSETMAGIC(TOPs);
#else
    if (SvAMAGIC(TOPs) ) {
      sv_inc(TOPs);
    } else {
      sv_setiv(TOPs, SvIV(TOPs) + 1);
      SvSETMAGIC(TOPs);
    }
#endif /* OVERLOAD */
    if (!SvOK(TARG))
	sv_setiv(TARG, 0);
    SETs(TARG);
    return NORMAL;
}

PP(pp_i_postdec)
{
    dSP; dTARGET;
    sv_setsv(TARG, TOPs);
#ifndef OVERLOAD
    sv_setiv(TOPs, SvIV(TOPs) - 1);
    SvSETMAGIC(TOPs);
#else
    if (SvAMAGIC(TOPs) ) {
      sv_dec(TOPs);
    } else {
      sv_setiv(TOPs, SvIV(TOPs) - 1);
      SvSETMAGIC(TOPs);
    }
#endif /* OVERLOAD */
    SETs(TARG);
    return NORMAL;
}

PP(pp_i_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left * right );
      RETURN;
    }
}

PP(pp_i_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN); 
    {
      dPOPiv;
      if (value == 0)
	DIE("Illegal division by zero");
      value = POPi / value;
      PUSHi( value );
      RETURN;
    }
}

PP(pp_i_modulo)
{
    dSP; dATARGET; tryAMAGICbin(mod,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left % right );
      RETURN;
    }
}

PP(pp_i_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left + right );
      RETURN;
    }
}

PP(pp_i_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN); 
    {
      dPOPTOPiirl;
      SETi( left - right );
      RETURN;
    }
}

PP(pp_i_lt)
{
    dSP; tryAMAGICbinSET(lt,0); 
    {
      dPOPTOPiirl;
      SETs((left < right) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_i_gt)
{
    dSP; tryAMAGICbinSET(gt,0); 
    {
      dPOPTOPiirl;
      SETs((left > right) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_i_le)
{
    dSP; tryAMAGICbinSET(le,0); 
    {
      dPOPTOPiirl;
      SETs((left <= right) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_i_ge)
{
    dSP; tryAMAGICbinSET(ge,0); 
    {
      dPOPTOPiirl;
      SETs((left >= right) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_i_eq)
{
    dSP; tryAMAGICbinSET(eq,0); 
    {
      dPOPTOPiirl;
      SETs((left == right) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_i_ne)
{
    dSP; tryAMAGICbinSET(ne,0); 
    {
      dPOPTOPiirl;
      SETs((left != right) ? &sv_yes : &sv_no);
      RETURN;
    }
}

PP(pp_i_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0); 
    {
      dPOPTOPiirl;
      I32 value;

      if (left > right)
	value = 1;
      else if (left < right)
	value = -1;
      else
	value = 0;
      SETi(value);
      RETURN;
    }
}

PP(pp_i_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    SETi(-TOPi);
    RETURN;
}

/* High falutin' math. */

PP(pp_atan2)
{
    dSP; dTARGET; tryAMAGICbin(atan2,0); 
    {
      dPOPTOPnnrl;
      SETn(atan2(left, right));
      RETURN;
    }
}

PP(pp_sin)
{
    dSP; dTARGET; tryAMAGICun(sin);
    {
      double value;
      value = POPn;
      value = sin(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_cos)
{
    dSP; dTARGET; tryAMAGICun(cos);
    {
      double value;
      value = POPn;
      value = cos(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_rand)
{
    dSP; dTARGET;
    double value;
    if (MAXARG < 1)
	value = 1.0;
    else
	value = POPn;
    if (value == 0.0)
	value = 1.0;
#if RANDBITS == 31
    value = rand() * value / 2147483648.0;
#else
#if RANDBITS == 16
    value = rand() * value / 65536.0;
#else
#if RANDBITS == 15
    value = rand() * value / 32768.0;
#else
    value = rand() * value / (double)(((unsigned long)1) << RANDBITS);
#endif
#endif
#endif
    XPUSHn(value);
    RETURN;
}

PP(pp_srand)
{
    dSP;
    I32 anum;
    Time_t when;

    if (MAXARG < 1) {
	(void)time(&when);
	anum = when;
    }
    else
	anum = POPi;
    (void)srand(anum);
    EXTEND(SP, 1);
    RETPUSHYES;
}

PP(pp_exp)
{
    dSP; dTARGET; tryAMAGICun(exp);
    {
      double value;
      value = POPn;
      value = exp(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_log)
{
    dSP; dTARGET; tryAMAGICun(log);
    {
      double value;
      value = POPn;
      if (value <= 0.0)
	DIE("Can't take log of %g", value);
      value = log(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_sqrt)
{
    dSP; dTARGET; tryAMAGICun(sqrt);
    {
      double value;
      value = POPn;
      if (value < 0.0)
	DIE("Can't take sqrt of %g", value);
      value = sqrt(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_int)
{
    dSP; dTARGET;
    double value;
    value = POPn;
    if (value >= 0.0)
	(void)modf(value, &value);
    else {
	(void)modf(-value, &value);
	value = -value;
    }
    XPUSHn(value);
    RETURN;
}

PP(pp_abs)
{
    dSP; dTARGET; tryAMAGICun(abs);
    {
      double value;
      value = POPn;

      if (value < 0.0)
	value = -value;

      XPUSHn(value);
      RETURN;
    }
}

PP(pp_hex)
{
    dSP; dTARGET;
    char *tmps;
    I32 argtype;

    tmps = POPp;
    XPUSHi( scan_hex(tmps, 99, &argtype) );
    RETURN;
}

PP(pp_oct)
{
    dSP; dTARGET;
    I32 value;
    I32 argtype;
    char *tmps;

    tmps = POPp;
    while (*tmps && (isSPACE(*tmps) || *tmps == '0'))
	tmps++;
    if (*tmps == 'x')
	value = (I32)scan_hex(++tmps, 99, &argtype);
    else
	value = (I32)scan_oct(tmps, 99, &argtype);
    XPUSHi(value);
    RETURN;
}

/* String stuff. */

PP(pp_length)
{
    dSP; dTARGET;
    SETi( sv_len(TOPs) );
    RETURN;
}

PP(pp_substr)
{
    dSP; dTARGET;
    SV *sv;
    I32 len;
    STRLEN curlen;
    I32 pos;
    I32 rem;
    I32 lvalue = op->op_flags & OPf_MOD;
    char *tmps;
    I32 arybase = curcop->cop_arybase;

    if (MAXARG > 2)
	len = POPi;
    pos = POPi - arybase;
    sv = POPs;
    tmps = SvPV(sv, curlen);
    if (pos < 0)
	pos += curlen + arybase;
    if (pos < 0 || pos > curlen) {
	if (dowarn)
	    warn("substr outside of string");
	RETPUSHUNDEF;
    }
    else {
	if (MAXARG < 3)
	    len = curlen;
	else if (len < 0) {
	    len += curlen;
	    if (len < 0)
		len = 0;
	}
	tmps += pos;
	rem = curlen - pos;	/* rem=how many bytes left*/
	if (rem > len)
	    rem = len;
	sv_setpvn(TARG, tmps, rem);
	if (lvalue) {			/* it's an lvalue! */
	    LvTYPE(TARG) = 's';
	    LvTARG(TARG) = sv;
	    LvTARGOFF(TARG) = pos;
	    LvTARGLEN(TARG) = rem; 
	}
    }
    PUSHs(TARG);		/* avoid SvSETMAGIC here */
    RETURN;
}

PP(pp_vec)
{
    dSP; dTARGET;
    register I32 size = POPi;
    register I32 offset = POPi;
    register SV *src = POPs;
    I32 lvalue = op->op_flags & OPf_MOD;
    STRLEN srclen;
    unsigned char *s = (unsigned char*)SvPV(src, srclen);
    unsigned long retnum;
    I32 len;

    offset *= size;		/* turn into bit offset */
    len = (offset + size + 7) / 8;
    if (offset < 0 || size < 1)
	retnum = 0;
    else {
	if (lvalue) {                      /* it's an lvalue! */
	    LvTYPE(TARG) = 'v';
	    LvTARG(TARG) = src;
	    LvTARGOFF(TARG) = offset; 
	    LvTARGLEN(TARG) = size; 
	}
	if (len > srclen) {
	    if (size <= 8)
		retnum = 0;
	    else {
		offset >>= 3;
		if (size == 16)
		    retnum = (unsigned long) s[offset] << 8;
		else if (size == 32) {
		    if (offset < len) {
			if (offset + 1 < len)
			    retnum = ((unsigned long) s[offset] << 24) +
				((unsigned long) s[offset + 1] << 16) +
				(s[offset + 2] << 8);
			else
			    retnum = ((unsigned long) s[offset] << 24) +
				((unsigned long) s[offset + 1] << 16);
		    }
		    else
			retnum = (unsigned long) s[offset] << 24;
		}
	    }
	}
	else if (size < 8)
	    retnum = (s[offset >> 3] >> (offset & 7)) & ((1 << size) - 1);
	else {
	    offset >>= 3;
	    if (size == 8)
		retnum = s[offset];
	    else if (size == 16)
		retnum = ((unsigned long) s[offset] << 8) + s[offset+1];
	    else if (size == 32)
		retnum = ((unsigned long) s[offset] << 24) +
			((unsigned long) s[offset + 1] << 16) +
			(s[offset + 2] << 8) + s[offset+3];
	}
    }

    sv_setiv(TARG, (I32)retnum);
    PUSHs(TARG);
    RETURN;
}

PP(pp_index)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    STRLEN biglen;
    I32 arybase = curcop->cop_arybase;

    if (MAXARG < 3)
	offset = 0;
    else
	offset = POPi - arybase;
    little = POPs;
    big = POPs;
    tmps = SvPV(big, biglen);
    if (offset < 0)
	offset = 0;
    else if (offset > biglen)
	offset = biglen;
    if (!(tmps2 = fbm_instr((unsigned char*)tmps + offset,
      (unsigned char*)tmps + biglen, little)))
	retval = -1 + arybase;
    else
	retval = tmps2 - tmps + arybase;
    PUSHi(retval);
    RETURN;
}

PP(pp_rindex)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    STRLEN blen;
    STRLEN llen;
    SV *offstr;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    I32 arybase = curcop->cop_arybase;

    if (MAXARG >= 3)
	offstr = POPs;
    little = POPs;
    big = POPs;
    tmps2 = SvPV(little, llen);
    tmps = SvPV(big, blen);
    if (MAXARG < 3)
	offset = blen;
    else
	offset = SvIV(offstr) - arybase + llen;
    if (offset < 0)
	offset = 0;
    else if (offset > blen)
	offset = blen;
    if (!(tmps2 = rninstr(tmps,  tmps  + offset,
			  tmps2, tmps2 + llen)))
	retval = -1 + arybase;
    else
	retval = tmps2 - tmps + arybase;
    PUSHi(retval);
    RETURN;
}

PP(pp_sprintf)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    do_sprintf(TARG, SP-MARK, MARK+1);
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

static void
doparseform(sv)
SV *sv;
{
    STRLEN len;
    register char *s = SvPV_force(sv, len);
    register char *send = s + len;
    register char *base;
    register I32 skipspaces = 0;
    bool noblank;
    bool repeat;
    bool postspace = FALSE;
    U16 *fops;
    register U16 *fpc;
    U16 *linepc;
    register I32 arg;
    bool ischop;

    New(804, fops, (send - s)*3+2, U16);    /* Almost certainly too long... */
    fpc = fops;

    if (s < send) {
	linepc = fpc;
	*fpc++ = FF_LINEMARK;
	noblank = repeat = FALSE;
	base = s;
    }

    while (s <= send) {
	switch (*s++) {
	default:
	    skipspaces = 0;
	    continue;

	case '~':
	    if (*s == '~') {
		repeat = TRUE;
		*s = ' ';
	    }
	    noblank = TRUE;
	    s[-1] = ' ';
	    /* FALL THROUGH */
	case ' ': case '\t':
	    skipspaces++;
	    continue;
	    
	case '\n': case 0:
	    arg = s - base;
	    skipspaces++;
	    arg -= skipspaces;
	    if (arg) {
		if (postspace) {
		    *fpc++ = FF_SPACE;
		    postspace = FALSE;
		}
		*fpc++ = FF_LITERAL;
		*fpc++ = arg;
	    }
	    if (s <= send)
		skipspaces--;
	    if (skipspaces) {
		*fpc++ = FF_SKIP;
		*fpc++ = skipspaces;
	    }
	    skipspaces = 0;
	    if (s <= send)
		*fpc++ = FF_NEWLINE;
	    if (noblank) {
		*fpc++ = FF_BLANK;
		if (repeat)
		    arg = fpc - linepc + 1;
		else
		    arg = 0;
		*fpc++ = arg;
	    }
	    if (s < send) {
		linepc = fpc;
		*fpc++ = FF_LINEMARK;
		noblank = repeat = FALSE;
		base = s;
	    }
	    else
		s++;
	    continue;

	case '@':
	case '^':
	    ischop = s[-1] == '^';

	    if (postspace) {
		*fpc++ = FF_SPACE;
		postspace = FALSE;
	    }
	    arg = (s - base) - 1;
	    if (arg) {
		*fpc++ = FF_LITERAL;
		*fpc++ = arg;
	    }

	    base = s - 1;
	    *fpc++ = FF_FETCH;
	    if (*s == '*') {
		s++;
		*fpc++ = 0;
		*fpc++ = FF_LINEGLOB;
	    }
	    else if (*s == '#' || (*s == '.' && s[1] == '#')) {
		arg = ischop ? 512 : 0;
		base = s - 1;
		while (*s == '#')
		    s++;
		if (*s == '.') {
		    char *f;
		    s++;
		    f = s;
		    while (*s == '#')
			s++;
		    arg |= 256 + (s - f);
		}
		*fpc++ = s - base;		/* fieldsize for FETCH */
		*fpc++ = FF_DECIMAL;
		*fpc++ = arg;
	    }
	    else {
		I32 prespace = 0;
		bool ismore = FALSE;

		if (*s == '>') {
		    while (*++s == '>') ;
		    prespace = FF_SPACE;
		}
		else if (*s == '|') {
		    while (*++s == '|') ;
		    prespace = FF_HALFSPACE;
		    postspace = TRUE;
		}
		else {
		    if (*s == '<')
			while (*++s == '<') ;
		    postspace = TRUE;
		}
		if (*s == '.' && s[1] == '.' && s[2] == '.') {
		    s += 3;
		    ismore = TRUE;
		}
		*fpc++ = s - base;		/* fieldsize for FETCH */

		*fpc++ = ischop ? FF_CHECKCHOP : FF_CHECKNL;

		if (prespace)
		    *fpc++ = prespace;
		*fpc++ = FF_ITEM;
		if (ismore)
		    *fpc++ = FF_MORE;
		if (ischop)
		    *fpc++ = FF_CHOP;
	    }
	    base = s;
	    skipspaces = 0;
	    continue;
	}
    }
    *fpc++ = FF_END;

    arg = fpc - fops;
    { /* need to jump to the next word */
        int z;
	z = WORD_ALIGN - SvCUR(sv) % WORD_ALIGN;
	SvGROW(sv, SvCUR(sv) + z + arg * sizeof(U16) + 4);
	s = SvPVX(sv) + SvCUR(sv) + z;
    }
    Copy(fops, s, arg, U16);
    Safefree(fops);
}

PP(pp_formline)
{
    dSP; dMARK; dORIGMARK;
    register SV *form = *++MARK;
    register U16 *fpc;
    register char *t;
    register char *f;
    register char *s;
    register char *send;
    register I32 arg;
    register SV *sv;
    char *item;
    I32 itemsize;
    I32 fieldsize;
    I32 lines = 0;
    bool chopspace = (strchr(chopset, ' ') != Nullch);
    char *chophere;
    char *linemark;
    char *formmark;
    SV **markmark;
    double value;
    bool gotsome;
    STRLEN len;

    if (!SvCOMPILED(form)) {
	SvREADONLY_off(form);
	doparseform(form);
    }

    SvPV_force(formtarget, len);
    t = SvGROW(formtarget, len + SvCUR(form) + 1);  /* XXX SvCUR bad */
    t += len;
    f = SvPV(form, len);
    /* need to jump to the next word */
    s = f + len + WORD_ALIGN - SvCUR(form) % WORD_ALIGN;

    fpc = (U16*)s;

    for (;;) {
	DEBUG_f( {
	    char *name = "???";
	    arg = -1;
	    switch (*fpc) {
	    case FF_LITERAL:	arg = fpc[1]; name = "LITERAL";	break;
	    case FF_BLANK:	arg = fpc[1]; name = "BLANK";	break;
	    case FF_SKIP:	arg = fpc[1]; name = "SKIP";	break;
	    case FF_FETCH:	arg = fpc[1]; name = "FETCH";	break;
	    case FF_DECIMAL:	arg = fpc[1]; name = "DECIMAL";	break;

	    case FF_CHECKNL:	name = "CHECKNL";	break;
	    case FF_CHECKCHOP:	name = "CHECKCHOP";	break;
	    case FF_SPACE:	name = "SPACE";		break;
	    case FF_HALFSPACE:	name = "HALFSPACE";	break;
	    case FF_ITEM:	name = "ITEM";		break;
	    case FF_CHOP:	name = "CHOP";		break;
	    case FF_LINEGLOB:	name = "LINEGLOB";	break;
	    case FF_NEWLINE:	name = "NEWLINE";	break;
	    case FF_MORE:	name = "MORE";		break;
	    case FF_LINEMARK:	name = "LINEMARK";	break;
	    case FF_END:	name = "END";		break;
	    }
	    if (arg >= 0)
		fprintf(stderr, "%-16s%ld\n", name, (long) arg);
	    else
		fprintf(stderr, "%-16s\n", name);
	} )
	switch (*fpc++) {
	case FF_LINEMARK:
	    linemark = t;
	    formmark = f;
	    markmark = MARK;
	    lines++;
	    gotsome = FALSE;
	    break;

	case FF_LITERAL:
	    arg = *fpc++;
	    while (arg--)
		*t++ = *f++;
	    break;

	case FF_SKIP:
	    f += *fpc++;
	    break;

	case FF_FETCH:
	    arg = *fpc++;
	    f += arg;
	    fieldsize = arg;

	    if (MARK < SP)
		sv = *++MARK;
	    else {
		sv = &sv_no;
		if (dowarn)
		    warn("Not enough format arguments");
	    }
	    break;

	case FF_CHECKNL:
	    item = s = SvPV(sv, len);
	    itemsize = len;
	    if (itemsize > fieldsize)
		itemsize = fieldsize;
	    send = chophere = s + itemsize;
	    while (s < send) {
		if (*s & ~31)
		    gotsome = TRUE;
		else if (*s == '\n')
		    break;
		s++;
	    }
	    itemsize = s - item;
	    break;

	case FF_CHECKCHOP:
	    item = s = SvPV(sv, len);
	    itemsize = len;
	    if (itemsize <= fieldsize) {
		send = chophere = s + itemsize;
		while (s < send) {
		    if (*s == '\r') {
			itemsize = s - item;
			break;
		    }
		    if (*s++ & ~31)
			gotsome = TRUE;
		}
	    }
	    else {
		itemsize = fieldsize;
		send = chophere = s + itemsize;
		while (s < send || (s == send && isSPACE(*s))) {
		    if (isSPACE(*s)) {
			if (chopspace)
			    chophere = s;
			if (*s == '\r')
			    break;
		    }
		    else {
			if (*s & ~31)
			    gotsome = TRUE;
			if (strchr(chopset, *s))
			    chophere = s + 1;
		    }
		    s++;
		}
		itemsize = chophere - item;
	    }
	    break;

	case FF_SPACE:
	    arg = fieldsize - itemsize;
	    if (arg) {
		fieldsize -= arg;
		while (arg-- > 0)
		    *t++ = ' ';
	    }
	    break;

	case FF_HALFSPACE:
	    arg = fieldsize - itemsize;
	    if (arg) {
		arg /= 2;
		fieldsize -= arg;
		while (arg-- > 0)
		    *t++ = ' ';
	    }
	    break;

	case FF_ITEM:
	    arg = itemsize;
	    s = item;
	    while (arg--) {
		if ((*t++ = *s++) < ' ')
		    t[-1] = ' ';
	    }
	    break;

	case FF_CHOP:
	    s = chophere;
	    if (chopspace) {
		while (*s && isSPACE(*s))
		    s++;
	    }
	    sv_chop(sv,s);
	    break;

	case FF_LINEGLOB:
	    item = s = SvPV(sv, len);
	    itemsize = len;
	    if (itemsize) {
		gotsome = TRUE;
		send = s + itemsize;
		while (s < send) {
		    if (*s++ == '\n') {
			if (s == send)
			    itemsize--;
			else
			    lines++;
		    }
		}
		SvCUR_set(formtarget, t - SvPVX(formtarget));
		sv_catpvn(formtarget, item, itemsize);
		SvGROW(formtarget, SvCUR(formtarget) + SvCUR(form) + 1);
		t = SvPVX(formtarget) + SvCUR(formtarget);
	    }
	    break;

	case FF_DECIMAL:
	    /* If the field is marked with ^ and the value is undefined,
	       blank it out. */
	    arg = *fpc++;
	    if ((arg & 512) && !SvOK(sv)) {
		arg = fieldsize;
		while (arg--)
		    *t++ = ' ';
		break;
	    }
	    gotsome = TRUE;
	    value = SvNV(sv);
	    if (arg & 256) {
		sprintf(t, "%#*.*f", (int) fieldsize, (int) arg & 255, value);
	    } else {
		sprintf(t, "%*.0f", (int) fieldsize, value);
	    }
	    t += fieldsize;
	    break;

	case FF_NEWLINE:
	    f++;
	    while (t-- > linemark && *t == ' ') ;
	    t++;
	    *t++ = '\n';
	    break;

	case FF_BLANK:
	    arg = *fpc++;
	    if (gotsome) {
		if (arg) {		/* repeat until fields exhausted? */
		    fpc -= arg;
		    f = formmark;
		    MARK = markmark;
		    if (lines == 200) {
			arg = t - linemark;
			if (strnEQ(linemark, linemark - arg, arg))
			    DIE("Runaway format");
		    }
		    arg = t - SvPVX(formtarget);
		    SvGROW(formtarget,
			(t - SvPVX(formtarget)) + (f - formmark) + 1);
		    t = SvPVX(formtarget) + arg;
		}
	    }
	    else {
		t = linemark;
		lines--;
	    }
	    break;

	case FF_MORE:
	    if (itemsize) {
		arg = fieldsize - itemsize;
		if (arg) {
		    fieldsize -= arg;
		    while (arg-- > 0)
			*t++ = ' ';
		}
		s = t - 3;
		if (strnEQ(s,"   ",3)) {
		    while (s > SvPVX(formtarget) && isSPACE(s[-1]))
			s--;
		}
		*s++ = '.';
		*s++ = '.';
		*s++ = '.';
	    }
	    break;

	case FF_END:
	    *t = '\0';
	    SvCUR_set(formtarget, t - SvPVX(formtarget));
	    FmLINES(formtarget) += lines;
	    SP = ORIGMARK;
	    RETPUSHYES;
	}
    }
}

PP(pp_ord)
{
    dSP; dTARGET;
    I32 value;
    char *tmps;

#ifndef I286
    tmps = POPp;
    value = (I32) (*tmps & 255);
#else
    I32 anum;
    tmps = POPp;
    anum = (I32) *tmps;
    value = (I32) (anum & 255);
#endif
    XPUSHi(value);
    RETURN;
}

PP(pp_chr)
{
    dSP; dTARGET;
    char *tmps;

    if (!SvPOK(TARG)) {
	SvUPGRADE(TARG,SVt_PV);
	SvGROW(TARG,1);
    }
    SvCUR_set(TARG, 1);
    tmps = SvPVX(TARG);
    *tmps = POPi;
    SvPOK_only(TARG);
    XPUSHs(TARG);
    RETURN;
}

PP(pp_crypt)
{
    dSP; dTARGET; dPOPTOPssrl;
#ifdef HAS_CRYPT
    char *tmps = SvPV(left, na);
#ifdef FCRYPT
    sv_setpv(TARG, fcrypt(tmps, SvPV(right, na)));
#else
    sv_setpv(TARG, crypt(tmps, SvPV(right, na)));
#endif
#else
    DIE(
      "The crypt() function is unimplemented due to excessive paranoia.");
#endif
    SETs(TARG);
    RETURN;
}

PP(pp_ucfirst)
{
    dSP;
    SV *sv = TOPs;
    register char *s;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }
    s = SvPV_force(sv, na);
    if (isLOWER(*s))
	*s = toUPPER(*s);

    RETURN;
}

PP(pp_lcfirst)
{
    dSP;
    SV *sv = TOPs;
    register char *s;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }
    s = SvPV_force(sv, na);
    if (isUPPER(*s))
	*s = toLOWER(*s);

    SETs(sv);
    RETURN;
}

PP(pp_uc)
{
    dSP;
    SV *sv = TOPs;
    register char *s;
    register char *send;
    STRLEN len;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }
    s = SvPV_force(sv, len);
    send = s + len;
    while (s < send) {
	if (isLOWER(*s))
	    *s = toUPPER(*s);
	s++;
    }
    RETURN;
}

PP(pp_lc)
{
    dSP;
    SV *sv = TOPs;
    register char *s;
    register char *send;
    STRLEN len;

    if (!SvPADTMP(sv)) {
	dTARGET;
	sv_setsv(TARG, sv);
	sv = TARG;
	SETs(sv);
    }
    s = SvPV_force(sv, len);
    send = s + len;
    while (s < send) {
	if (isUPPER(*s))
	    *s = toLOWER(*s);
	s++;
    }
    RETURN;
}

PP(pp_quotemeta)
{
    dSP; dTARGET;
    SV *sv = TOPs;
    STRLEN len;
    register char *s = SvPV(sv,len);
    register char *d;

    if (len) {
	SvUPGRADE(TARG, SVt_PV);
	SvGROW(TARG, len * 2);
	d = SvPVX(TARG);
	while (len--) {
	    if (!isALNUM(*s))
		*d++ = '\\';
	    *d++ = *s++;
	}
	*d = '\0';
	SvCUR_set(TARG, d - SvPVX(TARG));
	SvPOK_only(TARG);
    }
    else
	sv_setpvn(TARG, s, len);
    SETs(TARG);
    RETURN;
}

/* Arrays. */

PP(pp_rv2av)
{
    dSP; dPOPss;

    AV *av;

    if (SvROK(sv)) {
      wasref:
	av = (AV*)SvRV(sv);
	if (SvTYPE(av) != SVt_PVAV)
	    DIE("Not an ARRAY reference");
	if (op->op_private & OPpLVAL_INTRO)
	    av = (AV*)save_svref((SV**)sv);
	if (op->op_flags & OPf_REF) {
	    PUSHs((SV*)av);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVAV) {
	    av = (AV*)sv;
	    if (op->op_flags & OPf_REF) {
		PUSHs((SV*)av);
		RETURN;
	    }
	}
	else {
	    if (SvTYPE(sv) != SVt_PVGV) {
		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (op->op_flags & OPf_REF ||
		      op->op_private & HINT_STRICT_REFS)
			DIE(no_usym, "an ARRAY");
		    RETPUSHUNDEF;
		}
		if (op->op_private & HINT_STRICT_REFS)
		    DIE(no_symref, "an ARRAY");
		sv = (SV*)gv_fetchpv(SvPV(sv, na), TRUE, SVt_PVAV);
	    }
	    av = GvAVn(sv);
	    if (op->op_private & OPpLVAL_INTRO)
		av = save_ary(sv);
	    if (op->op_flags & OPf_REF) {
		PUSHs((SV*)av);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL(av) + 1;
	EXTEND(SP, maxarg);
	Copy(AvARRAY(av), SP+1, maxarg, SV*);
	SP += maxarg;
    }
    else {
	dTARGET;
	I32 maxarg = AvFILL(av) + 1;
	PUSHi(maxarg);
    }
    RETURN;
}

PP(pp_aelemfast)
{
    dSP;
    AV *av = GvAV((GV*)cSVOP->op_sv);
    SV** svp = av_fetch(av, op->op_private, op->op_flags & OPf_MOD);
    PUSHs(svp ? *svp : &sv_undef);
    RETURN;
}

PP(pp_aelem)
{
    dSP;
    SV** svp;
    I32 elem = POPi - curcop->cop_arybase;
    AV *av = (AV*)POPs;
    I32 lval = op->op_flags & OPf_MOD;

    if (SvTYPE(av) != SVt_PVAV)
	RETPUSHUNDEF;
    svp = av_fetch(av, elem, lval);
    if (lval) {
	if (!svp || *svp == &sv_undef)
	    DIE(no_aelem, elem);
	if (op->op_private & OPpLVAL_INTRO)
	    save_svref(svp);
	else if (op->op_private & (OPpDEREF_HV|OPpDEREF_AV)) {
	    SV* sv = *svp;
	    if (SvGMAGICAL(sv))
		mg_get(sv);
	    if (!SvOK(sv)) {
		SvUPGRADE(sv, SVt_RV);
		SvRV(sv) = (op->op_private & OPpDEREF_HV ?
			    (SV*)newHV() : (SV*)newAV());
		SvROK_on(sv);
		SvSETMAGIC(sv);
	    }
	}
    }
    PUSHs(svp ? *svp : &sv_undef);
    RETURN;
}

PP(pp_aslice)
{
    dSP; dMARK; dORIGMARK;
    register SV** svp;
    register AV* av = (AV*)POPs;
    register I32 lval = op->op_flags & OPf_MOD;

    if (SvTYPE(av) == SVt_PVAV) {
	while (++MARK <= SP) {
	    I32 elem = SvIVx(*MARK);

	    svp = av_fetch(av, elem, lval);
	    if (lval) {
		if (!svp || *svp == &sv_undef)
		    DIE(no_aelem, elem);
		if (op->op_private & OPpLVAL_INTRO)
		    save_svref(svp);
	    }
	    *MARK = svp ? *svp : &sv_undef;
	}
    }
    else if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* Associative arrays. */

PP(pp_each)
{
    dSP; dTARGET;
    HV *hash = (HV*)POPs;
    HE *entry = hv_iternext(hash);
    I32 i;
    char *tmps;

    EXTEND(SP, 2);
    if (entry) {
	tmps = hv_iterkey(entry, &i);
	if (!i)
	    tmps = "";
	PUSHs(sv_2mortal(newSVpv(tmps, i)));
	if (GIMME == G_ARRAY) {
	    sv_setsv(TARG, hv_iterval(hash, entry));
	    PUSHs(TARG);
	}
    }
    else if (GIMME == G_SCALAR)
	RETPUSHUNDEF;

    RETURN;
}

PP(pp_values)
{
    return do_kv(ARGS);
}

PP(pp_keys)
{
    return do_kv(ARGS);
}

PP(pp_delete)
{
    dSP;
    SV *sv;
    SV *tmpsv = POPs;
    HV *hv = (HV*)POPs;
    char *tmps;
    STRLEN len;
    if (SvTYPE(hv) != SVt_PVHV) {
	DIE("Not a HASH reference");
    }
    tmps = SvPV(tmpsv, len);
    sv = hv_delete(hv, tmps, len);
    if (!sv)
	RETPUSHUNDEF;
    PUSHs(sv);
    RETURN;
}

PP(pp_exists)
{
    dSP;
    SV *sv;
    SV *tmpsv = POPs;
    HV *hv = (HV*)POPs;
    char *tmps;
    STRLEN len;
    if (SvTYPE(hv) != SVt_PVHV) {
	DIE("Not a HASH reference");
    }
    tmps = SvPV(tmpsv, len);
    if (hv_exists(hv, tmps, len))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_rv2hv)
{

    dSP; dTOPss;

    HV *hv;

    if (SvROK(sv)) {
      wasref:
	hv = (HV*)SvRV(sv);
	if (SvTYPE(hv) != SVt_PVHV)
	    DIE("Not a HASH reference");
	if (op->op_private & OPpLVAL_INTRO)
	    hv = (HV*)save_svref((SV**)sv);
	if (op->op_flags & OPf_REF) {
	    SETs((SV*)hv);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVHV) {
	    hv = (HV*)sv;
	    if (op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	}
	else {
	    if (SvTYPE(sv) != SVt_PVGV) {
		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (op->op_flags & OPf_REF ||
		      op->op_private & HINT_STRICT_REFS)
			DIE(no_usym, "a HASH");
		    RETSETUNDEF;
		}
		if (op->op_private & HINT_STRICT_REFS)
		    DIE(no_symref, "a HASH");
		sv = (SV*)gv_fetchpv(SvPV(sv, na), TRUE, SVt_PVHV);
	    }
	    hv = GvHVn(sv);
	    if (op->op_private & OPpLVAL_INTRO)
		hv = save_hash(sv);
	    if (op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) { /* array wanted */
	*stack_sp = (SV*)hv;
	return do_kv(ARGS);
    }
    else {
	dTARGET;
	if (HvFILL(hv)) {
	    sprintf(buf, "%d/%d", HvFILL(hv), HvMAX(hv)+1);
	    sv_setpv(TARG, buf);
	}
	else
	    sv_setiv(TARG, 0);
	SETTARG;
	RETURN;
    }
}

PP(pp_helem)
{
    dSP;
    SV** svp;
    SV *keysv = POPs;
    STRLEN keylen;
    char *key = SvPV(keysv, keylen);
    HV *hv = (HV*)POPs;
    I32 lval = op->op_flags & OPf_MOD;

    if (SvTYPE(hv) != SVt_PVHV)
	RETPUSHUNDEF;
    svp = hv_fetch(hv, key, keylen, lval);
    if (lval) {
	if (!svp || *svp == &sv_undef)
	    DIE(no_helem, key);
	if (op->op_private & OPpLVAL_INTRO)
	    save_svref(svp);
	else if (op->op_private & (OPpDEREF_HV|OPpDEREF_AV)) {
	    SV* sv = *svp;
	    if (SvGMAGICAL(sv))
		mg_get(sv);
	    if (!SvOK(sv)) {
		SvUPGRADE(sv, SVt_RV);
		SvRV(sv) = (op->op_private & OPpDEREF_HV ?
			    (SV*)newHV() : (SV*)newAV());
		SvROK_on(sv);
		SvSETMAGIC(sv);
	    }
	}
    }
    PUSHs(svp ? *svp : &sv_undef);
    RETURN;
}

PP(pp_hslice)
{
    dSP; dMARK; dORIGMARK;
    register SV **svp;
    register HV *hv = (HV*)POPs;
    register I32 lval = op->op_flags & OPf_MOD;

    if (SvTYPE(hv) == SVt_PVHV) {
	while (++MARK <= SP) {
	    STRLEN keylen;
	    char *key = SvPV(*MARK, keylen);

	    svp = hv_fetch(hv, key, keylen, lval);
	    if (lval) {
		if (!svp || *svp == &sv_undef)
		    DIE(no_helem, key);
		if (op->op_private & OPpLVAL_INTRO)
		    save_svref(svp);
	    }
	    *MARK = svp ? *svp : &sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* Explosives and implosives. */

PP(pp_unpack)
{
    dSP;
    dPOPPOPssrl;
    SV *sv;
    STRLEN llen;
    STRLEN rlen;
    register char *pat = SvPV(left, llen);
    register char *s = SvPV(right, rlen);
    char *strend = s + rlen;
    char *strbeg = s;
    register char *patend = pat + llen;
    I32 datumtype;
    register I32 len;
    register I32 bits;

    /* These must not be in registers: */
    I16 ashort;
    int aint;
    I32 along;
#ifdef QUAD
    quad aquad;
#endif
    U16 aushort;
    unsigned int auint;
    U32 aulong;
#ifdef QUAD
    unsigned quad auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    I32 checksum = 0;
    register U32 culong;
    double cdouble;
    static char* bitcount = 0;

    if (GIMME != G_ARRAY) {		/* arrange to do first one only */
	/*SUPPRESS 530*/
	for (patend = pat; !isALPHA(*patend) || *patend == 'x'; patend++) ;
	if (strchr("aAbBhH", *patend) || *pat == '%') {
	    patend++;
	    while (isDIGIT(*patend) || *patend == '*')
		patend++;
	}
	else
	    patend++;
    }
    while (pat < patend) {
      reparse:
	datumtype = *pat++;
	if (pat >= patend)
	    len = 1;
	else if (*pat == '*') {
	    len = strend - strbeg;	/* long enough */
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat))
		len = (len * 10) + (*pat++ - '0');
	}
	else
	    len = (datumtype != '@');
	switch(datumtype) {
	default:
	    break;
	case '%':
	    if (len == 1 && pat[-1] != '1')
		len = 16;
	    checksum = len;
	    culong = 0;
	    cdouble = 0;
	    if (pat < patend)
		goto reparse;
	    break;
	case '@':
	    if (len > strend - strbeg)
		DIE("@ outside of string");
	    s = strbeg + len;
	    break;
	case 'X':
	    if (len > s - strbeg)
		DIE("X outside of string");
	    s -= len;
	    break;
	case 'x':
	    if (len > strend - s)
		DIE("x outside of string");
	    s += len;
	    break;
	case 'A':
	case 'a':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum)
		goto uchar_checksum;
	    sv = NEWSV(35, len);
	    sv_setpvn(sv, s, len);
	    s += len;
	    if (datumtype == 'A') {
		aptr = s;	/* borrow register */
		s = SvPVX(sv) + len - 1;
		while (s >= SvPVX(sv) && (!*s || isSPACE(*s)))
		    s--;
		*++s = '\0';
		SvCUR_set(sv, s - SvPVX(sv));
		s = aptr;	/* unborrow register */
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'B':
	case 'b':
	    if (pat[-1] == '*' || len > (strend - s) * 8)
		len = (strend - s) * 8;
	    if (checksum) {
		if (!bitcount) {
		    Newz(601, bitcount, 256, char);
		    for (bits = 1; bits < 256; bits++) {
			if (bits & 1)	bitcount[bits]++;
			if (bits & 2)	bitcount[bits]++;
			if (bits & 4)	bitcount[bits]++;
			if (bits & 8)	bitcount[bits]++;
			if (bits & 16)	bitcount[bits]++;
			if (bits & 32)	bitcount[bits]++;
			if (bits & 64)	bitcount[bits]++;
			if (bits & 128)	bitcount[bits]++;
		    }
		}
		while (len >= 8) {
		    culong += bitcount[*(unsigned char*)s++];
		    len -= 8;
		}
		if (len) {
		    bits = *s;
		    if (datumtype == 'b') {
			while (len-- > 0) {
			    if (bits & 1) culong++;
			    bits >>= 1;
			}
		    }
		    else {
			while (len-- > 0) {
			    if (bits & 128) culong++;
			    bits <<= 1;
			}
		    }
		}
		break;
	    }
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    aptr = pat;			/* borrow register */
	    pat = SvPVX(sv);
	    if (datumtype == 'b') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)		/*SUPPRESS 595*/
			bits >>= 1;
		    else
			bits = *s++;
		    *pat++ = '0' + (bits & 1);
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)
			bits <<= 1;
		    else
			bits = *s++;
		    *pat++ = '0' + ((bits & 128) != 0);
		}
	    }
	    *pat = '\0';
	    pat = aptr;			/* unborrow register */
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'H':
	case 'h':
	    if (pat[-1] == '*' || len > (strend - s) * 2)
		len = (strend - s) * 2;
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    aptr = pat;			/* borrow register */
	    pat = SvPVX(sv);
	    if (datumtype == 'h') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits >>= 4;
		    else
			bits = *s++;
		    *pat++ = hexdigit[bits & 15];
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits <<= 4;
		    else
			bits = *s++;
		    *pat++ = hexdigit[(bits >> 4) & 15];
		}
	    }
	    *pat = '\0';
	    pat = aptr;			/* unborrow register */
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'c':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    culong += aint;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    sv = NEWSV(36, 0);
		    sv_setiv(sv, (I32)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'C':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
	      uchar_checksum:
		while (len-- > 0) {
		    auint = *s++ & 255;
		    culong += auint;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    auint = *s++ & 255;
		    sv = NEWSV(37, 0);
		    sv_setiv(sv, (I32)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 's':
	    along = (strend - s) / sizeof(I16);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &ashort, 1, I16);
		    s += sizeof(I16);
		    culong += ashort;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &ashort, 1, I16);
		    s += sizeof(I16);
		    sv = NEWSV(38, 0);
		    sv_setiv(sv, (I32)ashort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'v':
	case 'n':
	case 'S':
	    along = (strend - s) / sizeof(U16);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aushort, 1, U16);
		    s += sizeof(U16);
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    culong += aushort;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &aushort, 1, U16);
		    s += sizeof(U16);
		    sv = NEWSV(39, 0);
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    sv_setiv(sv, (I32)aushort);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'i':
	    along = (strend - s) / sizeof(int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    if (checksum > 32)
			cdouble += (double)aint;
		    else
			culong += aint;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    sv = NEWSV(40, 0);
		    sv_setiv(sv, (I32)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'I':
	    along = (strend - s) / sizeof(unsigned int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    if (checksum > 32)
			cdouble += (double)auint;
		    else
			culong += auint;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    sv = NEWSV(41, 0);
		    sv_setiv(sv, (I32)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'l':
	    along = (strend - s) / sizeof(I32);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &along, 1, I32);
		    s += sizeof(I32);
		    if (checksum > 32)
			cdouble += (double)along;
		    else
			culong += along;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &along, 1, I32);
		    s += sizeof(I32);
		    sv = NEWSV(42, 0);
		    sv_setiv(sv, (I32)along);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'V':
	case 'N':
	case 'L':
	    along = (strend - s) / sizeof(U32);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aulong, 1, U32);
		    s += sizeof(U32);
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    if (checksum > 32)
			cdouble += (double)aulong;
		    else
			culong += aulong;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &aulong, 1, U32);
		    s += sizeof(U32);
		    sv = NEWSV(43, 0);
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    sv_setnv(sv, (double)aulong);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'p':
	    along = (strend - s) / sizeof(char*);
	    if (len > along)
		len = along;
	    EXTEND(SP, len);
	    while (len-- > 0) {
		if (sizeof(char*) > strend - s)
		    break;
		else {
		    Copy(s, &aptr, 1, char*);
		    s += sizeof(char*);
		}
		sv = NEWSV(44, 0);
		if (aptr)
		    sv_setpv(sv, aptr);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'P':
	    EXTEND(SP, 1);
	    if (sizeof(char*) > strend - s)
		break;
	    else {
		Copy(s, &aptr, 1, char*);
		s += sizeof(char*);
	    }
	    sv = NEWSV(44, 0);
	    if (aptr)
		sv_setpvn(sv, aptr, len);
	    PUSHs(sv_2mortal(sv));
	    break;
#ifdef QUAD
	case 'q':
	    EXTEND(SP, len);
	    while (len-- > 0) {
		if (s + sizeof(quad) > strend)
		    aquad = 0;
		else {
		    Copy(s, &aquad, 1, quad);
		    s += sizeof(quad);
		}
		sv = NEWSV(42, 0);
		sv_setiv(sv, (IV)aquad);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'Q':
	    EXTEND(SP, len);
	    while (len-- > 0) {
		if (s + sizeof(unsigned quad) > strend)
		    auquad = 0;
		else {
		    Copy(s, &auquad, 1, unsigned quad);
		    s += sizeof(unsigned quad);
		}
		sv = NEWSV(43, 0);
		sv_setiv(sv, (IV)auquad);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
#endif
	/* float and double added gnb@melba.bby.oz.au 22/11/89 */
	case 'f':
	case 'F':
	    along = (strend - s) / sizeof(float);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    cdouble += afloat;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    sv = NEWSV(47, 0);
		    sv_setnv(sv, (double)afloat);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'd':
	case 'D':
	    along = (strend - s) / sizeof(double);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    cdouble += adouble;
		}
	    }
	    else {
		EXTEND(SP, len);
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    sv = NEWSV(48, 0);
		    sv_setnv(sv, (double)adouble);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'u':
	    along = (strend - s) * 3 / 4;
	    sv = NEWSV(42, along);
	    while (s < strend && *s > ' ' && *s < 'a') {
		I32 a, b, c, d;
		char hunk[4];

		hunk[3] = '\0';
		len = (*s++ - ' ') & 077;
		while (len > 0) {
		    if (s < strend && *s >= ' ')
			a = (*s++ - ' ') & 077;
		    else
			a = 0;
		    if (s < strend && *s >= ' ')
			b = (*s++ - ' ') & 077;
		    else
			b = 0;
		    if (s < strend && *s >= ' ')
			c = (*s++ - ' ') & 077;
		    else
			c = 0;
		    if (s < strend && *s >= ' ')
			d = (*s++ - ' ') & 077;
		    else
			d = 0;
		    hunk[0] = a << 2 | b >> 4;
		    hunk[1] = b << 4 | c >> 2;
		    hunk[2] = c << 6 | d;
		    sv_catpvn(sv, hunk, len > 3 ? 3 : len);
		    len -= 3;
		}
		if (*s == '\n')
		    s++;
		else if (s[1] == '\n')		/* possible checksum byte */
		    s += 2;
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	}
	if (checksum) {
	    sv = NEWSV(42, 0);
	    if (strchr("fFdD", datumtype) ||
	      (checksum > 32 && strchr("iIlLN", datumtype)) ) {
		double trouble;

		adouble = 1.0;
		while (checksum >= 16) {
		    checksum -= 16;
		    adouble *= 65536.0;
		}
		while (checksum >= 4) {
		    checksum -= 4;
		    adouble *= 16.0;
		}
		while (checksum--)
		    adouble *= 2.0;
		along = (1 << checksum) - 1;
		while (cdouble < 0.0)
		    cdouble += adouble;
		cdouble = modf(cdouble / adouble, &trouble) * adouble;
		sv_setnv(sv, cdouble);
	    }
	    else {
		if (checksum < 32) {
		    along = (1 << checksum) - 1;
		    culong &= (U32)along;
		}
		sv_setnv(sv, (double)culong);
	    }
	    XPUSHs(sv_2mortal(sv));
	    checksum = 0;
	}
    }
    RETURN;
}

static void
doencodes(sv, s, len)
register SV *sv;
register char *s;
register I32 len;
{
    char hunk[5];

    *hunk = len + ' ';
    sv_catpvn(sv, hunk, 1);
    hunk[4] = '\0';
    while (len > 0) {
	hunk[0] = ' ' + (077 & (*s >> 2));
	hunk[1] = ' ' + (077 & ((*s << 4) & 060 | (s[1] >> 4) & 017));
	hunk[2] = ' ' + (077 & ((s[1] << 2) & 074 | (s[2] >> 6) & 03));
	hunk[3] = ' ' + (077 & (s[2] & 077));
	sv_catpvn(sv, hunk, 4);
	s += 3;
	len -= 3;
    }
    for (s = SvPVX(sv); *s; s++) {
	if (*s == ' ')
	    *s = '`';
    }
    sv_catpvn(sv, "\n", 1);
}

PP(pp_pack)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register SV *cat = TARG;
    register I32 items;
    STRLEN fromlen;
    register char *pat = SvPVx(*++MARK, fromlen);
    register char *patend = pat + fromlen;
    register I32 len;
    I32 datumtype;
    SV *fromstr;
    /*SUPPRESS 442*/
    static char null10[] = {0,0,0,0,0,0,0,0,0,0};
    static char *space10 = "          ";

    /* These must not be in registers: */
    char achar;
    I16 ashort;
    int aint;
    unsigned int auint;
    I32 along;
    U32 aulong;
#ifdef QUAD
    quad aquad;
    unsigned quad auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;

    items = SP - MARK;
    MARK++;
    sv_setpvn(cat, "", 0);
    while (pat < patend) {
#define NEXTFROM (items-- > 0 ? *MARK++ : &sv_no)
	datumtype = *pat++;
	if (*pat == '*') {
	    len = strchr("@Xxu", datumtype) ? 0 : items;
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat))
		len = (len * 10) + (*pat++ - '0');
	}
	else
	    len = 1;
	switch(datumtype) {
	default:
	    break;
	case '%':
	    DIE("%% may only be used in unpack");
	case '@':
	    len -= SvCUR(cat);
	    if (len > 0)
		goto grow;
	    len = -len;
	    if (len > 0)
		goto shrink;
	    break;
	case 'X':
	  shrink:
	    if (SvCUR(cat) < len)
		DIE("X outside of string");
	    SvCUR(cat) -= len;
	    *SvEND(cat) = '\0';
	    break;
	case 'x':
	  grow:
	    while (len >= 10) {
		sv_catpvn(cat, null10, 10);
		len -= 10;
	    }
	    sv_catpvn(cat, null10, len);
	    break;
	case 'A':
	case 'a':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    if (pat[-1] == '*')
		len = fromlen;
	    if (fromlen > len)
		sv_catpvn(cat, aptr, len);
	    else {
		sv_catpvn(cat, aptr, fromlen);
		len -= fromlen;
		if (datumtype == 'A') {
		    while (len >= 10) {
			sv_catpvn(cat, space10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, space10, len);
		}
		else {
		    while (len >= 10) {
			sv_catpvn(cat, null10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, null10, len);
		}
	    }
	    break;
	case 'B':
	case 'b':
	    {
		char *savepat = pat;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		aptr = SvPV(fromstr, fromlen);
		if (pat[-1] == '*')
		    len = fromlen;
		pat = aptr;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+7)/8;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'B') {
		    for (len = 0; len++ < aint;) {
			items |= *pat++ & 1;
			if (len & 7)
			    items <<= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (*pat++ & 1)
			    items |= 128;
			if (len & 7)
			    items >>= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 7) {
		    if (datumtype == 'B')
			items <<= 7 - (aint & 7);
		    else
			items >>= 7 - (aint & 7);
		    *aptr++ = items & 0xff;
		}
		pat = SvPVX(cat) + SvCUR(cat);
		while (aptr <= pat)
		    *aptr++ = '\0';

		pat = savepat;
		items = saveitems;
	    }
	    break;
	case 'H':
	case 'h':
	    {
		char *savepat = pat;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		aptr = SvPV(fromstr, fromlen);
		if (pat[-1] == '*')
		    len = fromlen;
		pat = aptr;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+1)/2;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'H') {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*pat))
			    items |= ((*pat++ & 15) + 9) & 15;
			else
			    items |= *pat++ & 15;
			if (len & 1)
			    items <<= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*pat))
			    items |= (((*pat++ & 15) + 9) & 15) << 4;
			else
			    items |= (*pat++ & 15) << 4;
			if (len & 1)
			    items >>= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 1)
		    *aptr++ = items & 0xff;
		pat = SvPVX(cat) + SvCUR(cat);
		while (aptr <= pat)
		    *aptr++ = '\0';

		pat = savepat;
		items = saveitems;
	    }
	    break;
	case 'C':
	case 'c':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		achar = aint;
		sv_catpvn(cat, &achar, sizeof(char));
	    }
	    break;
	/* Float and double added by gnb@melba.bby.oz.au  22/11/89 */
	case 'f':
	case 'F':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		afloat = (float)SvNV(fromstr);
		sv_catpvn(cat, (char *)&afloat, sizeof (float));
	    }
	    break;
	case 'd':
	case 'D':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		adouble = (double)SvNV(fromstr);
		sv_catpvn(cat, (char *)&adouble, sizeof (double));
	    }
	    break;
	case 'n':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTONS
		ashort = htons(ashort);
#endif
		sv_catpvn(cat, (char*)&ashort, sizeof(I16));
	    }
	    break;
	case 'v':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTOVS
		ashort = htovs(ashort);
#endif
		sv_catpvn(cat, (char*)&ashort, sizeof(I16));
	    }
	    break;
	case 'S':
	case 's':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
		sv_catpvn(cat, (char*)&ashort, sizeof(I16));
	    }
	    break;
	case 'I':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = U_I(SvNV(fromstr));
		sv_catpvn(cat, (char*)&auint, sizeof(unsigned int));
	    }
	    break;
	case 'i':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		sv_catpvn(cat, (char*)&aint, sizeof(int));
	    }
	    break;
	case 'N':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = U_L(SvNV(fromstr));
#ifdef HAS_HTONL
		aulong = htonl(aulong);
#endif
		sv_catpvn(cat, (char*)&aulong, sizeof(U32));
	    }
	    break;
	case 'V':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = U_L(SvNV(fromstr));
#ifdef HAS_HTOVL
		aulong = htovl(aulong);
#endif
		sv_catpvn(cat, (char*)&aulong, sizeof(U32));
	    }
	    break;
	case 'L':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = U_L(SvNV(fromstr));
		sv_catpvn(cat, (char*)&aulong, sizeof(U32));
	    }
	    break;
	case 'l':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		along = SvIV(fromstr);
		sv_catpvn(cat, (char*)&along, sizeof(I32));
	    }
	    break;
#ifdef QUAD
	case 'Q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auquad = (unsigned quad)SvIV(fromstr);
		sv_catpvn(cat, (char*)&auquad, sizeof(unsigned quad));
	    }
	    break;
	case 'q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aquad = (quad)SvIV(fromstr);
		sv_catpvn(cat, (char*)&aquad, sizeof(quad));
	    }
	    break;
#endif /* QUAD */
	case 'P':
	    len = 1;		/* assume SV is correct length */
	    /* FALL THROUGH */
	case 'p':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aptr = SvPV_force(fromstr, na);	/* XXX Error if TEMP? */
		sv_catpvn(cat, (char*)&aptr, sizeof(char*));
	    }
	    break;
	case 'u':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    SvGROW(cat, fromlen * 4 / 3);
	    if (len <= 1)
		len = 45;
	    else
		len = len / 3 * 3;
	    while (fromlen > 0) {
		I32 todo;

		if (fromlen > len)
		    todo = len;
		else
		    todo = fromlen;
		doencodes(cat, aptr, todo);
		fromlen -= todo;
		aptr += todo;
	    }
	    break;
	}
    }
    SvSETMAGIC(cat);
    SP = ORIGMARK;
    PUSHs(cat);
    RETURN;
}
#undef NEXTFROM

PP(pp_split)
{
    dSP; dTARG;
    AV *ary;
    register I32 limit = POPi;			/* note, negative is forever */
    SV *sv = POPs;
    STRLEN len;
    register char *s = SvPV(sv, len);
    char *strend = s + len;
    register PMOP *pm = (PMOP*)POPs;
    register SV *dstr;
    register char *m;
    I32 iters = 0;
    I32 maxiters = (strend - s) + 10;
    I32 i;
    char *orig;
    I32 origlimit = limit;
    I32 realarray = 0;
    I32 base;
    AV *oldstack = stack;
    register REGEXP *rx = pm->op_pmregexp;
    I32 gimme = GIMME;

    if (!pm || !s)
	DIE("panic: do_split");
    if (pm->op_pmreplroot)
	ary = GvAVn((GV*)pm->op_pmreplroot);
    else if (gimme != G_ARRAY)
	ary = GvAVn(defgv);
    else
	ary = Nullav;
    if (ary && (gimme != G_ARRAY || (pm->op_pmflags & PMf_ONCE))) {
	realarray = 1;
	if (!AvREAL(ary)) {
	    AvREAL_on(ary);
	    for (i = AvFILL(ary); i >= 0; i--)
		AvARRAY(ary)[i] = &sv_undef;	/* don't free mere refs */
	}
	av_extend(ary,0);
	av_clear(ary);
	/* temporarily switch stacks */
	SWITCHSTACK(stack, ary);
    }
    base = SP - stack_base;
    orig = s;
    if (pm->op_pmflags & PMf_SKIPWHITE) {
	while (isSPACE(*s))
	    s++;
    }
    if (!limit)
	limit = maxiters + 2;
    if (pm->op_pmflags & PMf_WHITE) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && !isSPACE(*m); m++) ;
	    if (m >= strend)
		break;
	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (!realarray)
		sv_2mortal(dstr);
	    XPUSHs(dstr);
	    /*SUPPRESS 530*/
	    for (s = m + 1; s < strend && isSPACE(*s); s++) ;
	}
    }
    else if (strEQ("^", rx->precomp)) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && *m != '\n'; m++) ;
	    m++;
	    if (m >= strend)
		break;
	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (!realarray)
		sv_2mortal(dstr);
	    XPUSHs(dstr);
	    s = m;
	}
    }
    else if (pm->op_pmshort) {
	i = SvCUR(pm->op_pmshort);
	if (i == 1) {
	    I32 fold = (pm->op_pmflags & PMf_FOLD);
	    i = *SvPVX(pm->op_pmshort);
	    if (fold && isUPPER(i))
		i = toLOWER(i);
	    while (--limit) {
		if (fold) {
		    for ( m = s;
			  m < strend && *m != i &&
			    (!isUPPER(*m) || toLOWER(*m) != i);
			  m++)			/*SUPPRESS 530*/
			;
		}
		else				/*SUPPRESS 530*/
		    for (m = s; m < strend && *m != i; m++) ;
		if (m >= strend)
		    break;
		dstr = NEWSV(30, m-s);
		sv_setpvn(dstr, s, m-s);
		if (!realarray)
		    sv_2mortal(dstr);
		XPUSHs(dstr);
		s = m + 1;
	    }
	}
	else {
#ifndef lint
	    while (s < strend && --limit &&
	      (m=fbm_instr((unsigned char*)s, (unsigned char*)strend,
		    pm->op_pmshort)) )
#endif
	    {
		dstr = NEWSV(31, m-s);
		sv_setpvn(dstr, s, m-s);
		if (!realarray)
		    sv_2mortal(dstr);
		XPUSHs(dstr);
		s = m + i;
	    }
	}
    }
    else {
	maxiters += (strend - s) * rx->nparens;
	while (s < strend && --limit &&
	    regexec(rx, s, strend, orig, 1, Nullsv, TRUE) ) {
	    if (rx->subbase
	      && rx->subbase != orig) {
		m = s;
		s = orig;
		orig = rx->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0];
	    dstr = NEWSV(32, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (!realarray)
		sv_2mortal(dstr);
	    XPUSHs(dstr);
	    if (rx->nparens) {
		for (i = 1; i <= rx->nparens; i++) {
		    s = rx->startp[i];
		    m = rx->endp[i];
		    dstr = NEWSV(33, m-s);
		    sv_setpvn(dstr, s, m-s);
		    if (!realarray)
			sv_2mortal(dstr);
		    XPUSHs(dstr);
		}
	    }
	    s = rx->endp[0];
	}
    }
    iters = (SP - stack_base) - base;
    if (iters > maxiters)
	DIE("Split loop");
    
    /* keep field after final delim? */
    if (s < strend || (iters && origlimit)) {
	dstr = NEWSV(34, strend-s);
	sv_setpvn(dstr, s, strend-s);
	if (!realarray)
	    sv_2mortal(dstr);
	XPUSHs(dstr);
	iters++;
    }
    else if (!origlimit) {
	while (iters > 0 && SvCUR(TOPs) == 0)
	    iters--, SP--;
    }
    if (realarray) {
	SWITCHSTACK(ary, oldstack);
	if (gimme == G_ARRAY) {
	    EXTEND(SP, iters);
	    Copy(AvARRAY(ary), SP + 1, iters, SV*);
	    SP += iters;
	    RETURN;
	}
    }
    else {
	if (gimme == G_ARRAY)
	    RETURN;
    }
    if (iters || !pm->op_pmreplroot) {
	GETTARGET;
	PUSHi(iters);
	RETURN;
    }
    RETPUSHUNDEF;
}

PP(pp_join)
{
    dSP; dMARK; dTARGET;
    MARK++;
    do_join(TARG, *MARK, MARK, SP);
    SP = MARK;
    SETs(TARG);
    RETURN;
}

/* List operators. */

PP(pp_list)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;		/* unwanted list, return last item */
	else
	    *MARK = &sv_undef;
	SP = MARK;
    }
    RETURN;
}

PP(pp_lslice)
{
    dSP;
    SV **lastrelem = stack_sp;
    SV **lastlelem = stack_base + POPMARK;
    SV **firstlelem = stack_base + POPMARK + 1;
    register SV **firstrelem = lastlelem + 1;
    I32 arybase = curcop->cop_arybase;

    register I32 max = lastrelem - lastlelem;
    register SV **lelem;
    register I32 ix;

    if (GIMME != G_ARRAY) {
	ix = SvIVx(*lastlelem) - arybase;
	if (ix < 0 || ix >= max)
	    *firstlelem = &sv_undef;
	else
	    *firstlelem = firstrelem[ix];
	SP = firstlelem;
	RETURN;
    }

    if (max == 0) {
	SP = firstlelem - 1;
	RETURN;
    }

    for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
	ix = SvIVx(*lelem) - arybase;
	if (ix < 0) {
	    ix += max;
	    if (ix < 0)
		*lelem = &sv_undef;
	    else if (!(*lelem = firstrelem[ix]))
		*lelem = &sv_undef;
	}
	else if (ix >= max || !(*lelem = firstrelem[ix]))
	    *lelem = &sv_undef;
    }
    SP = lastlelem;
    RETURN;
}

PP(pp_anonlist)
{
    dSP; dMARK;
    I32 items = SP - MARK;
    SP = MARK;
    XPUSHs((SV*)sv_2mortal((SV*)av_make(items, MARK+1)));
    RETURN;
}

PP(pp_anonhash)
{
    dSP; dMARK; dORIGMARK;
    STRLEN len;
    HV* hv = (HV*)sv_2mortal((SV*)newHV());

    while (MARK < SP) {
	SV* key = *++MARK;
	char *tmps;
	SV *val = NEWSV(46, 0);
	if (MARK < SP)
	    sv_setsv(val, *++MARK);
	else
	    warn("Odd number of elements in hash list");
	tmps = SvPV(key,len);
	(void)hv_store(hv,tmps,len,val,0);
    }
    SP = ORIGMARK;
    XPUSHs((SV*)hv);
    RETURN;
}

PP(pp_splice)
{
    dSP; dMARK; dORIGMARK;
    register AV *ary = (AV*)*++MARK;
    register SV **src;
    register SV **dst;
    register I32 i;
    register I32 offset;
    register I32 length;
    I32 newlen;
    I32 after;
    I32 diff;
    SV **tmparyval = 0;

    SP++;

    if (++MARK < SP) {
	offset = SvIVx(*MARK);
	if (offset < 0)
	    offset += AvFILL(ary) + 1;
	else
	    offset -= curcop->cop_arybase;
	if (++MARK < SP) {
	    length = SvIVx(*MARK++);
	    if (length < 0)
		length = 0;
	}
	else
	    length = AvMAX(ary) + 1;		/* close enough to infinity */
    }
    else {
	offset = 0;
	length = AvMAX(ary) + 1;
    }
    if (offset < 0) {
	length += offset;
	offset = 0;
	if (length < 0)
	    length = 0;
    }
    if (offset > AvFILL(ary) + 1)
	offset = AvFILL(ary) + 1;
    after = AvFILL(ary) + 1 - (offset + length);
    if (after < 0) {				/* not that much array */
	length += after;			/* offset+length now in array */
	after = 0;
	if (!AvALLOC(ary))
	    av_extend(ary, 0);
    }

    /* At this point, MARK .. SP-1 is our new LIST */

    newlen = SP - MARK;
    diff = newlen - length;

    if (diff < 0) {				/* shrinking the area */
	if (newlen) {
	    New(451, tmparyval, newlen, SV*);	/* so remember insertion */
	    Copy(MARK, tmparyval, newlen, SV*);
	}

	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    MEXTEND(MARK, length);
	    Copy(AvARRAY(ary)+offset, MARK, length, SV*);
	    if (AvREAL(ary)) {
		for (i = length, dst = MARK; i; i--)
		    sv_2mortal(*dst++);	/* free them eventualy */
	    }
	    MARK += length - 1;
	}
	else {
	    *MARK = AvARRAY(ary)[offset+length-1];
	    if (AvREAL(ary)) {
		sv_2mortal(*MARK);
		for (i = length - 1, dst = &AvARRAY(ary)[offset]; i > 0; i--)
		    SvREFCNT_dec(*dst++);	/* free them now */
	    }
	}
	AvFILL(ary) += diff;

	/* pull up or down? */

	if (offset < after) {			/* easier to pull up */
	    if (offset) {			/* esp. if nothing to pull */
		src = &AvARRAY(ary)[offset-1];
		dst = src - diff;		/* diff is negative */
		for (i = offset; i > 0; i--)	/* can't trust Copy */
		    *dst-- = *src--;
	    }
	    dst = AvARRAY(ary);
	    SvPVX(ary) = (char*)(AvARRAY(ary) - diff); /* diff is negative */
	    AvMAX(ary) += diff;
	}
	else {
	    if (after) {			/* anything to pull down? */
		src = AvARRAY(ary) + offset + length;
		dst = src + diff;		/* diff is negative */
		Move(src, dst, after, SV*);
	    }
	    dst = &AvARRAY(ary)[AvFILL(ary)+1];
						/* avoid later double free */
	}
	i = -diff;
	while (i)
	    dst[--i] = &sv_undef;
	
	if (newlen) {
	    for (src = tmparyval, dst = AvARRAY(ary) + offset;
	      newlen; newlen--) {
		*dst = NEWSV(46, 0);
		sv_setsv(*dst++, *src++);
	    }
	    Safefree(tmparyval);
	}
    }
    else {					/* no, expanding (or same) */
	if (length) {
	    New(452, tmparyval, length, SV*);	/* so remember deletion */
	    Copy(AvARRAY(ary)+offset, tmparyval, length, SV*);
	}

	if (diff > 0) {				/* expanding */

	    /* push up or down? */

	    if (offset < after && diff <= AvARRAY(ary) - AvALLOC(ary)) {
		if (offset) {
		    src = AvARRAY(ary);
		    dst = src - diff;
		    Move(src, dst, offset, SV*);
		}
		SvPVX(ary) = (char*)(AvARRAY(ary) - diff);/* diff is positive */
		AvMAX(ary) += diff;
		AvFILL(ary) += diff;
	    }
	    else {
		if (AvFILL(ary) + diff >= AvMAX(ary))	/* oh, well */
		    av_extend(ary, AvFILL(ary) + diff);
		AvFILL(ary) += diff;

		if (after) {
		    dst = AvARRAY(ary) + AvFILL(ary);
		    src = dst - diff;
		    for (i = after; i; i--) {
			*dst-- = *src--;
		    }
		}
	    }
	}

	for (src = MARK, dst = AvARRAY(ary) + offset; newlen; newlen--) {
	    *dst = NEWSV(46, 0);
	    sv_setsv(*dst++, *src++);
	}
	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    if (length) {
		Copy(tmparyval, MARK, length, SV*);
		if (AvREAL(ary)) {
		    for (i = length, dst = MARK; i; i--)
			sv_2mortal(*dst++);	/* free them eventualy */
		}
		Safefree(tmparyval);
	    }
	    MARK += length - 1;
	}
	else if (length--) {
	    *MARK = tmparyval[length];
	    if (AvREAL(ary)) {
		sv_2mortal(*MARK);
		while (length-- > 0)
		    SvREFCNT_dec(tmparyval[length]);
	    }
	    Safefree(tmparyval);
	}
	else
	    *MARK = &sv_undef;
    }
    SP = MARK;
    RETURN;
}

PP(pp_push)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv = &sv_undef;

    for (++MARK; MARK <= SP; MARK++) {
	sv = NEWSV(51, 0);
	if (*MARK)
	    sv_setsv(sv, *MARK);
	av_push(ary, sv);
    }
    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_pop)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_pop(av);
    if (sv != &sv_undef && AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_shift)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_shift(av);
    EXTEND(SP, 1);
    if (!sv)
	RETPUSHUNDEF;
    if (sv != &sv_undef && AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_unshift)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv;
    register I32 i = 0;

    av_unshift(ary, SP - MARK);
    while (MARK < SP) {
	sv = NEWSV(27, 0);
	sv_setsv(sv, *++MARK);
	(void)av_store(ary, i++, sv);
    }

    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_grepstart)
{
    dSP;
    SV *src;

    if (stack_base + *markstack_ptr == sp) {
	POPMARK;
	if (GIMME != G_ARRAY)
	    XPUSHs(&sv_no);
	RETURNOP(op->op_next->op_next);
    }
    stack_sp = stack_base + *markstack_ptr + 1;
    pp_pushmark();				/* push dst */
    pp_pushmark();				/* push src */
    ENTER;					/* enter outer scope */

    SAVETMPS;
    SAVESPTR(GvSV(defgv));

    ENTER;					/* enter inner scope */
    SAVESPTR(curpm);

    src = stack_base[*markstack_ptr];
    SvTEMP_off(src);
    GvSV(defgv) = src;

    PUTBACK;
    if (op->op_type == OP_MAPSTART)
	pp_pushmark();				/* push top */
    return ((LOGOP*)op->op_next)->op_other;
}

PP(pp_grepwhile)
{
    dSP;

    if (SvTRUEx(POPs))
	stack_base[markstack_ptr[-1]++] = stack_base[*markstack_ptr];
    ++*markstack_ptr;
    LEAVE;					/* exit inner scope */

    /* All done yet? */
    if (stack_base + *markstack_ptr > sp) {
	I32 items;

	LEAVE;					/* exit outer scope */
	POPMARK;				/* pop src */
	items = --*markstack_ptr - markstack_ptr[-1];
	POPMARK;				/* pop dst */
	SP = stack_base + POPMARK;		/* pop original mark */
	if (GIMME != G_ARRAY) {
	    dTARGET;
	    XPUSHi(items);
	    RETURN;
	}
	SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER;					/* enter inner scope */
	SAVESPTR(curpm);

	src = stack_base[*markstack_ptr];
	SvTEMP_off(src);
	GvSV(defgv) = src;

	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_mapstart)
{
    DIE("panic: mapstart");	/* uses grepstart */
}

PP(pp_mapwhile)
{
    dSP;
    I32 diff = (sp - stack_base) - *markstack_ptr;
    I32 count;
    I32 shift;
    SV** src;
    SV** dst; 

    ++markstack_ptr[-1];
    if (diff) {
	if (diff > markstack_ptr[-1] - markstack_ptr[-2]) {
	    shift = diff - (markstack_ptr[-1] - markstack_ptr[-2]);
	    count = (sp - stack_base) - markstack_ptr[-1] + 2;
	    
	    EXTEND(sp,shift);
	    src = sp;
	    dst = (sp += shift);
	    markstack_ptr[-1] += shift;
	    *markstack_ptr += shift;
	    while (--count)
		*dst-- = *src--;
	}
	dst = stack_base + (markstack_ptr[-2] += diff) - 1; 
	++diff;
	while (--diff)
	    *dst-- = SvTEMP(TOPs) ? POPs : sv_mortalcopy(POPs); 
    }
    LEAVE;					/* exit inner scope */

    /* All done yet? */
    if (markstack_ptr[-1] > *markstack_ptr) {
	I32 items;

	POPMARK;				/* pop top */
	LEAVE;					/* exit outer scope */
	POPMARK;				/* pop src */
	items = --*markstack_ptr - markstack_ptr[-1];
	POPMARK;				/* pop dst */
	SP = stack_base + POPMARK;		/* pop original mark */
	if (GIMME != G_ARRAY) {
	    dTARGET;
	    XPUSHi(items);
	    RETURN;
	}
	SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER;					/* enter inner scope */
	SAVESPTR(curpm);

	src = stack_base[markstack_ptr[-1]];
	SvTEMP_off(src);
	GvSV(defgv) = src;

	RETURNOP(cLOGOP->op_other);
    }
}


PP(pp_sort)
{
    dSP; dMARK; dORIGMARK;
    register SV **up;
    SV **myorigmark = ORIGMARK;
    register I32 max;
    HV *stash;
    GV *gv;
    CV *cv;

    if (GIMME != G_ARRAY) {
	SP = MARK;
	RETPUSHUNDEF;
    }

    if (op->op_flags & OPf_STACKED) {
	ENTER;
	if (op->op_flags & OPf_SPECIAL) {
	    OP *kid = cLISTOP->op_first->op_sibling;	/* pass pushmark */
	    kid = kUNOP->op_first;			/* pass rv2gv */
	    kid = kUNOP->op_first;			/* pass leave */
	    sortcop = kid->op_next;
	    stash = curcop->cop_stash;
	}
	else {
	    cv = sv_2cv(*++MARK, &stash, &gv, 0);
	    if (!(cv && CvROOT(cv))) {
		if (gv) {
		    SV *tmpstr = sv_newmortal();
		    gv_efullname(tmpstr, gv);
		    if (CvXSUB(cv))
			DIE("Xsub \"%s\" called in sort", SvPVX(tmpstr));
		    DIE("Undefined sort subroutine \"%s\" called",
			SvPVX(tmpstr));
		}
		if (cv) {
		    if (CvXSUB(cv))
			DIE("Xsub called in sort");
		    DIE("Undefined subroutine in sort");
		}
		DIE("Not a CODE reference in sort");
	    }
	    sortcop = CvSTART(cv);
	    SAVESPTR(CvROOT(cv)->op_ppaddr);
	    CvROOT(cv)->op_ppaddr = ppaddr[OP_NULL];
	    
	    SAVESPTR(curpad);
	    curpad = AvARRAY((AV*)AvARRAY(CvPADLIST(cv))[1]);
	}
    }
    else {
	sortcop = Nullop;
	stash = curcop->cop_stash;
    }

    up = myorigmark + 1;
    while (MARK < SP) {	/* This may or may not shift down one here. */
	/*SUPPRESS 560*/
	if (*up = *++MARK) {			/* Weed out nulls. */
	    if (!SvPOK(*up))
		(void)sv_2pv(*up, &na);
	    else
		SvTEMP_off(*up);
	    up++;
	}
    }
    max = --up - myorigmark;
    if (sortcop) {
	if (max > 1) {
	    AV *oldstack;

	    SAVETMPS;
	    SAVESPTR(op);

	    oldstack = stack;
	    if (!sortstack) {
		sortstack = newAV();
		AvREAL_off(sortstack);
		av_extend(sortstack, 32);
	    }
	    SWITCHSTACK(stack, sortstack);
	    if (sortstash != stash) {
		firstgv = gv_fetchpv("a", TRUE, SVt_PV);
		secondgv = gv_fetchpv("b", TRUE, SVt_PV);
		sortstash = stash;
	    }

	    SAVESPTR(GvSV(firstgv));
	    SAVESPTR(GvSV(secondgv));

	    qsort((char*)(myorigmark+1), max, sizeof(SV*), sortcv);

	    SWITCHSTACK(sortstack, oldstack);
	}
	LEAVE;
    }
    else {
	if (max > 1) {
	    MEXTEND(SP, 20);	/* Can't afford stack realloc on signal. */
	    qsort((char*)(ORIGMARK+1), max, sizeof(SV*), sortcmp);
	}
    }
    SP = ORIGMARK + max;
    RETURN;
}

PP(pp_reverse)
{
    dSP; dMARK;
    register SV *tmp;
    SV **oldsp = SP;

    if (GIMME == G_ARRAY) {
	MARK++;
	while (MARK < SP) {
	    tmp = *MARK;
	    *MARK++ = *SP;
	    *SP-- = tmp;
	}
	SP = oldsp;
    }
    else {
	register char *up;
	register char *down;
	register I32 tmp;
	dTARGET;
	STRLEN len;

	if (SP - MARK > 1)
	    do_join(TARG, &sv_no, MARK, SP);
	else
	    sv_setsv(TARG, *SP);
	up = SvPV_force(TARG, len);
	if (len > 1) {
	    down = SvPVX(TARG) + len - 1;
	    while (down > up) {
		tmp = *up;
		*up++ = *down;
		*down-- = tmp;
	    }
	    SvPOK_only(TARG);
	}
	SP = MARK + 1;
	SETTARG;
    }
    RETURN;
}

/* Range stuff. */

PP(pp_range)
{
    if (GIMME == G_ARRAY)
	return cCONDOP->op_true;
    return SvTRUEx(PAD_SV(op->op_targ)) ? cCONDOP->op_false : cCONDOP->op_true;
}

PP(pp_flip)
{
    dSP;

    if (GIMME == G_ARRAY) {
	RETURNOP(((CONDOP*)cUNOP->op_first)->op_false);
    }
    else {
	dTOPss;
	SV *targ = PAD_SV(op->op_targ);

	if ((op->op_private & OPpFLIP_LINENUM)
	  ? last_in_gv && SvIV(sv) == IoLINES(GvIO(last_in_gv))
	  : SvTRUE(sv) ) {
	    sv_setiv(PAD_SV(cUNOP->op_first->op_targ), 1);
	    if (op->op_flags & OPf_SPECIAL) {
		sv_setiv(targ, 1);
		RETURN;
	    }
	    else {
		sv_setiv(targ, 0);
		sp--;
		RETURNOP(((CONDOP*)cUNOP->op_first)->op_false);
	    }
	}
	sv_setpv(TARG, "");
	SETs(targ);
	RETURN;
    }
}

PP(pp_flop)
{
    dSP;

    if (GIMME == G_ARRAY) {
	dPOPPOPssrl;
	register I32 i;
	register SV *sv;
	I32 max;

	if (SvNIOK(left) || !SvPOK(left) ||
	  (looks_like_number(left) && *SvPVX(left) != '0') ) {
	    i = SvIV(left);
	    max = SvIV(right);
	    if (max > i)
		EXTEND(SP, max - i + 1);
	    while (i <= max) {
		sv = sv_mortalcopy(&sv_no);
		sv_setiv(sv,i++);
		PUSHs(sv);
	    }
	}
	else {
	    SV *final = sv_mortalcopy(right);
	    STRLEN len;
	    char *tmps = SvPV(final, len);

	    sv = sv_mortalcopy(left);
	    while (!SvNIOK(sv) && SvCUR(sv) <= len &&
		strNE(SvPVX(sv),tmps) ) {
		XPUSHs(sv);
		sv = sv_2mortal(newSVsv(sv));
		sv_inc(sv);
	    }
	    if (strEQ(SvPVX(sv),tmps))
		XPUSHs(sv);
	}
    }
    else {
	dTOPss;
	SV *targ = PAD_SV(cUNOP->op_first->op_targ);
	sv_inc(targ);
	if ((op->op_private & OPpFLIP_LINENUM)
	  ? last_in_gv && SvIV(sv) == IoLINES(GvIO(last_in_gv))
	  : SvTRUE(sv) ) {
	    sv_setiv(PAD_SV(((UNOP*)cUNOP->op_first)->op_first->op_targ), 0);
	    sv_catpv(targ, "E0");
	}
	SETs(targ);
    }

    RETURN;
}

/* Control. */

static I32
dopoptolabel(label)
char *label;
{
    register I32 i;
    register CONTEXT *cx;

    for (i = cxstack_ix; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	case CXt_SUBST:
	    if (dowarn)
		warn("Exiting substitution via %s", op_name[op->op_type]);
	    break;
	case CXt_SUB:
	    if (dowarn)
		warn("Exiting subroutine via %s", op_name[op->op_type]);
	    break;
	case CXt_EVAL:
	    if (dowarn)
		warn("Exiting eval via %s", op_name[op->op_type]);
	    break;
	case CXt_LOOP:
	    if (!cx->blk_loop.label ||
	      strNE(label, cx->blk_loop.label) ) {
		DEBUG_l(deb("(Skipping label #%d %s)\n",
			i, cx->blk_loop.label));
		continue;
	    }
	    DEBUG_l( deb("(Found label #%d %s)\n", i, label));
	    return i;
	}
    }
    return i;
}

static I32
dopoptosub(startingblock)
I32 startingblock;
{
    I32 i;
    register CONTEXT *cx;
    for (i = startingblock; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	default:
	    continue;
	case CXt_EVAL:
	case CXt_SUB:
	    DEBUG_l( deb("(Found sub #%d)\n", i));
	    return i;
	}
    }
    return i;
}

static I32
dopoptoeval(startingblock)
I32 startingblock;
{
    I32 i;
    register CONTEXT *cx;
    for (i = startingblock; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	default:
	    continue;
	case CXt_EVAL:
	    DEBUG_l( deb("(Found eval #%d)\n", i));
	    return i;
	}
    }
    return i;
}

static I32
dopoptoloop(startingblock)
I32 startingblock;
{
    I32 i;
    register CONTEXT *cx;
    for (i = startingblock; i >= 0; i--) {
	cx = &cxstack[i];
	switch (cx->cx_type) {
	case CXt_SUBST:
	    if (dowarn)
		warn("Exiting substitition via %s", op_name[op->op_type]);
	    break;
	case CXt_SUB:
	    if (dowarn)
		warn("Exiting subroutine via %s", op_name[op->op_type]);
	    break;
	case CXt_EVAL:
	    if (dowarn)
		warn("Exiting eval via %s", op_name[op->op_type]);
	    break;
	case CXt_LOOP:
	    DEBUG_l( deb("(Found loop #%d)\n", i));
	    return i;
	}
    }
    return i;
}

void
dounwind(cxix)
I32 cxix;
{
    register CONTEXT *cx;
    SV **newsp;
    I32 optype;

    while (cxstack_ix > cxix) {
	cx = &cxstack[cxstack_ix--];
	DEBUG_l(fprintf(stderr, "Unwinding block %ld, type %s\n", (long) cxstack_ix+1,
		    block_type[cx->cx_type]));
	/* Note: we don't need to restore the base context info till the end. */
	switch (cx->cx_type) {
	case CXt_SUB:
	    POPSUB(cx);
	    break;
	case CXt_EVAL:
	    POPEVAL(cx);
	    break;
	case CXt_LOOP:
	    POPLOOP(cx);
	    break;
	case CXt_SUBST:
	    break;
	}
    }
}

#ifdef STANDARD_C
OP *
die(char* pat, ...)
#else
/*VARARGS0*/
OP *
die(pat, va_alist)
    char *pat;
    va_dcl
#endif
{
    va_list args;
    char *message;
    int oldrunlevel = runlevel;

#ifdef I_STDARG
    va_start(args, pat);
#else
    va_start(args);
#endif
    message = mess(pat, &args);
    va_end(args);
    restartop = die_where(message);
    if (oldrunlevel > 1)
	longjmp(top_env, 3);
    return restartop;
}

OP *
die_where(message)
char *message;
{
    if (in_eval) {
	I32 cxix;
	register CONTEXT *cx;
	I32 gimme;
	SV **newsp;

	sv_setpv(GvSV(gv_fetchpv("@",TRUE, SVt_PV)),message);
	cxix = dopoptoeval(cxstack_ix);
	if (cxix >= 0) {
	    I32 optype;

	    if (cxix < cxstack_ix)
		dounwind(cxix);

	    POPBLOCK(cx,curpm);
	    if (cx->cx_type != CXt_EVAL) {
		fprintf(stderr, "panic: die %s", message);
		my_exit(1);
	    }
	    POPEVAL(cx);

	    if (gimme == G_SCALAR)
		*++newsp = &sv_undef;
	    stack_sp = newsp;

	    LEAVE;
	    if (optype == OP_REQUIRE)
		DIE("%s", SvPVx(GvSV(gv_fetchpv("@",TRUE, SVt_PV)), na));
	    return pop_return();
	}
    }
    fputs(message, stderr);
    (void)fflush(stderr);
    if (e_fp)
	(void)UNLINK(e_tmpname);
    statusvalue >>= 8;
    my_exit((I32)((errno&255)?errno:((statusvalue&255)?statusvalue:255)));
    return 0;
}

PP(pp_and)
{
    dSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_or)
{
    dSP;
    if (SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}
	
PP(pp_xor)
{
    dSP; dPOPTOPssrl;
    if (SvTRUE(left) != SvTRUE(right))
	RETSETYES;
    else
	RETSETNO;
}
	
PP(pp_cond_expr)
{
    dSP;
    if (SvTRUEx(POPs))
	RETURNOP(cCONDOP->op_true);
    else
	RETURNOP(cCONDOP->op_false);
}

PP(pp_andassign)
{
    dSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else
	RETURNOP(cLOGOP->op_other);
}

PP(pp_orassign)
{
    dSP;
    if (SvTRUE(TOPs))
	RETURN;
    else
	RETURNOP(cLOGOP->op_other);
}
	
PP(pp_method)
{
    dSP;
    SV* sv;
    SV* ob;
    GV* gv;
    SV* nm;

    nm = TOPs;
    sv = *(stack_base + TOPMARK + 1);
    
    gv = 0;
    if (SvROK(sv))
	ob = SvRV(sv);
    else {
	GV* iogv;
	char* packname = 0;

	if (!SvOK(sv) ||
	    !(packname = SvPV(sv, na)) ||
	    !(iogv = gv_fetchpv(packname, FALSE, SVt_PVIO)) ||
	    !(ob=(SV*)GvIO(iogv)))
	{
	    char *name = SvPV(nm, na);
	    HV *stash;
	    if (!packname || !isALPHA(*packname))
DIE("Can't call method \"%s\" without a package or object reference", name);
	    if (!(stash = gv_stashpv(packname, FALSE)))
		DIE("Can't call method \"%s\" in empty package \"%s\"",
		    name, packname);
	    gv = gv_fetchmethod(stash,name);
	    if (!gv)
		DIE("Can't locate object method \"%s\" via package \"%s\"",
		    name, packname);
	    SETs(gv);
	    RETURN;
	}
    }

    if (!ob || !SvOBJECT(ob)) {
	char *name = SvPV(nm, na);
	DIE("Can't call method \"%s\" on unblessed reference", name);
    }

    if (!gv) {		/* nothing cached */
	char *name = SvPV(nm, na);
	gv = gv_fetchmethod(SvSTASH(ob),name);
	if (!gv)
	    DIE("Can't locate object method \"%s\" via package \"%s\"",
		name, HvNAME(SvSTASH(ob)));
    }

    SETs(gv);
    RETURN;
}

#ifdef DEPRECATED
PP(pp_entersubr)
{
    dSP;
    SV** mark = (stack_base + *markstack_ptr + 1);
    SV* cv = *mark;
    while (mark < sp) {	/* emulate old interface */
	*mark = mark[1];
	mark++;
    }
    *sp = cv;
    return pp_entersub();
}
#endif

PP(pp_entersub)
{
    dSP; dPOPss;
    GV *gv;
    HV *stash;
    register CV *cv;
    register CONTEXT *cx;

    if (!sv)
	DIE("Not a CODE reference");
    switch (SvTYPE(sv)) {
    default:
	if (!SvROK(sv)) {
	    if (sv == &sv_yes)		/* unfound import, ignore */
		RETURN;
	    if (!SvOK(sv))
		DIE(no_usym, "a subroutine");
	    if (op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, "a subroutine");
	    gv = gv_fetchpv(SvPV(sv, na), FALSE, SVt_PVCV);
	    if (!gv)
		cv = 0;
	    else
		cv = GvCV(gv);
	    break;
	}
	cv = (CV*)SvRV(sv);
	if (SvTYPE(cv) == SVt_PVCV)
	    break;
	/* FALL THROUGH */
    case SVt_PVHV:
    case SVt_PVAV:
	DIE("Not a CODE reference");
    case SVt_PVCV:
	cv = (CV*)sv;
	break;
    case SVt_PVGV:
	if (!(cv = GvCV((GV*)sv)))
	    cv = sv_2cv(sv, &stash, &gv, TRUE);
	break;
    }

    ENTER;
    SAVETMPS;

  retry:
    if (!cv)
	DIE("Not a CODE reference");

    if (!CvROOT(cv) && !CvXSUB(cv)) {
	if (gv = CvGV(cv)) {
	    SV *tmpstr = sv_newmortal();
	    GV *ngv;
	    gv_efullname(tmpstr, gv);
	    ngv = gv_fetchmethod(GvESTASH(gv), "AUTOLOAD");
	    if (ngv && ngv != gv && (cv = GvCV(ngv))) {	/* One more chance... */
		gv = ngv;
		sv_setsv(GvSV(gv), tmpstr);
		goto retry;
	    }
	    else
		DIE("Undefined subroutine &%s called",SvPVX(tmpstr));
	}
	DIE("Undefined subroutine called");
    }

    if ((op->op_private & OPpDEREF_DB) && !CvXSUB(cv)) {
	sv = GvSV(DBsub);
	save_item(sv);
	gv = CvGV(cv);
	gv_efullname(sv,gv);
	cv = GvCV(DBsub);
	if (!cv)
	    DIE("No DBsub routine");
    }

    if (CvXSUB(cv)) {
	if (CvOLDSTYLE(cv)) {
	    dMARK;
	    register I32 items = SP - MARK;
	    I32 hasargs = (op->op_flags & OPf_STACKED) != 0;
	    while (sp > mark) {
		sp[1] = sp[0];
		sp--;
	    }
	    stack_sp = mark + 1;
	    items = (*(I32(*)_((int,int,int)))CvXSUB(cv))(CvXSUBANY(cv).any_i32,
					MARK - stack_base + 1, items);
	    stack_sp = stack_base + items;
	}
	else {
	    PUTBACK;
	    (void)(*CvXSUB(cv))(cv);
	}
	LEAVE;
	return NORMAL;
    }
    else {
	dMARK;
	register I32 items = SP - MARK;
	I32 hasargs = (op->op_flags & OPf_STACKED) != 0;
	I32 gimme = GIMME;
	AV* padlist = CvPADLIST(cv);
	SV** svp = AvARRAY(padlist);
	push_return(op->op_next);
	PUSHBLOCK(cx, CXt_SUB, MARK);
	PUSHSUB(cx);
	CvDEPTH(cv)++;
	if (CvDEPTH(cv) >= 2) {	/* save temporaries on recursion? */
	    if (CvDEPTH(cv) == 100 && dowarn)
		warn("Deep recursion on subroutine \"%s\"",GvENAME(CvGV(cv)));
	    if (CvDEPTH(cv) > AvFILL(padlist)) {
		AV *newpad = newAV();
		I32 ix = AvFILL((AV*)svp[1]);
		svp = AvARRAY(svp[0]);
		while (ix > 0) {
		    if (svp[ix] != &sv_undef) {
			char *name = SvPVX(svp[ix]);	/* XXX */
			if (*name == '@')
			    av_store(newpad, ix--, (SV*)newAV());
			else if (*name == '%')
			    av_store(newpad, ix--, (SV*)newHV());
			else
			    av_store(newpad, ix--, NEWSV(0,0));
		    }
		    else
			av_store(newpad, ix--, NEWSV(0,0));
		}
		if (hasargs) {
		    AV* av = newAV();		/* will be @_ */
		    av_extend(av, 0);
		    av_store(newpad, 0, (SV*)av);
		    AvFLAGS(av) = AVf_REIFY;
		}
		av_store(padlist, CvDEPTH(cv), (SV*)newpad);
		AvFILL(padlist) = CvDEPTH(cv);
		svp = AvARRAY(padlist);
	    }
	}
	SAVESPTR(curpad);
	curpad = AvARRAY((AV*)svp[CvDEPTH(cv)]);
	if (hasargs) {
	    AV* av = (AV*)curpad[0];
	    SV** ary;

	    cx->blk_sub.savearray = GvAV(defgv);
	    cx->blk_sub.argarray = av;
	    GvAV(defgv) = cx->blk_sub.argarray;
	    ++MARK;

	    if (items > AvMAX(av) + 1) {
		ary = AvALLOC(av);
		if (AvARRAY(av) != ary) {
		    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
		    SvPVX(av) = (char*)ary;
		}
		if (items > AvMAX(av) + 1) {
		    AvMAX(av) = items - 1;
		    Renew(ary,items,SV*);
		    AvALLOC(av) = ary;
		    SvPVX(av) = (char*)ary;
		}
	    }
	    Copy(MARK,AvARRAY(av),items,SV*);
	    AvFILL(av) = items - 1;
	    
	    while (items--) {
		if (*MARK)
		    SvTEMP_off(*MARK);
		MARK++;
	    }
	}
	RETURNOP(CvSTART(cv));
    }
}

PP(pp_leavesub)
{
    dSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register CONTEXT *cx;

    POPBLOCK(cx,newpm);
    POPSUB(cx);

    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP)
	    if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
	SP = MARK;
    }
    else {
	for (mark = newsp + 1; mark <= SP; mark++)
	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP)))
		*mark = sv_mortalcopy(*mark);
		/* in case LEAVE wipes old return values */
    }

    if (cx->blk_sub.hasargs) {		/* You don't exist; go away. */
	AV* av = cx->blk_sub.argarray;

	av_clear(av);
	AvREAL_off(av);
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;
    PUTBACK;
    return pop_return();
}

PP(pp_done)
{
    return pop_return();
}

PP(pp_caller)
{
    dSP;
    register I32 cxix = dopoptosub(cxstack_ix);
    I32 nextcxix;
    register CONTEXT *cx;
    SV *sv;
    I32 count = 0;

    if (MAXARG)
	count = POPi;
    EXTEND(SP, 6);
    for (;;) {
	if (cxix < 0) {
	    if (GIMME != G_ARRAY)
		RETPUSHUNDEF;
	    RETURN;
	}
	nextcxix = dopoptosub(cxix - 1);
	if (DBsub && nextcxix >= 0 &&
		cxstack[nextcxix].blk_sub.cv == GvCV(DBsub))
	    count++;
	if (!count--)
	    break;
	cxix = nextcxix;
    }
    cx = &cxstack[cxix];
    if (GIMME != G_ARRAY) {
	dTARGET;

	sv_setpv(TARG, HvNAME(cx->blk_oldcop->cop_stash));
	PUSHs(TARG);
	RETURN;
    }

    PUSHs(sv_2mortal(newSVpv(HvNAME(cx->blk_oldcop->cop_stash), 0)));
    PUSHs(sv_2mortal(newSVpv(SvPVX(GvSV(cx->blk_oldcop->cop_filegv)), 0)));
    PUSHs(sv_2mortal(newSViv((I32)cx->blk_oldcop->cop_line)));
    if (!MAXARG)
	RETURN;
    if (cx->cx_type == CXt_SUB) {
	sv = NEWSV(49, 0);
	gv_efullname(sv, CvGV(cx->blk_sub.cv));
	PUSHs(sv_2mortal(sv));
	PUSHs(sv_2mortal(newSViv((I32)cx->blk_sub.hasargs)));
    }
    else {
	PUSHs(sv_2mortal(newSVpv("(eval)",0)));
	PUSHs(sv_2mortal(newSViv(0)));
    }
    PUSHs(sv_2mortal(newSViv((I32)cx->blk_gimme)));
    if (cx->blk_sub.hasargs && curstash == debstash) {
	AV *ary = cx->blk_sub.argarray;

	if (!dbargs) {
	    GV* tmpgv;
	    dbargs = GvAV(gv_AVadd(tmpgv = gv_fetchpv("DB::args", TRUE,
				SVt_PVAV)));
	    SvMULTI_on(tmpgv);
	    AvREAL_off(dbargs);		/* XXX Should be REIFY */
	}
	if (AvMAX(dbargs) < AvFILL(ary))
	    av_extend(dbargs, AvFILL(ary));
	Copy(AvARRAY(ary), AvARRAY(dbargs), AvFILL(ary)+1, SV*);
	AvFILL(dbargs) = AvFILL(ary);
    }
    RETURN;
}

static int
sortcv(a, b)
const void *a;
const void *b;
{
    SV **str1 = (SV **) a;
    SV **str2 = (SV **) b;
    I32 oldscopeix = scopestack_ix;
    I32 result;
    GvSV(firstgv) = *str1;
    GvSV(secondgv) = *str2;
    stack_sp = stack_base;
    op = sortcop;
    run();
    result = SvIVx(AvARRAY(stack)[1]);
    while (scopestack_ix > oldscopeix) {
	LEAVE;
    }
    return result;
}

static int
sortcmp(a, b)
const void *a;
const void *b;
{
    register SV *str1 = *(SV **) a;
    register SV *str2 = *(SV **) b;
    I32 retval;

    if (SvCUR(str1) < SvCUR(str2)) {
	/*SUPPRESS 560*/
	if (retval = memcmp(SvPVX(str1), SvPVX(str2), SvCUR(str1)))
	    return retval;
	else
	    return -1;
    }
    /*SUPPRESS 560*/
    else if (retval = memcmp(SvPVX(str1), SvPVX(str2), SvCUR(str2)))
	return retval;
    else if (SvCUR(str1) == SvCUR(str2))
	return 0;
    else
	return 1;
}

PP(pp_warn)
{
    dSP; dMARK;
    char *tmps;
    if (SP - MARK != 1) {
	dTARGET;
	do_join(TARG, &sv_no, MARK, SP);
	tmps = SvPV(TARG, na);
	SP = MARK + 1;
    }
    else {
	tmps = SvPV(TOPs, na);
    }
    if (!tmps || !*tmps) {
	SV *error = GvSV(gv_fetchpv("@", TRUE, SVt_PV));
	SvUPGRADE(error, SVt_PV);
	if (SvPOK(error) && SvCUR(error))
	    sv_catpv(error, "\t...caught");
	tmps = SvPV(error, na);
    }
    if (!tmps || !*tmps)
	tmps = "Warning: something's wrong";
    warn("%s", tmps);
    RETSETYES;
}

PP(pp_die)
{
    dSP; dMARK;
    char *tmps;
    if (SP - MARK != 1) {
	dTARGET;
	do_join(TARG, &sv_no, MARK, SP);
	tmps = SvPV(TARG, na);
	SP = MARK + 1;
    }
    else {
	tmps = SvPV(TOPs, na);
    }
    if (!tmps || !*tmps) {
	SV *error = GvSV(gv_fetchpv("@", TRUE, SVt_PV));
	SvUPGRADE(error, SVt_PV);
	if (SvPOK(error) && SvCUR(error))
	    sv_catpv(error, "\t...propagated");
	tmps = SvPV(error, na);
    }
    if (!tmps || !*tmps)
	tmps = "Died";
    DIE("%s", tmps);
}

PP(pp_reset)
{
    dSP;
    char *tmps;

    if (MAXARG < 1)
	tmps = "";
    else
	tmps = POPp;
    sv_reset(tmps, curcop->cop_stash);
    PUSHs(&sv_yes);
    RETURN;
}

PP(pp_lineseq)
{
    return NORMAL;
}

PP(pp_nextstate)
{
    curcop = (COP*)op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    stack_sp = stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREE_TMPS();
    return NORMAL;
}

PP(pp_dbstate)
{
    curcop = (COP*)op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    stack_sp = stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREE_TMPS();

    if (op->op_private || SvIV(DBsingle) || SvIV(DBsignal) || SvIV(DBtrace))
    {
	SV **sp;
	register CV *cv;
	register CONTEXT *cx;
	I32 gimme = GIMME;
	I32 hasargs;
	GV *gv;

	ENTER;
	SAVETMPS;

	SAVEI32(debug);
	debug = 0;
	hasargs = 0;
	gv = DBgv;
	cv = GvCV(gv);
	sp = stack_sp;
	*++sp = Nullsv;

	if (!cv)
	    DIE("No DB::DB routine defined");

	if (CvDEPTH(cv) >= 1)		/* don't do recursive DB::DB call */
	    return NORMAL;
	push_return(op->op_next);
	PUSHBLOCK(cx, CXt_SUB, sp - 1);
	PUSHSUB(cx);
	CvDEPTH(cv)++;
	SAVESPTR(curpad);
	curpad = AvARRAY((AV*)*av_fetch(CvPADLIST(cv),1,FALSE));
	RETURNOP(CvSTART(cv));
    }
    else
	return NORMAL;
}

PP(pp_unstack)
{
    I32 oldsave;
    TAINT_NOT;		/* Each statement is presumed innocent */
    stack_sp = stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREE_TMPS();
    oldsave = scopestack[scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return NORMAL;
}

PP(pp_enter)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme;

    /*
     * We don't just use the GIMME macro here because it assumes there's
     * already a context, which ain't necessarily so at initial startup.
     */

    if (op->op_flags & OPf_KNOW)
	gimme = op->op_flags & OPf_LIST;
    else if (cxstack_ix >= 0)
	gimme = cxstack[cxstack_ix].blk_gimme;
    else
	gimme = G_SCALAR;

    ENTER;

    SAVETMPS;
    PUSHBLOCK(cx, CXt_BLOCK, sp);

    RETURN;
}

PP(pp_leave)
{
    dSP;
    register CONTEXT *cx;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;

    if (op->op_flags & OPf_SPECIAL) {
	cx = &cxstack[cxstack_ix];
	cx->blk_oldpm = curpm;	/* fake block should preserve $1 et al */
    }

    POPBLOCK(cx,newpm);

    if (op->op_flags & OPf_KNOW)
	gimme = op->op_flags & OPf_LIST;
    else if (cxstack_ix >= 0)
	gimme = cxstack[cxstack_ix].blk_gimme;
    else
	gimme = G_SCALAR;

    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP)
	    if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
	SP = MARK;
    }
    else {
	for (mark = newsp + 1; mark <= SP; mark++)
	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP)))
		*mark = sv_mortalcopy(*mark);
		/* in case LEAVE wipes old return values */
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;

    RETURN;
}

PP(pp_scope)
{
    return NORMAL;
}

PP(pp_enteriter)
{
    dSP; dMARK;
    register CONTEXT *cx;
    I32 gimme = GIMME;
    SV **svp;

    if (op->op_targ)
	svp = &curpad[op->op_targ];		/* "my" variable */
    else
	svp = &GvSV((GV*)POPs);			/* symbol table variable */

    ENTER;
    SAVETMPS;
    ENTER;

    PUSHBLOCK(cx, CXt_LOOP, SP);
    PUSHLOOP(cx, svp, MARK);
    cx->blk_loop.iterary = stack;
    cx->blk_loop.iterix = MARK - stack_base;

    RETURN;
}

PP(pp_iter)
{
    dSP;
    register CONTEXT *cx;
    SV *sv;

    EXTEND(sp, 1);
    cx = &cxstack[cxstack_ix];
    if (cx->cx_type != CXt_LOOP)
	DIE("panic: pp_iter");

    if (cx->blk_loop.iterix >= cx->blk_oldsp)
	RETPUSHNO;

    if (sv = AvARRAY(cx->blk_loop.iterary)[++cx->blk_loop.iterix]) {
	SvTEMP_off(sv);
	*cx->blk_loop.itervar = sv;
    }
    else
	*cx->blk_loop.itervar = &sv_undef;

    RETPUSHYES;
}

PP(pp_enterloop)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme = GIMME;

    ENTER;
    SAVETMPS;
    ENTER;

    PUSHBLOCK(cx, CXt_LOOP, SP);
    PUSHLOOP(cx, 0, SP);

    RETURN;
}

PP(pp_leaveloop)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme;
    SV **newsp;
    PMOP *newpm;
    SV **mark;

    POPBLOCK(cx,newpm);
    mark = newsp;
    POPLOOP(cx);
    if (gimme == G_SCALAR) {
	if (mark < SP)
	    *++newsp = sv_mortalcopy(*SP);
	else
	    *++newsp = &sv_undef;
    }
    else {
	while (mark < SP)
	    *++newsp = sv_mortalcopy(*++mark);
    }
    curpm = newpm;	/* Don't pop $1 et al till now */
    sp = newsp;
    LEAVE;
    LEAVE;

    RETURN;
}

PP(pp_return)
{
    dSP; dMARK;
    I32 cxix;
    register CONTEXT *cx;
    I32 gimme;
    SV **newsp;
    PMOP *newpm;
    I32 optype = 0;

    if (stack == sortstack) {
	AvARRAY(stack)[1] = *SP;
	return 0;
    }

    cxix = dopoptosub(cxstack_ix);
    if (cxix < 0)
	DIE("Can't return outside a subroutine");
    if (cxix < cxstack_ix)
	dounwind(cxix);

    POPBLOCK(cx,newpm);
    switch (cx->cx_type) {
    case CXt_SUB:
	POPSUB(cx);
	break;
    case CXt_EVAL:
	POPEVAL(cx);
	break;
    default:
	DIE("panic: return");
	break;
    }

    if (gimme == G_SCALAR) {
	if (MARK < SP)
	    *++newsp = sv_mortalcopy(*SP);
	else
	    *++newsp = &sv_undef;
	if (optype == OP_REQUIRE && !SvTRUE(*newsp))
	    DIE("%s", SvPVx(GvSV(gv_fetchpv("@",TRUE, SVt_PV)), na));
    }
    else {
	if (optype == OP_REQUIRE && MARK == SP)
	    DIE("%s", SvPVx(GvSV(gv_fetchpv("@",TRUE, SVt_PV)), na));
	while (MARK < SP)
	    *++newsp = sv_mortalcopy(*++MARK);
    }
    curpm = newpm;	/* Don't pop $1 et al till now */
    stack_sp = newsp;

    LEAVE;
    return pop_return();
}

PP(pp_last)
{
    dSP;
    I32 cxix;
    register CONTEXT *cx;
    I32 gimme;
    I32 optype;
    OP *nextop;
    SV **newsp;
    PMOP *newpm;
    SV **mark = stack_base + cxstack[cxstack_ix].blk_oldsp;
    /* XXX The sp is probably not right yet... */

    if (op->op_flags & OPf_SPECIAL) {
	cxix = dopoptoloop(cxstack_ix);
	if (cxix < 0)
	    DIE("Can't \"last\" outside a block");
    }
    else {
	cxix = dopoptolabel(cPVOP->op_pv);
	if (cxix < 0)
	    DIE("Label not found for \"last %s\"", cPVOP->op_pv);
    }
    if (cxix < cxstack_ix)
	dounwind(cxix);

    POPBLOCK(cx,newpm);
    switch (cx->cx_type) {
    case CXt_LOOP:
	POPLOOP(cx);
	nextop = cx->blk_loop.last_op->op_next;
	LEAVE;
	break;
    case CXt_EVAL:
	POPEVAL(cx);
	nextop = pop_return();
	break;
    case CXt_SUB:
	POPSUB(cx);
	nextop = pop_return();
	break;
    default:
	DIE("panic: last");
	break;
    }

    if (gimme == G_SCALAR) {
	if (mark < SP)
	    *++newsp = sv_mortalcopy(*SP);
	else
	    *++newsp = &sv_undef;
    }
    else {
	while (mark < SP)
	    *++newsp = sv_mortalcopy(*++mark);
    }
    curpm = newpm;	/* Don't pop $1 et al till now */
    sp = newsp;

    LEAVE;
    RETURNOP(nextop);
}

PP(pp_next)
{
    I32 cxix;
    register CONTEXT *cx;
    I32 oldsave;

    if (op->op_flags & OPf_SPECIAL) {
	cxix = dopoptoloop(cxstack_ix);
	if (cxix < 0)
	    DIE("Can't \"next\" outside a block");
    }
    else {
	cxix = dopoptolabel(cPVOP->op_pv);
	if (cxix < 0)
	    DIE("Label not found for \"next %s\"", cPVOP->op_pv);
    }
    if (cxix < cxstack_ix)
	dounwind(cxix);

    TOPBLOCK(cx);
    oldsave = scopestack[scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return cx->blk_loop.next_op;
}

PP(pp_redo)
{
    I32 cxix;
    register CONTEXT *cx;
    I32 oldsave;

    if (op->op_flags & OPf_SPECIAL) {
	cxix = dopoptoloop(cxstack_ix);
	if (cxix < 0)
	    DIE("Can't \"redo\" outside a block");
    }
    else {
	cxix = dopoptolabel(cPVOP->op_pv);
	if (cxix < 0)
	    DIE("Label not found for \"redo %s\"", cPVOP->op_pv);
    }
    if (cxix < cxstack_ix)
	dounwind(cxix);

    TOPBLOCK(cx);
    oldsave = scopestack[scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return cx->blk_loop.redo_op;
}

static OP* lastgotoprobe;

static OP *
dofindlabel(op,label,opstack)
OP *op;
char *label;
OP **opstack;
{
    OP *kid;
    OP **ops = opstack;

    if (op->op_type == OP_LEAVE ||
	op->op_type == OP_SCOPE ||
	op->op_type == OP_LEAVELOOP ||
	op->op_type == OP_LEAVETRY)
	    *ops++ = cUNOP->op_first;
    *ops = 0;
    if (op->op_flags & OPf_KIDS) {
	/* First try all the kids at this level, since that's likeliest. */
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling) {
	    if ((kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE) &&
		    kCOP->cop_label && strEQ(kCOP->cop_label, label))
		return kid;
	}
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling) {
	    if (kid == lastgotoprobe)
		continue;
	    if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE) {
		if (ops > opstack &&
		  (ops[-1]->op_type == OP_NEXTSTATE ||
		   ops[-1]->op_type == OP_DBSTATE))
		    *ops = kid;
		else
		    *ops++ = kid;
	    }
	    if (op = dofindlabel(kid,label,ops))
		return op;
	}
    }
    *ops = 0;
    return 0;
}

PP(pp_dump)
{
    return pp_goto(ARGS);
    /*NOTREACHED*/
}

PP(pp_goto)
{
    dSP;
    OP *retop = 0;
    I32 ix;
    register CONTEXT *cx;
    OP *enterops[64];
    char *label;
    int do_dump= op->op_type == OP_DUMP;
    label = 0;
    if (op->op_flags & OPf_STACKED) {
	SV *sv = POPs;

	/* This egregious kludge implements goto &subroutine */
	if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVCV) {
	    I32 cxix;
	    register CONTEXT *cx;
	    CV* cv = (CV*)SvRV(sv);
	    SV** mark;
	    I32 items = 0;
	    I32 oldsave;

	    /* First do some returnish stuff. */
	    cxix = dopoptosub(cxstack_ix);
	    if (cxix < 0)
		DIE("Can't goto subroutine outside a subroutine");
	    if (cxix < cxstack_ix)
		dounwind(cxix);
	    TOPBLOCK(cx);
	    mark = stack_sp;
	    if (cx->blk_sub.hasargs) {   /* put @_ back onto stack */
		AV* av = cx->blk_sub.argarray;
		
		items = AvFILL(av) + 1;
		Copy(AvARRAY(av), ++stack_sp, items, SV*);
		stack_sp += items;
		GvAV(defgv) = cx->blk_sub.savearray;
		av_clear(av);
		AvREAL_off(av);
	    }
	    if (!(CvDEPTH(cx->blk_sub.cv) = cx->blk_sub.olddepth)) {
		if (CvDELETED(cx->blk_sub.cv))
		    SvREFCNT_dec(cx->blk_sub.cv);
	    }
	    oldsave = scopestack[scopestack_ix - 1];
	    LEAVE_SCOPE(oldsave);

	    /* Now do some callish stuff. */
	    SAVETMPS;
	    if (CvXSUB(cv)) {
		if (CvOLDSTYLE(cv)) {
		    while (sp > mark) {
			sp[1] = sp[0];
			sp--;
		    }
		    items = (*(I32(*)_((int,int,int)))CvXSUB(cv))(
					CvXSUBANY(cv).any_i32,
					mark - stack_base + 1,
					items);
		    sp = stack_base + items;
		}
		else {
		    (void)(*CvXSUB(cv))(cv);
		}
		LEAVE;
		return pop_return();
	    }
	    else {
		AV* padlist = CvPADLIST(cv);
		SV** svp = AvARRAY(padlist);
		cx->blk_sub.cv = cv;
		cx->blk_sub.olddepth = CvDEPTH(cv);
		CvDEPTH(cv)++;
		if (CvDEPTH(cv) >= 2) {	/* save temporaries on recursion? */
		    if (CvDEPTH(cv) == 100 && dowarn)
			warn("Deep recursion on subroutine \"%s\"",
			    GvENAME(CvGV(cv)));
		    if (CvDEPTH(cv) > AvFILL(padlist)) {
			AV *newpad = newAV();
			I32 ix = AvFILL((AV*)svp[1]);
			svp = AvARRAY(svp[0]);
			while (ix > 0) {
			    if (svp[ix] != &sv_undef) {
				char *name = SvPVX(svp[ix]);	/* XXX */
				if (*name == '@')
				    av_store(newpad, ix--, (SV*)newAV());
				else if (*name == '%')
				    av_store(newpad, ix--, (SV*)newHV());
				else
				    av_store(newpad, ix--, NEWSV(0,0));
			    }
			    else
				av_store(newpad, ix--, NEWSV(0,0));
			}
			if (cx->blk_sub.hasargs) {
			    AV* av = newAV();
			    av_extend(av, 0);
			    av_store(newpad, 0, (SV*)av);
			    AvFLAGS(av) = AVf_REIFY;
			}
			av_store(padlist, CvDEPTH(cv), (SV*)newpad);
			AvFILL(padlist) = CvDEPTH(cv);
			svp = AvARRAY(padlist);
		    }
		}
		SAVESPTR(curpad);
		curpad = AvARRAY((AV*)svp[CvDEPTH(cv)]);
		if (cx->blk_sub.hasargs) {
		    AV* av = (AV*)curpad[0];
		    SV** ary;

		    cx->blk_sub.savearray = GvAV(defgv);
		    cx->blk_sub.argarray = av;
		    GvAV(defgv) = cx->blk_sub.argarray;
		    ++mark;

		    if (items >= AvMAX(av) + 1) {
			ary = AvALLOC(av);
			if (AvARRAY(av) != ary) {
			    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
			    SvPVX(av) = (char*)ary;
			}
			if (items >= AvMAX(av) + 1) {
			    AvMAX(av) = items - 1;
			    Renew(ary,items+1,SV*);
			    AvALLOC(av) = ary;
			    SvPVX(av) = (char*)ary;
			}
		    }
		    Copy(mark,AvARRAY(av),items,SV*);
		    AvFILL(av) = items - 1;
		    
		    while (items--) {
			if (*mark)
			    SvTEMP_off(*mark);
			mark++;
		    }
		}
		RETURNOP(CvSTART(cv));
	    }
	}
	else
	    label = SvPV(sv,na);
    }
    else if (op->op_flags & OPf_SPECIAL) {
	if (! do_dump)
	    DIE("goto must have label");
    }
    else
	label = cPVOP->op_pv;

    if (label && *label) {
	OP *gotoprobe = 0;

	/* find label */

	lastgotoprobe = 0;
	*enterops = 0;
	for (ix = cxstack_ix; ix >= 0; ix--) {
	    cx = &cxstack[ix];
	    switch (cx->cx_type) {
	    case CXt_SUB:
		gotoprobe = CvROOT(cx->blk_sub.cv);
		break;
	    case CXt_EVAL:
		gotoprobe = eval_root; /* XXX not good for nested eval */
		break;
	    case CXt_LOOP:
		gotoprobe = cx->blk_oldcop->op_sibling;
		break;
	    case CXt_SUBST:
		continue;
	    case CXt_BLOCK:
		if (ix)
		    gotoprobe = cx->blk_oldcop->op_sibling;
		else
		    gotoprobe = main_root;
		break;
	    default:
		if (ix)
		    DIE("panic: goto");
		else
		    gotoprobe = main_root;
		break;
	    }
	    retop = dofindlabel(gotoprobe, label, enterops);
	    if (retop)
		break;
	    lastgotoprobe = gotoprobe;
	}
	if (!retop)
	    DIE("Can't find label %s", label);

	/* pop unwanted frames */

	if (ix < cxstack_ix) {
	    I32 oldsave;

	    if (ix < 0)
		ix = 0;
	    dounwind(ix);
	    TOPBLOCK(cx);
	    oldsave = scopestack[scopestack_ix - 1];
	    LEAVE_SCOPE(oldsave);
	}

	/* push wanted frames */

	if (*enterops) {
	    OP *oldop = op;
	    for (ix = 0 + (gotoprobe == main_root); enterops[ix]; ix++) {
		op = enterops[ix];
		(*op->op_ppaddr)();
	    }
	    op = oldop;
	}
    }

    if (do_dump) {
	restartop = retop;
	do_undump = TRUE;

	my_unexec();

	restartop = 0;		/* hmm, must be GNU unexec().. */
	do_undump = FALSE;
    }

    RETURNOP(retop);
}

PP(pp_exit)
{
    dSP;
    I32 anum;

    if (MAXARG < 1)
	anum = 0;
    else
	anum = SvIVx(POPs);
    my_exit(anum);
    PUSHs(&sv_undef);
    RETURN;
}

#ifdef NOTYET
PP(pp_nswitch)
{
    dSP;
    double value = SvNVx(GvSV(cCOP->cop_gv));
    register I32 match = (I32)value;

    if (value < 0.0) {
	if (((double)match) > value)
	    --match;		/* was fractional--truncate other way */
    }
    match -= cCOP->uop.scop.scop_offset;
    if (match < 0)
	match = 0;
    else if (match > cCOP->uop.scop.scop_max)
	match = cCOP->uop.scop.scop_max;
    op = cCOP->uop.scop.scop_next[match];
    RETURNOP(op);
}

PP(pp_cswitch)
{
    dSP;
    register I32 match;

    if (multiline)
	op = op->op_next;			/* can't assume anything */
    else {
	match = *(SvPVx(GvSV(cCOP->cop_gv), na)) & 255;
	match -= cCOP->uop.scop.scop_offset;
	if (match < 0)
	    match = 0;
	else if (match > cCOP->uop.scop.scop_max)
	    match = cCOP->uop.scop.scop_max;
	op = cCOP->uop.scop.scop_next[match];
    }
    RETURNOP(op);
}
#endif

/* I/O. */

PP(pp_open)
{
    dSP; dTARGET;
    GV *gv;
    SV *sv;
    char *tmps;
    STRLEN len;

    if (MAXARG > 1)
	sv = POPs;
    else
	sv = GvSV(TOPs);
    gv = (GV*)POPs;
    tmps = SvPV(sv, len);
    if (do_open(gv, tmps, len,Nullfp)) {
	IoLINES(GvIO(gv)) = 0;
	PUSHi( (I32)forkprocess );
    }
    else if (forkprocess == 0)		/* we are a new child */
	PUSHi(0);
    else
	RETPUSHUNDEF;
    RETURN;
}

PP(pp_close)
{
    dSP;
    GV *gv;

    if (MAXARG == 0)
	gv = defoutgv;
    else
	gv = (GV*)POPs;
    EXTEND(SP, 1);
    PUSHs( do_close(gv, TRUE) ? &sv_yes : &sv_no );
    RETURN;
}

PP(pp_pipe_op)
{
    dSP;
#ifdef HAS_PIPE
    GV *rgv;
    GV *wgv;
    register IO *rstio;
    register IO *wstio;
    int fd[2];

    wgv = (GV*)POPs;
    rgv = (GV*)POPs;

    if (!rgv || !wgv)
	goto badexit;

    rstio = GvIOn(rgv);
    wstio = GvIOn(wgv);

    if (IoIFP(rstio))
	do_close(rgv, FALSE);
    if (IoIFP(wstio))
	do_close(wgv, FALSE);

    if (pipe(fd) < 0)
	goto badexit;

    IoIFP(rstio) = fdopen(fd[0], "r");
    IoOFP(wstio) = fdopen(fd[1], "w");
    IoIFP(wstio) = IoOFP(wstio);
    IoTYPE(rstio) = '<';
    IoTYPE(wstio) = '>';

    if (!IoIFP(rstio) || !IoOFP(wstio)) {
	if (IoIFP(rstio)) fclose(IoIFP(rstio));
	else close(fd[0]);
	if (IoOFP(wstio)) fclose(IoOFP(wstio));
	else close(fd[1]);
	goto badexit;
    }

    RETPUSHYES;

badexit:
    RETPUSHUNDEF;
#else
    DIE(no_func, "pipe");
#endif
}

PP(pp_fileno)
{
    dSP; dTARGET;
    GV *gv;
    IO *io;
    FILE *fp;
    if (MAXARG < 1)
	RETPUSHUNDEF;
    gv = (GV*)POPs;
    if (!gv || !(io = GvIO(gv)) || !(fp = IoIFP(io)))
	RETPUSHUNDEF;
    PUSHi(fileno(fp));
    RETURN;
}

PP(pp_umask)
{
    dSP; dTARGET;
    int anum;

#ifdef HAS_UMASK
    if (MAXARG < 1) {
	anum = umask(0);
	(void)umask(anum);
    }
    else
	anum = umask(POPi);
    TAINT_PROPER("umask");
    XPUSHi(anum);
#else
    DIE(no_func, "Unsupported function umask");
#endif
    RETURN;
}

PP(pp_binmode)
{
    dSP;
    GV *gv;
    IO *io;
    FILE *fp;

    if (MAXARG < 1)
	RETPUSHUNDEF;

    gv = (GV*)POPs;

    EXTEND(SP, 1);
    if (!gv || !(io = GvIO(gv)) || !(fp = IoIFP(io)))
	RETSETUNDEF;

#ifdef DOSISH
#ifdef atarist
    if (!fflush(fp) && (fp->_flag |= _IOBIN))
	RETPUSHYES;
    else
	RETPUSHUNDEF;
#else
    if (setmode(fileno(fp), OP_BINARY) != -1)
	RETPUSHYES;
    else
	RETPUSHUNDEF;
#endif
#else
    RETPUSHYES;
#endif
}

PP(pp_tie)
{
    dSP;
    SV *varsv;
    HV* stash;
    GV *gv;
    BINOP myop;
    SV *sv;
    SV **mark = stack_base + ++*markstack_ptr;	/* reuse in entersub */
    I32 markoff = mark - stack_base - 1;

    varsv = mark[0];

    stash = gv_stashsv(mark[1], FALSE);
    if (!stash || !(gv = gv_fetchmethod(stash, "new")) || !GvCV(gv))
	DIE("Can't tie to package %s", SvPV(mark[1],na));

    Zero(&myop, 1, BINOP);
    myop.op_last = (OP *) &myop;
    myop.op_next = Nullop;
    myop.op_flags = OPf_KNOW|OPf_STACKED;

    ENTER;
    SAVESPTR(op);
    op = (OP *) &myop;

    XPUSHs(gv);
    PUTBACK;

    if (op = pp_entersub())
        run();
    SPAGAIN;

    if (!sv_isobject(TOPs))
	DIE("new didn't return an object");
    sv = TOPs;
    if (SvTYPE(varsv) == SVt_PVHV || SvTYPE(varsv) == SVt_PVAV) {
	sv_unmagic(varsv, 'P');
	sv_magic(varsv, sv, 'P', Nullch, 0);
    }
    else {
	sv_unmagic(varsv, 'q');
	sv_magic(varsv, sv, 'q', Nullch, 0);
    }
    LEAVE;
    SP = stack_base + markoff;
    PUSHs(sv);
    RETURN;
}

PP(pp_untie)
{
    dSP;
    if (SvTYPE(TOPs) == SVt_PVHV || SvTYPE(TOPs) == SVt_PVAV)
	sv_unmagic(TOPs, 'P');
    else
	sv_unmagic(TOPs, 'q');
    RETSETYES;
}

PP(pp_dbmopen)
{
    dSP;
    HV *hv;
    dPOPPOPssrl;
    HV* stash;
    GV *gv;
    BINOP myop;
    SV *sv;

    hv = (HV*)POPs;

    sv = sv_mortalcopy(&sv_no);
    sv_setpv(sv, "AnyDBM_File");
    stash = gv_stashsv(sv, FALSE);
    if (!stash || !(gv = gv_fetchmethod(stash, "new")) || !GvCV(gv)) {
	PUTBACK;
	perl_requirepv("AnyDBM_File.pm");
	SPAGAIN;
	if (!(gv = gv_fetchmethod(stash, "new")) || !GvCV(gv))
	    DIE("No dbm on this machine");
    }

    Zero(&myop, 1, BINOP);
    myop.op_last = (OP *) &myop;
    myop.op_next = Nullop;
    myop.op_flags = OPf_KNOW|OPf_STACKED;

    ENTER;
    SAVESPTR(op);
    op = (OP *) &myop;
    PUTBACK;
    pp_pushmark();

    EXTEND(sp, 5);
    PUSHs(sv);
    PUSHs(left);
    if (SvIV(right))
	PUSHs(sv_2mortal(newSViv(O_RDWR|O_CREAT)));
    else
	PUSHs(sv_2mortal(newSViv(O_RDWR)));
    PUSHs(right);
    PUSHs(gv);
    PUTBACK;

    if (op = pp_entersub())
        run();
    LEAVE;
    SPAGAIN;

    sv = TOPs;
    sv_magic((SV*)hv, sv, 'P', Nullch, 0);
    RETURN;
}

PP(pp_dbmclose)
{
    return pp_untie(ARGS);
}

PP(pp_sselect)
{
    dSP; dTARGET;
#ifdef HAS_SELECT
    register I32 i;
    register I32 j;
    register char *s;
    register SV *sv;
    double value;
    I32 maxlen = 0;
    I32 nfound;
    struct timeval timebuf;
    struct timeval *tbuf = &timebuf;
    I32 growsize;
    char *fd_sets[4];
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	I32 masksize;
	I32 offset;
	I32 k;

#   if BYTEORDER & 0xf0000
#	define ORDERBYTE (0x88888888 - BYTEORDER)
#   else
#	define ORDERBYTE (0x4444 - BYTEORDER)
#   endif

#endif

    SP -= 4;
    for (i = 1; i <= 3; i++) {
	if (!SvPOK(SP[i]))
	    continue;
	j = SvCUR(SP[i]);
	if (maxlen < j)
	    maxlen = j;
    }

#if BYTEORDER == 0x1234 || BYTEORDER == 0x12345678
    growsize = maxlen;		/* little endians can use vecs directly */
#else
#ifdef NFDBITS

#ifndef NBBY
#define NBBY 8
#endif

    masksize = NFDBITS / NBBY;
#else
    masksize = sizeof(long);	/* documented int, everyone seems to use long */
#endif
    growsize = maxlen + (masksize - (maxlen % masksize));
    Zero(&fd_sets[0], 4, char*);
#endif

    sv = SP[4];
    if (SvOK(sv)) {
	value = SvNV(sv);
	if (value < 0.0)
	    value = 0.0;
	timebuf.tv_sec = (long)value;
	value -= (double)timebuf.tv_sec;
	timebuf.tv_usec = (long)(value * 1000000.0);
    }
    else
	tbuf = Null(struct timeval*);

    for (i = 1; i <= 3; i++) {
	sv = SP[i];
	if (!SvOK(sv)) {
	    fd_sets[i] = 0;
	    continue;
	}
	else if (!SvPOK(sv))
	    SvPV_force(sv,na);	/* force string conversion */
	j = SvLEN(sv);
	if (j < growsize) {
	    Sv_Grow(sv, growsize);
	    s = SvPVX(sv) + j;
	    while (++j <= growsize) {
		*s++ = '\0';
	    }
	}
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	s = SvPVX(sv);
	New(403, fd_sets[i], growsize, char);
	for (offset = 0; offset < growsize; offset += masksize) {
	    for (j = 0, k=ORDERBYTE; j < masksize; j++, (k >>= 4))
		fd_sets[i][j+offset] = s[(k % masksize) + offset];
	}
#else
	fd_sets[i] = SvPVX(sv);
#endif
    }

    nfound = select(
	maxlen * 8,
	(Select_fd_set_t) fd_sets[1],
	(Select_fd_set_t) fd_sets[2],
	(Select_fd_set_t) fd_sets[3],
	tbuf);
    for (i = 1; i <= 3; i++) {
	if (fd_sets[i]) {
	    sv = SP[i];
#if BYTEORDER != 0x1234 && BYTEORDER != 0x12345678
	    s = SvPVX(sv);
	    for (offset = 0; offset < growsize; offset += masksize) {
		for (j = 0, k=ORDERBYTE; j < masksize; j++, (k >>= 4))
		    s[(k % masksize) + offset] = fd_sets[i][j+offset];
	    }
	    Safefree(fd_sets[i]);
#endif
	    SvSETMAGIC(sv);
	}
    }

    PUSHi(nfound);
    if (GIMME == G_ARRAY && tbuf) {
	value = (double)(timebuf.tv_sec) +
		(double)(timebuf.tv_usec) / 1000000.0;
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setnv(sv, value);
    }
    RETURN;
#else
    DIE("select not implemented");
#endif
}

PP(pp_select)
{
    dSP; dTARGET;
    GV *oldgv = defoutgv;
    if (op->op_private > 0) {
	defoutgv = (GV*)POPs;
	if (!GvIO(defoutgv))
	    GvIO(defoutgv) = newIO();
    }
    gv_efullname(TARG, oldgv);
    XPUSHTARG;
    RETURN;
}

PP(pp_getc)
{
    dSP; dTARGET;
    GV *gv;

    if (MAXARG <= 0)
	gv = stdingv;
    else
	gv = (GV*)POPs;
    if (!gv)
	gv = argvgv;
    if (!gv || do_eof(gv)) /* make sure we have fp with something */
	RETPUSHUNDEF;
    TAINT_IF(1);
    sv_setpv(TARG, " ");
    *SvPVX(TARG) = getc(IoIFP(GvIO(gv))); /* should never be EOF */
    PUSHTARG;
    RETURN;
}

PP(pp_read)
{
    return pp_sysread(ARGS);
}

static OP *
doform(cv,gv,retop)
CV *cv;
GV *gv;
OP *retop;
{
    register CONTEXT *cx;
    I32 gimme = GIMME;
    ENTER;
    SAVETMPS;

    push_return(retop);
    PUSHBLOCK(cx, CXt_SUB, stack_sp);
    PUSHFORMAT(cx);
    defoutgv = gv;		/* locally select filehandle so $% et al work */
    return CvSTART(cv);
}

PP(pp_enterwrite)
{
    dSP;
    register GV *gv;
    register IO *io;
    GV *fgv;
    CV *cv;

    if (MAXARG == 0)
	gv = defoutgv;
    else {
	gv = (GV*)POPs;
	if (!gv)
	    gv = defoutgv;
    }
    EXTEND(SP, 1);
    io = GvIO(gv);
    if (!io) {
	RETPUSHNO;
    }
    if (IoFMT_GV(io))
	fgv = IoFMT_GV(io);
    else
	fgv = gv;

    cv = GvFORM(fgv);

    if (!cv) {
	if (fgv) {
	    SV *tmpstr = sv_newmortal();
	    gv_efullname(tmpstr, gv);
	    DIE("Undefined format \"%s\" called",SvPVX(tmpstr));
	}
	DIE("Not a format reference");
    }

    return doform(cv,gv,op->op_next);
}

PP(pp_leavewrite)
{
    dSP;
    GV *gv = cxstack[cxstack_ix].blk_sub.gv;
    register IO *io = GvIO(gv);
    FILE *ofp = IoOFP(io);
    FILE *fp;
    SV **newsp;
    I32 gimme;
    register CONTEXT *cx;

    DEBUG_f(fprintf(stderr,"left=%ld, todo=%ld\n",
	  (long)IoLINES_LEFT(io), (long)FmLINES(formtarget)));
    if (IoLINES_LEFT(io) < FmLINES(formtarget) &&
	formtarget != toptarget)
    {
	if (!IoTOP_GV(io)) {
	    GV *topgv;
	    char tmpbuf[256];

	    if (!IoTOP_NAME(io)) {
		if (!IoFMT_NAME(io))
		    IoFMT_NAME(io) = savestr(GvNAME(gv));
		sprintf(tmpbuf, "%s_TOP", IoFMT_NAME(io));
		topgv = gv_fetchpv(tmpbuf,FALSE, SVt_PVFM);
                if ((topgv && GvFORM(topgv)) ||
		  !gv_fetchpv("top",FALSE,SVt_PVFM))
		    IoTOP_NAME(io) = savestr(tmpbuf);
		else
		    IoTOP_NAME(io) = savestr("top");
	    }
	    topgv = gv_fetchpv(IoTOP_NAME(io),FALSE, SVt_PVFM);
	    if (!topgv || !GvFORM(topgv)) {
		IoLINES_LEFT(io) = 100000000;
		goto forget_top;
	    }
	    IoTOP_GV(io) = topgv;
	}
	if (IoLINES_LEFT(io) >= 0 && IoPAGE(io) > 0)
	    fwrite(SvPVX(formfeed), SvCUR(formfeed), 1, ofp);
	IoLINES_LEFT(io) = IoPAGE_LEN(io);
	IoPAGE(io)++;
	formtarget = toptarget;
	return doform(GvFORM(IoTOP_GV(io)),gv,op);
    }

  forget_top:
    POPBLOCK(cx,curpm);
    POPFORMAT(cx);
    LEAVE;

    fp = IoOFP(io);
    if (!fp) {
	if (dowarn) {
	    if (IoIFP(io))
		warn("Filehandle only opened for input");
	    else
		warn("Write on closed filehandle");
	}
	PUSHs(&sv_no);
    }
    else {
	if ((IoLINES_LEFT(io) -= FmLINES(formtarget)) < 0) {
	    if (dowarn)
		warn("page overflow");
	}
	if (!fwrite(SvPVX(formtarget), 1, SvCUR(formtarget), ofp) ||
		ferror(fp))
	    PUSHs(&sv_no);
	else {
	    FmLINES(formtarget) = 0;
	    SvCUR_set(formtarget, 0);
	    if (IoFLAGS(io) & IOf_FLUSH)
		(void)fflush(fp);
	    PUSHs(&sv_yes);
	}
    }
    formtarget = bodytarget;
    PUTBACK;
    return pop_return();
}

PP(pp_prtf)
{
    dSP; dMARK; dORIGMARK;
    GV *gv;
    IO *io;
    FILE *fp;
    SV *sv = NEWSV(0,0);

    if (op->op_flags & OPf_STACKED)
	gv = (GV*)*++MARK;
    else
	gv = defoutgv;
    if (!(io = GvIO(gv))) {
	if (dowarn)
	    warn("Filehandle %s never opened", GvNAME(gv));
	errno = EBADF;
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (dowarn)  {
	    if (IoIFP(io))
		warn("Filehandle %s opened only for input", GvNAME(gv));
	    else
		warn("printf on closed filehandle %s", GvNAME(gv));
	}
	errno = EBADF;
	goto just_say_no;
    }
    else {
	do_sprintf(sv, SP - MARK, MARK + 1);
	if (!do_print(sv, fp))
	    goto just_say_no;

	if (IoFLAGS(io) & IOf_FLUSH)
	    if (fflush(fp) == EOF)
		goto just_say_no;
    }
    SvREFCNT_dec(sv);
    SP = ORIGMARK;
    PUSHs(&sv_yes);
    RETURN;

  just_say_no:
    SvREFCNT_dec(sv);
    SP = ORIGMARK;
    PUSHs(&sv_undef);
    RETURN;
}

PP(pp_print)
{
    dSP; dMARK; dORIGMARK;
    GV *gv;
    IO *io;
    register FILE *fp;

    if (op->op_flags & OPf_STACKED)
	gv = (GV*)*++MARK;
    else
	gv = defoutgv;
    if (!(io = GvIO(gv))) {
	if (dowarn)
	    warn("Filehandle %s never opened", GvNAME(gv));
	errno = EBADF;
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (dowarn)  {
	    if (IoIFP(io))
		warn("Filehandle %s opened only for input", GvNAME(gv));
	    else
		warn("print on closed filehandle %s", GvNAME(gv));
	}
	errno = EBADF;
	goto just_say_no;
    }
    else {
	MARK++;
	if (ofslen) {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
		if (MARK <= SP) {
		    if (fwrite(ofs, 1, ofslen, fp) == 0 || ferror(fp)) {
			MARK--;
			break;
		    }
		}
	    }
	}
	else {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
	    }
	}
	if (MARK <= SP)
	    goto just_say_no;
	else {
	    if (orslen)
		if (fwrite(ors, 1, orslen, fp) == 0 || ferror(fp))
		    goto just_say_no;

	    if (IoFLAGS(io) & IOf_FLUSH)
		if (fflush(fp) == EOF)
		    goto just_say_no;
	}
    }
    SP = ORIGMARK;
    PUSHs(&sv_yes);
    RETURN;

  just_say_no:
    SP = ORIGMARK;
    PUSHs(&sv_undef);
    RETURN;
}

PP(pp_sysread)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    int offset;
    GV *gv;
    IO *io;
    char *buffer;
    int length;
    int bufsize;
    SV *bufstr;
    STRLEN blen;

    gv = (GV*)*++MARK;
    if (!gv)
	goto say_undef;
    bufstr = *++MARK;
    buffer = SvPV_force(bufstr, blen);
    length = SvIVx(*++MARK);
    errno = 0;
    if (MARK < SP)
	offset = SvIVx(*++MARK);
    else
	offset = 0;
    io = GvIO(gv);
    if (!io || !IoIFP(io))
	goto say_undef;
#ifdef HAS_SOCKET
    if (op->op_type == OP_RECV) {
	bufsize = sizeof buf;
	buffer = SvGROW(bufstr, length+1);
	length = recvfrom(fileno(IoIFP(io)), buffer, length, offset,
	    (struct sockaddr *)buf, &bufsize);
	if (length < 0)
	    RETPUSHUNDEF;
	SvCUR_set(bufstr, length);
	*SvEND(bufstr) = '\0';
	SvPOK_only(bufstr);
	SvSETMAGIC(bufstr);
	if (tainting)
	    sv_magic(bufstr, Nullsv, 't', Nullch, 0);
	SP = ORIGMARK;
	sv_setpvn(TARG, buf, bufsize);
	PUSHs(TARG);
	RETURN;
    }
#else
    if (op->op_type == OP_RECV)
	DIE(no_sock_func, "recv");
#endif
    buffer = SvGROW(bufstr, length+offset+1);
    if (op->op_type == OP_SYSREAD) {
	length = read(fileno(IoIFP(io)), buffer+offset, length);
    }
    else
#ifdef HAS_SOCKET__bad_code_maybe
    if (IoTYPE(io) == 's') {
	bufsize = sizeof buf;
	length = recvfrom(fileno(IoIFP(io)), buffer+offset, length, 0,
	    (struct sockaddr *)buf, &bufsize);
    }
    else
#endif
	length = fread(buffer+offset, 1, length, IoIFP(io));
    if (length < 0)
	goto say_undef;
    SvCUR_set(bufstr, length+offset);
    *SvEND(bufstr) = '\0';
    SvPOK_only(bufstr);
    SvSETMAGIC(bufstr);
    if (tainting)
	sv_magic(bufstr, Nullsv, 't', Nullch, 0);
    SP = ORIGMARK;
    PUSHi(length);
    RETURN;

  say_undef:
    SP = ORIGMARK;
    RETPUSHUNDEF;
}

PP(pp_syswrite)
{
    return pp_send(ARGS);
}

PP(pp_send)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    GV *gv;
    IO *io;
    int offset;
    SV *bufstr;
    char *buffer;
    int length;
    STRLEN blen;

    gv = (GV*)*++MARK;
    if (!gv)
	goto say_undef;
    bufstr = *++MARK;
    buffer = SvPV(bufstr, blen);
    length = SvIVx(*++MARK);
    errno = 0;
    io = GvIO(gv);
    if (!io || !IoIFP(io)) {
	length = -1;
	if (dowarn) {
	    if (op->op_type == OP_SYSWRITE)
		warn("Syswrite on closed filehandle");
	    else
		warn("Send on closed socket");
	}
    }
    else if (op->op_type == OP_SYSWRITE) {
	if (MARK < SP)
	    offset = SvIVx(*++MARK);
	else
	    offset = 0;
	if (length > blen - offset)
	    length = blen - offset;
	length = write(fileno(IoIFP(io)), buffer+offset, length);
    }
#ifdef HAS_SOCKET
    else if (SP >= MARK) {
	char *sockbuf;
	STRLEN mlen;
	sockbuf = SvPVx(*++MARK, mlen);
	length = sendto(fileno(IoIFP(io)), buffer, blen, length,
				(struct sockaddr *)sockbuf, mlen);
    }
    else
	length = send(fileno(IoIFP(io)), buffer, blen, length);
#else
    else
	DIE(no_sock_func, "send");
#endif
    if (length < 0)
	goto say_undef;
    SP = ORIGMARK;
    PUSHi(length);
    RETURN;

  say_undef:
    SP = ORIGMARK;
    RETPUSHUNDEF;
}

PP(pp_recv)
{
    return pp_sysread(ARGS);
}

PP(pp_eof)
{
    dSP;
    GV *gv;

    if (MAXARG <= 0)
	gv = last_in_gv;
    else
	gv = last_in_gv = (GV*)POPs;
    PUSHs(!gv || do_eof(gv) ? &sv_yes : &sv_no);
    RETURN;
}

PP(pp_tell)
{
    dSP; dTARGET;
    GV *gv;

    if (MAXARG <= 0)
	gv = last_in_gv;
    else
	gv = last_in_gv = (GV*)POPs;
    PUSHi( do_tell(gv) );
    RETURN;
}

PP(pp_seek)
{
    dSP;
    GV *gv;
    int whence = POPi;
    long offset = POPl;

    gv = last_in_gv = (GV*)POPs;
    PUSHs( do_seek(gv, offset, whence) ? &sv_yes : &sv_no );
    RETURN;
}

PP(pp_truncate)
{
    dSP;
    Off_t len = (Off_t)POPn;
    int result = 1;
    GV *tmpgv;

    errno = 0;
#if defined(HAS_TRUNCATE) || defined(HAS_CHSIZE)
#ifdef HAS_TRUNCATE
    if (op->op_flags & OPf_SPECIAL) {
	tmpgv = gv_fetchpv(POPp,FALSE, SVt_PVIO);
	if (!tmpgv || !GvIO(tmpgv) || !IoIFP(GvIO(tmpgv)) ||
	  ftruncate(fileno(IoIFP(GvIO(tmpgv))), len) < 0)
	    result = 0;
    }
    else if (truncate(POPp, len) < 0)
	result = 0;
#else
    if (op->op_flags & OPf_SPECIAL) {
	tmpgv = gv_fetchpv(POPp,FALSE, SVt_PVIO);
	if (!tmpgv || !GvIO(tmpgv) || !IoIFP(GvIO(tmpgv)) ||
	  chsize(fileno(IoIFP(GvIO(tmpgv))), len) < 0)
	    result = 0;
    }
    else {
	int tmpfd;

	if ((tmpfd = open(POPp, 0)) < 0)
	    result = 0;
	else {
	    if (chsize(tmpfd, len) < 0)
		result = 0;
	    close(tmpfd);
	}
    }
#endif

    if (result)
	RETPUSHYES;
    if (!errno)
	errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE("truncate not implemented");
#endif
}

PP(pp_fcntl)
{
    return pp_ioctl(ARGS);
}

PP(pp_ioctl)
{
    dSP; dTARGET;
    SV *argstr = POPs;
    unsigned int func = U_I(POPn);
    int optype = op->op_type;
    char *s;
    int retval;
    GV *gv = (GV*)POPs;
    IO *io = GvIOn(gv);

    if (!io || !argstr || !IoIFP(io)) {
	errno = EBADF;	/* well, sort of... */
	RETPUSHUNDEF;
    }

    if (SvPOK(argstr) || !SvNIOK(argstr)) {
	STRLEN len;
	s = SvPV_force(argstr, len);
	retval = IOCPARM_LEN(func);
	if (len < retval) {
	    s = Sv_Grow(argstr, retval+1);
	    SvCUR_set(argstr, retval);
	}

	s[SvCUR(argstr)] = 17;	/* a little sanity check here */
    }
    else {
	retval = SvIV(argstr);
#ifdef DOSISH
	s = (char*)(long)retval;	/* ouch */
#else
	s = (char*)retval;		/* ouch */
#endif
    }

    TAINT_PROPER(optype == OP_IOCTL ? "ioctl" : "fcntl");

    if (optype == OP_IOCTL)
#ifdef HAS_IOCTL
	retval = ioctl(fileno(IoIFP(io)), func, s);
#else
	DIE("ioctl is not implemented");
#endif
    else
#ifdef DOSISH
	DIE("fcntl is not implemented");
#else
#   ifdef HAS_FCNTL
	retval = fcntl(fileno(IoIFP(io)), func, s);
#   else
	DIE("fcntl is not implemented");
#   endif
#endif

    if (SvPOK(argstr)) {
	if (s[SvCUR(argstr)] != 17)
	    DIE("Possible memory corruption: %s overflowed 3rd argument",
		op_name[optype]);
	s[SvCUR(argstr)] = 0;		/* put our null back */
	SvSETMAGIC(argstr);		/* Assume it has changed */
    }

    if (retval == -1)
	RETPUSHUNDEF;
    if (retval != 0) {
	PUSHi(retval);
    }
    else {
	PUSHp("0 but true", 10);
    }
    RETURN;
}

PP(pp_flock)
{
    dSP; dTARGET;
    I32 value;
    int argtype;
    GV *gv;
    FILE *fp;
#ifdef HAS_FLOCK
    argtype = POPi;
    if (MAXARG <= 0)
	gv = last_in_gv;
    else
	gv = (GV*)POPs;
    if (gv && GvIO(gv))
	fp = IoIFP(GvIO(gv));
    else
	fp = Nullfp;
    if (fp) {
	value = (I32)(flock(fileno(fp), argtype) >= 0);
    }
    else
	value = 0;
    PUSHi(value);
    RETURN;
#else
# ifdef HAS_LOCKF
    DIE(no_func, "flock()"); /* XXX emulate flock() with lockf()? */
# else
    DIE(no_func, "flock()");
# endif
#endif
}

/* Sockets. */

PP(pp_socket)
{
    dSP;
#ifdef HAS_SOCKET
    GV *gv;
    register IO *io;
    int protocol = POPi;
    int type = POPi;
    int domain = POPi;
    int fd;

    gv = (GV*)POPs;

    if (!gv) {
	errno = EBADF;
	RETPUSHUNDEF;
    }

    io = GvIOn(gv);
    if (IoIFP(io))
	do_close(gv, FALSE);

    TAINT_PROPER("socket");
    fd = socket(domain, type, protocol);
    if (fd < 0)
	RETPUSHUNDEF;
    IoIFP(io) = fdopen(fd, "r");	/* stdio gets confused about sockets */
    IoOFP(io) = fdopen(fd, "w");
    IoTYPE(io) = 's';
    if (!IoIFP(io) || !IoOFP(io)) {
	if (IoIFP(io)) fclose(IoIFP(io));
	if (IoOFP(io)) fclose(IoOFP(io));
	if (!IoIFP(io) && !IoOFP(io)) close(fd);
	RETPUSHUNDEF;
    }

    RETPUSHYES;
#else
    DIE(no_sock_func, "socket");
#endif
}

PP(pp_sockpair)
{
    dSP;
#ifdef HAS_SOCKETPAIR
    GV *gv1;
    GV *gv2;
    register IO *io1;
    register IO *io2;
    int protocol = POPi;
    int type = POPi;
    int domain = POPi;
    int fd[2];

    gv2 = (GV*)POPs;
    gv1 = (GV*)POPs;
    if (!gv1 || !gv2)
	RETPUSHUNDEF;

    io1 = GvIOn(gv1);
    io2 = GvIOn(gv2);
    if (IoIFP(io1))
	do_close(gv1, FALSE);
    if (IoIFP(io2))
	do_close(gv2, FALSE);

    TAINT_PROPER("socketpair");
    if (socketpair(domain, type, protocol, fd) < 0)
	RETPUSHUNDEF;
    IoIFP(io1) = fdopen(fd[0], "r");
    IoOFP(io1) = fdopen(fd[0], "w");
    IoTYPE(io1) = 's';
    IoIFP(io2) = fdopen(fd[1], "r");
    IoOFP(io2) = fdopen(fd[1], "w");
    IoTYPE(io2) = 's';
    if (!IoIFP(io1) || !IoOFP(io1) || !IoIFP(io2) || !IoOFP(io2)) {
	if (IoIFP(io1)) fclose(IoIFP(io1));
	if (IoOFP(io1)) fclose(IoOFP(io1));
	if (!IoIFP(io1) && !IoOFP(io1)) close(fd[0]);
	if (IoIFP(io2)) fclose(IoIFP(io2));
	if (IoOFP(io2)) fclose(IoOFP(io2));
	if (!IoIFP(io2) && !IoOFP(io2)) close(fd[1]);
	RETPUSHUNDEF;
    }

    RETPUSHYES;
#else
    DIE(no_sock_func, "socketpair");
#endif
}

PP(pp_bind)
{
    dSP;
#ifdef HAS_SOCKET
    SV *addrstr = POPs;
    char *addr;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);
    STRLEN len;

    if (!io || !IoIFP(io))
	goto nuts;

    addr = SvPV(addrstr, len);
    TAINT_PROPER("bind");
    if (bind(fileno(IoIFP(io)), (struct sockaddr *)addr, len) >= 0)
	RETPUSHYES;
    else
	RETPUSHUNDEF;

nuts:
    if (dowarn)
	warn("bind() on closed fd");
    errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "bind");
#endif
}

PP(pp_connect)
{
    dSP;
#ifdef HAS_SOCKET
    SV *addrstr = POPs;
    char *addr;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);
    STRLEN len;

    if (!io || !IoIFP(io))
	goto nuts;

    addr = SvPV(addrstr, len);
    TAINT_PROPER("connect");
    if (connect(fileno(IoIFP(io)), (struct sockaddr *)addr, len) >= 0)
	RETPUSHYES;
    else
	RETPUSHUNDEF;

nuts:
    if (dowarn)
	warn("connect() on closed fd");
    errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "connect");
#endif
}

PP(pp_listen)
{
    dSP;
#ifdef HAS_SOCKET
    int backlog = POPi;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoIFP(io))
	goto nuts;

    if (listen(fileno(IoIFP(io)), backlog) >= 0)
	RETPUSHYES;
    else
	RETPUSHUNDEF;

nuts:
    if (dowarn)
	warn("listen() on closed fd");
    errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "listen");
#endif
}

PP(pp_accept)
{
    dSP; dTARGET;
#ifdef HAS_SOCKET
    GV *ngv;
    GV *ggv;
    register IO *nstio;
    register IO *gstio;
    int len = sizeof buf;
    int fd;

    ggv = (GV*)POPs;
    ngv = (GV*)POPs;

    if (!ngv)
	goto badexit;
    if (!ggv)
	goto nuts;

    gstio = GvIO(ggv);
    if (!gstio || !IoIFP(gstio))
	goto nuts;

    nstio = GvIOn(ngv);
    if (IoIFP(nstio))
	do_close(ngv, FALSE);

    fd = accept(fileno(IoIFP(gstio)), (struct sockaddr *)buf, &len);
    if (fd < 0)
	goto badexit;
    IoIFP(nstio) = fdopen(fd, "r");
    IoOFP(nstio) = fdopen(fd, "w");
    IoTYPE(nstio) = 's';
    if (!IoIFP(nstio) || !IoOFP(nstio)) {
	if (IoIFP(nstio)) fclose(IoIFP(nstio));
	if (IoOFP(nstio)) fclose(IoOFP(nstio));
	if (!IoIFP(nstio) && !IoOFP(nstio)) close(fd);
	goto badexit;
    }

    PUSHp(buf, len);
    RETURN;

nuts:
    if (dowarn)
	warn("accept() on closed fd");
    errno = EBADF;

badexit:
    RETPUSHUNDEF;

#else
    DIE(no_sock_func, "accept");
#endif
}

PP(pp_shutdown)
{
    dSP; dTARGET;
#ifdef HAS_SOCKET
    int how = POPi;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoIFP(io))
	goto nuts;

    PUSHi( shutdown(fileno(IoIFP(io)), how) >= 0 );
    RETURN;

nuts:
    if (dowarn)
	warn("shutdown() on closed fd");
    errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_sock_func, "shutdown");
#endif
}

PP(pp_gsockopt)
{
#ifdef HAS_SOCKET
    return pp_ssockopt(ARGS);
#else
    DIE(no_sock_func, "getsockopt");
#endif
}

PP(pp_ssockopt)
{
    dSP;
#ifdef HAS_SOCKET
    int optype = op->op_type;
    SV *sv;
    int fd;
    unsigned int optname;
    unsigned int lvl;
    GV *gv;
    register IO *io;

    if (optype == OP_GSOCKOPT)
	sv = sv_2mortal(NEWSV(22, 257));
    else
	sv = POPs;
    optname = (unsigned int) POPi;
    lvl = (unsigned int) POPi;

    gv = (GV*)POPs;
    io = GvIOn(gv);
    if (!io || !IoIFP(io))
	goto nuts;

    fd = fileno(IoIFP(io));
    switch (optype) {
    case OP_GSOCKOPT:
	SvGROW(sv, 256);
	SvPOK_only(sv);
	if (getsockopt(fd, lvl, optname, SvPVX(sv), (int*)&SvCUR(sv)) < 0)
	    goto nuts2;
	PUSHs(sv);
	break;
    case OP_SSOCKOPT: {
	    int aint;
	    STRLEN len = 0;
	    char *buf = 0;
	    if (SvPOKp(sv))
		buf = SvPV(sv, len);
	    else if (SvOK(sv)) {
		aint = (int)SvIV(sv);
		buf = (char*)&aint;
		len = sizeof(int);
	    }
	    if (setsockopt(fd, lvl, optname, buf, (int)len) < 0)
		goto nuts2;
	    PUSHs(&sv_yes);
	}
	break;
    }
    RETURN;

nuts:
    if (dowarn)
	warn("[gs]etsockopt() on closed fd");
    errno = EBADF;
nuts2:
    RETPUSHUNDEF;

#else
    DIE(no_sock_func, "setsockopt");
#endif
}

PP(pp_getsockname)
{
#ifdef HAS_SOCKET
    return pp_getpeername(ARGS);
#else
    DIE(no_sock_func, "getsockname");
#endif
}

PP(pp_getpeername)
{
    dSP;
#ifdef HAS_SOCKET
    int optype = op->op_type;
    SV *sv;
    int fd;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoIFP(io))
	goto nuts;

    sv = sv_2mortal(NEWSV(22, 257));
    SvCUR_set(sv, 256);
    SvPOK_on(sv);
    fd = fileno(IoIFP(io));
    switch (optype) {
    case OP_GETSOCKNAME:
	if (getsockname(fd, (struct sockaddr *)SvPVX(sv), (int*)&SvCUR(sv)) < 0)
	    goto nuts2;
	break;
    case OP_GETPEERNAME:
	if (getpeername(fd, (struct sockaddr *)SvPVX(sv), (int*)&SvCUR(sv)) < 0)
	    goto nuts2;
	break;
    }
    PUSHs(sv);
    RETURN;

nuts:
    if (dowarn)
	warn("get{sock, peer}name() on closed fd");
    errno = EBADF;
nuts2:
    RETPUSHUNDEF;

#else
    DIE(no_sock_func, "getpeername");
#endif
}

/* Stat calls. */

PP(pp_lstat)
{
    return pp_stat(ARGS);
}

PP(pp_stat)
{
    dSP;
    GV *tmpgv;
    I32 max = 13;

    if (op->op_flags & OPf_REF) {
	tmpgv = cGVOP->op_gv;
	if (tmpgv != defgv) {
	    laststype = OP_STAT;
	    statgv = tmpgv;
	    sv_setpv(statname, "");
	    if (!GvIO(tmpgv) || !IoIFP(GvIO(tmpgv)) ||
	      fstat(fileno(IoIFP(GvIO(tmpgv))), &statcache) < 0) {
		max = 0;
		laststatval = -1;
	    }
	}
	else if (laststatval < 0)
	    max = 0;
    }
    else {
	sv_setpv(statname, POPp);
	statgv = Nullgv;
#ifdef HAS_LSTAT
	laststype = op->op_type;
	if (op->op_type == OP_LSTAT)
	    laststatval = lstat(SvPV(statname, na), &statcache);
	else
#endif
	    laststatval = stat(SvPV(statname, na), &statcache);
	if (laststatval < 0) {
	    if (dowarn && strchr(SvPV(statname, na), '\n'))
		warn(warn_nl, "stat");
	    max = 0;
	}
    }

    EXTEND(SP, 13);
    if (GIMME != G_ARRAY) {
	if (max)
	    RETPUSHYES;
	else
	    RETPUSHUNDEF;
    }
    if (max) {
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_dev)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_ino)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_mode)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_nlink)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_uid)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_gid)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_rdev)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_size)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_atime)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_mtime)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_ctime)));
#ifdef USE_STAT_BLOCKS
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_blksize)));
	PUSHs(sv_2mortal(newSViv((I32)statcache.st_blocks)));
#else
	PUSHs(sv_2mortal(newSVpv("", 0)));
	PUSHs(sv_2mortal(newSVpv("", 0)));
#endif
    }
    RETURN;
}

PP(pp_ftrread)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IRUSR, 0, &statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftrwrite)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IWUSR, 0, &statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftrexec)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IXUSR, 0, &statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_fteread)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IRUSR, 1, &statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftewrite)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IWUSR, 1, &statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_fteexec)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (cando(S_IXUSR, 1, &statcache))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftis)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    RETPUSHYES;
}

PP(pp_fteowned)
{
    return pp_ftrowned(ARGS);
}

PP(pp_ftrowned)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (statcache.st_uid == (op->op_type == OP_FTEOWNED ? euid : uid) )
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftzero)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (!statcache.st_size)
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftsize)
{
    I32 result = my_stat(ARGS);
    dSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHi(statcache.st_size);
    RETURN;
}

PP(pp_ftmtime)
{
    I32 result = my_stat(ARGS);
    dSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHn( (basetime - statcache.st_mtime) / 86400.0 );
    RETURN;
}

PP(pp_ftatime)
{
    I32 result = my_stat(ARGS);
    dSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHn( (basetime - statcache.st_atime) / 86400.0 );
    RETURN;
}

PP(pp_ftctime)
{
    I32 result = my_stat(ARGS);
    dSP; dTARGET;
    if (result < 0)
	RETPUSHUNDEF;
    PUSHn( (basetime - statcache.st_ctime) / 86400.0 );
    RETURN;
}

PP(pp_ftsock)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISSOCK(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftchr)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISCHR(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftblk)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISBLK(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftfile)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISREG(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftdir)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISDIR(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftpipe)
{
    I32 result = my_stat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISFIFO(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftlink)
{
    I32 result = my_lstat(ARGS);
    dSP;
    if (result < 0)
	RETPUSHUNDEF;
    if (S_ISLNK(statcache.st_mode))
	RETPUSHYES;
    RETPUSHNO;
}

PP(pp_ftsuid)
{
    dSP;
#ifdef S_ISUID
    I32 result = my_stat(ARGS);
    SPAGAIN;
    if (result < 0)
	RETPUSHUNDEF;
    if (statcache.st_mode & S_ISUID)
	RETPUSHYES;
#endif
    RETPUSHNO;
}

PP(pp_ftsgid)
{
    dSP;
#ifdef S_ISGID
    I32 result = my_stat(ARGS);
    SPAGAIN;
    if (result < 0)
	RETPUSHUNDEF;
    if (statcache.st_mode & S_ISGID)
	RETPUSHYES;
#endif
    RETPUSHNO;
}

PP(pp_ftsvtx)
{
    dSP;
#ifdef S_ISVTX
    I32 result = my_stat(ARGS);
    SPAGAIN;
    if (result < 0)
	RETPUSHUNDEF;
    if (statcache.st_mode & S_ISVTX)
	RETPUSHYES;
#endif
    RETPUSHNO;
}

PP(pp_fttty)
{
    dSP;
    int fd;
    GV *gv;
    char *tmps;
    if (op->op_flags & OPf_REF) {
	gv = cGVOP->op_gv;
	tmps = "";
    }
    else
	gv = gv_fetchpv(tmps = POPp, FALSE, SVt_PVIO);
    if (gv && GvIO(gv) && IoIFP(GvIO(gv)))
	fd = fileno(IoIFP(GvIO(gv)));
    else if (isDIGIT(*tmps))
	fd = atoi(tmps);
    else
	RETPUSHUNDEF;
    if (isatty(fd))
	RETPUSHYES;
    RETPUSHNO;
}

#if defined(USE_STD_STDIO) || defined(atarist) /* this will work with atariST */
# define FBASE(f) ((f)->_base)
# define FSIZE(f) ((f)->_cnt + ((f)->_ptr - (f)->_base))
# define FPTR(f) ((f)->_ptr)
# define FCOUNT(f) ((f)->_cnt)
#else 
# if defined(USE_LINUX_STDIO)
#   define FBASE(f) ((f)->_IO_read_base)
#   define FSIZE(f) ((f)->_IO_read_end - FBASE(f))
#   define FPTR(f) ((f)->_IO_read_ptr)
#   define FCOUNT(f) ((f)->_IO_read_end - FPTR(f))
# endif
#endif

PP(pp_fttext)
{
    dSP;
    I32 i;
    I32 len;
    I32 odd = 0;
    STDCHAR tbuf[512];
    register STDCHAR *s;
    register IO *io;
    SV *sv;

    if (op->op_flags & OPf_REF) {
	EXTEND(SP, 1);
	if (cGVOP->op_gv == defgv) {
	    if (statgv)
		io = GvIO(statgv);
	    else {
		sv = statname;
		goto really_filename;
	    }
	}
	else {
	    statgv = cGVOP->op_gv;
	    sv_setpv(statname, "");
	    io = GvIO(statgv);
	}
	if (io && IoIFP(io)) {
#ifdef FBASE
	    fstat(fileno(IoIFP(io)), &statcache);
	    if (S_ISDIR(statcache.st_mode))	/* handle NFS glitch */
		if (op->op_type == OP_FTTEXT)
		    RETPUSHNO;
		else
		    RETPUSHYES;
	    if (FCOUNT(IoIFP(io)) <= 0) {
		i = getc(IoIFP(io));
		if (i != EOF)
		    (void)ungetc(i, IoIFP(io));
	    }
	    if (FCOUNT(IoIFP(io)) <= 0)	/* null file is anything */
		RETPUSHYES;
	    len = FSIZE(IoIFP(io));
	    s = FBASE(IoIFP(io));
#else
	    DIE("-T and -B not implemented on filehandles");
#endif
	}
	else {
	    if (dowarn)
		warn("Test on unopened file <%s>",
		  GvENAME(cGVOP->op_gv));
	    errno = EBADF;
	    RETPUSHUNDEF;
	}
    }
    else {
	sv = POPs;
	statgv = Nullgv;
	sv_setpv(statname, SvPV(sv, na));
      really_filename:
#ifdef HAS_OPEN3
	i = open(SvPV(sv, na), O_RDONLY, 0);
#else
	i = open(SvPV(sv, na), 0);
#endif
	if (i < 0) {
	    if (dowarn && strchr(SvPV(sv, na), '\n'))
		warn(warn_nl, "open");
	    RETPUSHUNDEF;
	}
	fstat(i, &statcache);
	len = read(i, tbuf, 512);
	(void)close(i);
	if (len <= 0) {
	    if (S_ISDIR(statcache.st_mode) && op->op_type == OP_FTTEXT)
		RETPUSHNO;		/* special case NFS directories */
	    RETPUSHYES;		/* null file is anything */
	}
	s = tbuf;
    }

    /* now scan s to look for textiness */

    for (i = 0; i < len; i++, s++) {
	if (!*s) {			/* null never allowed in text */
	    odd += len;
	    break;
	}
	else if (*s & 128)
	    odd++;
	else if (*s < 32 &&
	  *s != '\n' && *s != '\r' && *s != '\b' &&
	  *s != '\t' && *s != '\f' && *s != 27)
	    odd++;
    }

    if ((odd * 30 > len) == (op->op_type == OP_FTTEXT)) /* allow 30% odd */
	RETPUSHNO;
    else
	RETPUSHYES;
}

PP(pp_ftbinary)
{
    return pp_fttext(ARGS);
}

/* File calls. */

PP(pp_chdir)
{
    dSP; dTARGET;
    char *tmps;
    SV **svp;

    if (MAXARG < 1)
	tmps = Nullch;
    else
	tmps = POPp;
    if (!tmps || !*tmps) {
	svp = hv_fetch(GvHVn(envgv), "HOME", 4, FALSE);
	if (svp)
	    tmps = SvPV(*svp, na);
    }
    if (!tmps || !*tmps) {
	svp = hv_fetch(GvHVn(envgv), "LOGDIR", 6, FALSE);
	if (svp)
	    tmps = SvPV(*svp, na);
    }
    TAINT_PROPER("chdir");
    PUSHi( chdir(tmps) >= 0 );
    RETURN;
}

PP(pp_chown)
{
    dSP; dMARK; dTARGET;
    I32 value;
#ifdef HAS_CHOWN
    value = (I32)apply(op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    DIE(no_func, "Unsupported function chown");
#endif
}

PP(pp_chroot)
{
    dSP; dTARGET;
    char *tmps;
#ifdef HAS_CHROOT
    tmps = POPp;
    TAINT_PROPER("chroot");
    PUSHi( chroot(tmps) >= 0 );
    RETURN;
#else
    DIE(no_func, "chroot");
#endif
}

PP(pp_unlink)
{
    dSP; dMARK; dTARGET;
    I32 value;
    value = (I32)apply(op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
}

PP(pp_chmod)
{
    dSP; dMARK; dTARGET;
    I32 value;
    value = (I32)apply(op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
}

PP(pp_utime)
{
    dSP; dMARK; dTARGET;
    I32 value;
    value = (I32)apply(op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
}

PP(pp_rename)
{
    dSP; dTARGET;
    int anum;

    char *tmps2 = POPp;
    char *tmps = SvPV(TOPs, na);
    TAINT_PROPER("rename");
#ifdef HAS_RENAME
    anum = rename(tmps, tmps2);
#else
    if (same_dirent(tmps2, tmps))	/* can always rename to same name */
	anum = 1;
    else {
	if (euid || stat(tmps2, &statbuf) < 0 || !S_ISDIR(statbuf.st_mode))
	    (void)UNLINK(tmps2);
	if (!(anum = link(tmps, tmps2)))
	    anum = UNLINK(tmps);
    }
#endif
    SETi( anum >= 0 );
    RETURN;
}

PP(pp_link)
{
    dSP; dTARGET;
#ifdef HAS_LINK
    char *tmps2 = POPp;
    char *tmps = SvPV(TOPs, na);
    TAINT_PROPER("link");
    SETi( link(tmps, tmps2) >= 0 );
#else
    DIE(no_func, "Unsupported function link");
#endif
    RETURN;
}

PP(pp_symlink)
{
    dSP; dTARGET;
#ifdef HAS_SYMLINK
    char *tmps2 = POPp;
    char *tmps = SvPV(TOPs, na);
    TAINT_PROPER("symlink");
    SETi( symlink(tmps, tmps2) >= 0 );
    RETURN;
#else
    DIE(no_func, "symlink");
#endif
}

PP(pp_readlink)
{
    dSP; dTARGET;
#ifdef HAS_SYMLINK
    char *tmps;
    int len;
    tmps = POPp;
    len = readlink(tmps, buf, sizeof buf);
    EXTEND(SP, 1);
    if (len < 0)
	RETPUSHUNDEF;
    PUSHp(buf, len);
    RETURN;
#else
    EXTEND(SP, 1);
    RETSETUNDEF;		/* just pretend it's a normal file */
#endif
}

#if !defined(HAS_MKDIR) || !defined(HAS_RMDIR)
static int
dooneliner(cmd, filename)
char *cmd;
char *filename;
{
    char mybuf[8192];
    char *s, *tmps;
    int anum = 1;
    FILE *myfp;

    strcpy(mybuf, cmd);
    strcat(mybuf, " ");
    for (s = mybuf+strlen(mybuf); *filename; ) {
	*s++ = '\\';
	*s++ = *filename++;
    }
    strcpy(s, " 2>&1");
    myfp = my_popen(mybuf, "r");
    if (myfp) {
	*mybuf = '\0';
	s = fgets(mybuf, sizeof mybuf, myfp);
	(void)my_pclose(myfp);
	if (s != Nullch) {
	    for (errno = 1; errno < sys_nerr; errno++) {
#ifdef HAS_SYS_ERRLIST
		if (instr(mybuf, sys_errlist[errno]))	/* you don't see this */
		    return 0;
#else
		char *errmsg;				/* especially if it isn't there */

		if (instr(mybuf,
		          (errmsg = strerror(errno)) ? errmsg : "NoErRoR"))
		    return 0;
#endif
	    }
	    errno = 0;
#ifndef EACCES
#define EACCES EPERM
#endif
	    if (instr(mybuf, "cannot make"))
		errno = EEXIST;
	    else if (instr(mybuf, "existing file"))
		errno = EEXIST;
	    else if (instr(mybuf, "ile exists"))
		errno = EEXIST;
	    else if (instr(mybuf, "non-exist"))
		errno = ENOENT;
	    else if (instr(mybuf, "does not exist"))
		errno = ENOENT;
	    else if (instr(mybuf, "not empty"))
		errno = EBUSY;
	    else if (instr(mybuf, "cannot access"))
		errno = EACCES;
	    else
		errno = EPERM;
	    return 0;
	}
	else {	/* some mkdirs return no failure indication */
	    anum = (stat(filename, &statbuf) >= 0);
	    if (op->op_type == OP_RMDIR)
		anum = !anum;
	    if (anum)
		errno = 0;
	    else
		errno = EACCES;	/* a guess */
	}
	return anum;
    }
    else
	return 0;
}
#endif

PP(pp_mkdir)
{
    dSP; dTARGET;
    int mode = POPi;
#ifndef HAS_MKDIR
    int oldumask;
#endif
    char *tmps = SvPV(TOPs, na);

    TAINT_PROPER("mkdir");
#ifdef HAS_MKDIR
    SETi( mkdir(tmps, mode) >= 0 );
#else
    SETi( dooneliner("mkdir", tmps) );
    oldumask = umask(0)
    umask(oldumask);
    chmod(tmps, (mode & ~oldumask) & 0777);
#endif
    RETURN;
}

PP(pp_rmdir)
{
    dSP; dTARGET;
    char *tmps;

    tmps = POPp;
    TAINT_PROPER("rmdir");
#ifdef HAS_RMDIR
    XPUSHi( rmdir(tmps) >= 0 );
#else
    XPUSHi( dooneliner("rmdir", tmps) );
#endif
    RETURN;
}

/* Directory calls. */

PP(pp_open_dir)
{
    dSP;
#if defined(Direntry_t) && defined(HAS_READDIR)
    char *dirname = POPp;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io)
	goto nope;

    if (IoDIRP(io))
	closedir(IoDIRP(io));
    if (!(IoDIRP(io) = opendir(dirname)))
	goto nope;

    RETPUSHYES;
nope:
    if (!errno)
	errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "opendir");
#endif
}

PP(pp_readdir)
{
    dSP;
#if defined(Direntry_t) && defined(HAS_READDIR)
#ifndef I_DIRENT
    Direntry_t *readdir _((DIR *));
#endif
    register Direntry_t *dp;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    if (GIMME == G_ARRAY) {
	/*SUPPRESS 560*/
	while (dp = (Direntry_t *)readdir(IoDIRP(io))) {
#ifdef DIRNAMLEN
	    XPUSHs(sv_2mortal(newSVpv(dp->d_name, dp->d_namlen)));
#else
	    XPUSHs(sv_2mortal(newSVpv(dp->d_name, 0)));
#endif
	}
    }
    else {
	if (!(dp = (Direntry_t *)readdir(IoDIRP(io))))
	    goto nope;
#ifdef DIRNAMLEN
	XPUSHs(sv_2mortal(newSVpv(dp->d_name, dp->d_namlen)));
#else
	XPUSHs(sv_2mortal(newSVpv(dp->d_name, 0)));
#endif
    }
    RETURN;

nope:
    if (!errno)
	errno = EBADF;
    if (GIMME == G_ARRAY)
	RETURN;
    else
	RETPUSHUNDEF;
#else
    DIE(no_dir_func, "readdir");
#endif
}

PP(pp_telldir)
{
    dSP; dTARGET;
#if defined(HAS_TELLDIR) || defined(telldir)
#ifndef telldir
    long telldir _((DIR *));
#endif
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    PUSHi( telldir(IoDIRP(io)) );
    RETURN;
nope:
    if (!errno)
	errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "telldir");
#endif
}

PP(pp_seekdir)
{
    dSP;
#if defined(HAS_SEEKDIR) || defined(seekdir)
    long along = POPl;
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    (void)seekdir(IoDIRP(io), along);

    RETPUSHYES;
nope:
    if (!errno)
	errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "seekdir");
#endif
}

PP(pp_rewinddir)
{
    dSP;
#if defined(HAS_REWINDDIR) || defined(rewinddir)
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

    (void)rewinddir(IoDIRP(io));
    RETPUSHYES;
nope:
    if (!errno)
	errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "rewinddir");
#endif
}

PP(pp_closedir)
{
    dSP;
#if defined(Direntry_t) && defined(HAS_READDIR)
    GV *gv = (GV*)POPs;
    register IO *io = GvIOn(gv);

    if (!io || !IoDIRP(io))
	goto nope;

#ifdef VOID_CLOSEDIR
    closedir(IoDIRP(io));
#else
    if (closedir(IoDIRP(io)) < 0)
	goto nope;
#endif
    IoDIRP(io) = 0;

    RETPUSHYES;
nope:
    if (!errno)
	errno = EBADF;
    RETPUSHUNDEF;
#else
    DIE(no_dir_func, "closedir");
#endif
}

/* Process control. */

PP(pp_fork)
{
    dSP; dTARGET;
    int childpid;
    GV *tmpgv;

    EXTEND(SP, 1);
#ifdef HAS_FORK
    childpid = fork();
    if (childpid < 0)
	RETSETUNDEF;
    if (!childpid) {
	/*SUPPRESS 560*/
	if (tmpgv = gv_fetchpv("$", TRUE, SVt_PV))
	    sv_setiv(GvSV(tmpgv), (I32)getpid());
	hv_clear(pidstatus);	/* no kids, so don't wait for 'em */
    }
    PUSHi(childpid);
    RETURN;
#else
    DIE(no_func, "Unsupported function fork");
#endif
}

PP(pp_wait)
{
    dSP; dTARGET;
    int childpid;
    int argflags;
    I32 value;

    EXTEND(SP, 1);
#ifdef HAS_WAIT
    childpid = wait(&argflags);
    if (childpid > 0)
	pidgone(childpid, argflags);
    value = (I32)childpid;
    statusvalue = (U16)argflags;
    PUSHi(value);
    RETURN;
#else
    DIE(no_func, "Unsupported function wait");
#endif
}

PP(pp_waitpid)
{
    dSP; dTARGET;
    int childpid;
    int optype;
    int argflags;
    I32 value;

#ifdef HAS_WAIT
    optype = POPi;
    childpid = TOPi;
    childpid = wait4pid(childpid, &argflags, optype);
    value = (I32)childpid;
    statusvalue = (U16)argflags;
    SETi(value);
    RETURN;
#else
    DIE(no_func, "Unsupported function wait");
#endif
}

PP(pp_system)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    I32 value;
    int childpid;
    int result;
    int status;
    VOIDRET (*ihand)();     /* place to save signal during system() */
    VOIDRET (*qhand)();     /* place to save signal during system() */

#ifdef HAS_FORK
    if (SP - MARK == 1) {
	if (tainting) {
	    char *junk = SvPV(TOPs, na);
	    TAINT_ENV();
	    TAINT_PROPER("system");
	}
    }
    while ((childpid = vfork()) == -1) {
	if (errno != EAGAIN) {
	    value = -1;
	    SP = ORIGMARK;
	    PUSHi(value);
	    RETURN;
	}
	sleep(5);
    }
    if (childpid > 0) {
	ihand = signal(SIGINT, SIG_IGN);
	qhand = signal(SIGQUIT, SIG_IGN);
	result = wait4pid(childpid, &status, 0);
	(void)signal(SIGINT, ihand);
	(void)signal(SIGQUIT, qhand);
	statusvalue = (U16)status;
	if (result < 0)
	    value = -1;
	else {
	    value = (I32)((unsigned int)status & 0xffff);
	}
	do_execfree();	/* free any memory child malloced on vfork */
	SP = ORIGMARK;
	PUSHi(value);
	RETURN;
    }
    if (op->op_flags & OPf_STACKED) {
	SV *really = *++MARK;
	value = (I32)do_aexec(really, MARK, SP);
    }
    else if (SP - MARK != 1)
	value = (I32)do_aexec(Nullsv, MARK, SP);
    else {
	value = (I32)do_exec(SvPVx(sv_mortalcopy(*SP), na));
    }
    _exit(-1);
#else /* ! FORK */
    if ((op[1].op_type & A_MASK) == A_GV)
	value = (I32)do_aspawn(st[1], arglast);
    else if (arglast[2] - arglast[1] != 1)
	value = (I32)do_aspawn(Nullsv, arglast);
    else {
	value = (I32)do_spawn(SvPVx(sv_mortalcopy(st[2]), na));
    }
    PUSHi(value);
#endif /* FORK */
    RETURN;
}

PP(pp_exec)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    I32 value;

    if (op->op_flags & OPf_STACKED) {
	SV *really = *++MARK;
	value = (I32)do_aexec(really, MARK, SP);
    }
    else if (SP - MARK != 1)
	value = (I32)do_aexec(Nullsv, MARK, SP);
    else {
	if (tainting) {
	    char *junk = SvPV(*SP, na);
	    TAINT_ENV();
	    TAINT_PROPER("exec");
	}
	value = (I32)do_exec(SvPVx(sv_mortalcopy(*SP), na));
    }
    SP = ORIGMARK;
    PUSHi(value);
    RETURN;
}

PP(pp_kill)
{
    dSP; dMARK; dTARGET;
    I32 value;
#ifdef HAS_KILL
    value = (I32)apply(op->op_type, MARK, SP);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    DIE(no_func, "Unsupported function kill");
#endif
}

PP(pp_getppid)
{
#ifdef HAS_GETPPID
    dSP; dTARGET;
    XPUSHi( getppid() );
    RETURN;
#else
    DIE(no_func, "getppid");
#endif
}

PP(pp_getpgrp)
{
#ifdef HAS_GETPGRP
    dSP; dTARGET;
    int pid;
    I32 value;

    if (MAXARG < 1)
	pid = 0;
    else
	pid = SvIVx(POPs);
#ifdef _POSIX_SOURCE
    if (pid != 0)
	DIE("POSIX getpgrp can't take an argument");
    value = (I32)getpgrp();
#else
    value = (I32)getpgrp(pid);
#endif
    XPUSHi(value);
    RETURN;
#else
    DIE(no_func, "getpgrp()");
#endif
}

PP(pp_setpgrp)
{
#ifdef HAS_SETPGRP
    dSP; dTARGET;
    int pgrp = POPi;
    int pid = TOPi;

    TAINT_PROPER("setpgrp");
    SETi( setpgrp(pid, pgrp) >= 0 );
    RETURN;
#else
    DIE(no_func, "setpgrp()");
#endif
}

PP(pp_getpriority)
{
    dSP; dTARGET;
    int which;
    int who;
#ifdef HAS_GETPRIORITY
    who = POPi;
    which = TOPi;
    SETi( getpriority(which, who) );
    RETURN;
#else
    DIE(no_func, "getpriority()");
#endif
}

PP(pp_setpriority)
{
    dSP; dTARGET;
    int which;
    int who;
    int niceval;
#ifdef HAS_SETPRIORITY
    niceval = POPi;
    who = POPi;
    which = TOPi;
    TAINT_PROPER("setpriority");
    SETi( setpriority(which, who, niceval) >= 0 );
    RETURN;
#else
    DIE(no_func, "setpriority()");
#endif
}

/* Time calls. */

PP(pp_time)
{
    dSP; dTARGET;
    XPUSHi( time(Null(Time_t*)) );
    RETURN;
}

#ifndef HZ
#define HZ 60
#endif

PP(pp_tms)
{
    dSP;

#if defined(MSDOS) || !defined(HAS_TIMES)
    DIE("times not implemented");
#else
    EXTEND(SP, 4);

    (void)times(&timesbuf);

    PUSHs(sv_2mortal(newSVnv(((double)timesbuf.tms_utime)/HZ)));
    if (GIMME == G_ARRAY) {
	PUSHs(sv_2mortal(newSVnv(((double)timesbuf.tms_stime)/HZ)));
	PUSHs(sv_2mortal(newSVnv(((double)timesbuf.tms_cutime)/HZ)));
	PUSHs(sv_2mortal(newSVnv(((double)timesbuf.tms_cstime)/HZ)));
    }
    RETURN;
#endif /* MSDOS */
}

PP(pp_localtime)
{
    return pp_gmtime(ARGS);
}

PP(pp_gmtime)
{
    dSP;
    Time_t when;
    struct tm *tmbuf;
    static char *dayname[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static char *monname[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (MAXARG < 1)
	(void)time(&when);
    else
	when = (Time_t)SvIVx(POPs);

    if (op->op_type == OP_LOCALTIME)
	tmbuf = localtime(&when);
    else
	tmbuf = gmtime(&when);

    EXTEND(SP, 9);
    if (GIMME != G_ARRAY) {
	dTARGET;
	char mybuf[30];
	if (!tmbuf)
	    RETPUSHUNDEF;
	sprintf(mybuf, "%s %s %2d %02d:%02d:%02d %d",
	    dayname[tmbuf->tm_wday],
	    monname[tmbuf->tm_mon],
	    tmbuf->tm_mday,
	    tmbuf->tm_hour,
	    tmbuf->tm_min,
	    tmbuf->tm_sec,
	    tmbuf->tm_year + 1900);
	PUSHp(mybuf, strlen(mybuf));
    }
    else if (tmbuf) {
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_sec)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_min)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_hour)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_mday)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_mon)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_year)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_wday)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_yday)));
	PUSHs(sv_2mortal(newSViv((I32)tmbuf->tm_isdst)));
    }
    RETURN;
}

PP(pp_alarm)
{
    dSP; dTARGET;
    int anum;
#ifdef HAS_ALARM
    anum = POPi;
    anum = alarm((unsigned int)anum);
    EXTEND(SP, 1);
    if (anum < 0)
	RETPUSHUNDEF;
    PUSHi((I32)anum);
    RETURN;
#else
    DIE(no_func, "Unsupported function alarm");
    break;
#endif
}

PP(pp_sleep)
{
    dSP; dTARGET;
    I32 duration;
    Time_t lasttime;
    Time_t when;

    (void)time(&lasttime);
    if (MAXARG < 1)
	pause();
    else {
	duration = POPi;
	sleep((unsigned int)duration);
    }
    (void)time(&when);
    XPUSHi(when - lasttime);
    RETURN;
}

/* Shared memory. */

PP(pp_shmget)
{
    return pp_semget(ARGS);
}

PP(pp_shmctl)
{
    return pp_semctl(ARGS);
}

PP(pp_shmread)
{
    return pp_shmwrite(ARGS);
}

PP(pp_shmwrite)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    dSP; dMARK; dTARGET;
    I32 value = (I32)(do_shmio(op->op_type, MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    pp_semget(ARGS);
#endif
}

/* Message passing. */

PP(pp_msgget)
{
    return pp_semget(ARGS);
}

PP(pp_msgctl)
{
    return pp_semctl(ARGS);
}

PP(pp_msgsnd)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    dSP; dMARK; dTARGET;
    I32 value = (I32)(do_msgsnd(MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    pp_semget(ARGS);
#endif
}

PP(pp_msgrcv)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    dSP; dMARK; dTARGET;
    I32 value = (I32)(do_msgrcv(MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    pp_semget(ARGS);
#endif
}

/* Semaphores. */

PP(pp_semget)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    dSP; dMARK; dTARGET;
    int anum = do_ipcget(op->op_type, MARK, SP);
    SP = MARK;
    if (anum == -1)
	RETPUSHUNDEF;
    PUSHi(anum);
    RETURN;
#else
    DIE("System V IPC is not implemented on this machine");
#endif
}

PP(pp_semctl)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    dSP; dMARK; dTARGET;
    int anum = do_ipcctl(op->op_type, MARK, SP);
    SP = MARK;
    if (anum == -1)
	RETSETUNDEF;
    if (anum != 0) {
	PUSHi(anum);
    }
    else {
	PUSHp("0 but true",10);
    }
    RETURN;
#else
    pp_semget(ARGS);
#endif
}

PP(pp_semop)
{
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
    dSP; dMARK; dTARGET;
    I32 value = (I32)(do_semop(MARK, SP) >= 0);
    SP = MARK;
    PUSHi(value);
    RETURN;
#else
    pp_semget(ARGS);
#endif
}

/* Eval. */

static void
save_lines(array, sv)
AV *array;
SV *sv;
{
    register char *s = SvPVX(sv);
    register char *send = SvPVX(sv) + SvCUR(sv);
    register char *t;
    register I32 line = 1;

    while (s && s < send) {
	SV *tmpstr = NEWSV(85,0);

	sv_upgrade(tmpstr, SVt_PVMG);
	t = strchr(s, '\n');
	if (t)
	    t++;
	else
	    t = send;

	sv_setpvn(tmpstr, s, t - s);
	av_store(array, line++, tmpstr);
	s = t;
    }
}

static OP *
doeval()
{
    dSP;
    OP *saveop = op;
    HV *newstash;

    in_eval = 1;

    /* set up a scratch pad */

    SAVEINT(padix);
    SAVESPTR(curpad);
    SAVESPTR(comppad);
    SAVESPTR(comppad_name);
    SAVEINT(comppad_name_fill);
    SAVEINT(min_intro_pending);
    SAVEINT(max_intro_pending);
    comppad = newAV();
    comppad_name = newAV();
    comppad_name_fill = 0;
    min_intro_pending = 0;
    av_push(comppad, Nullsv);
    curpad = AvARRAY(comppad);
    padix = 0;

    /* make sure we compile in the right package */

    newstash = curcop->cop_stash;
    if (curstash != newstash) {
	SAVESPTR(curstash);
	curstash = newstash;
    }
    SAVESPTR(beginav);
    beginav = 0;

    /* try to compile it */

    eval_root = Nullop;
    error_count = 0;
    curcop = &compiling;
    curcop->cop_arybase = 0;
    rs = "\n";
    rslen = 1;
    rschar = '\n';
    rspara = 0;
    sv_setpv(GvSV(gv_fetchpv("@",TRUE, SVt_PV)),"");
    if (yyparse() || error_count || !eval_root) {
	SV **newsp;
	I32 gimme;
	CONTEXT *cx;
	I32 optype;

	op = saveop;
	if (eval_root) {
	    op_free(eval_root);
	    eval_root = Nullop;
	}
	POPBLOCK(cx,curpm);
	POPEVAL(cx);
	pop_return();
	lex_end();
	LEAVE;
	if (optype == OP_REQUIRE)
	    DIE("%s", SvPVx(GvSV(gv_fetchpv("@",TRUE, SVt_PV)), na));
	rs = nrs;
	rslen = nrslen;
	rschar = nrschar;
	rspara = (nrslen == 2);
	RETPUSHUNDEF;
    }
    rs = nrs;
    rslen = nrslen;
    rschar = nrschar;
    rspara = (nrslen == 2);
    compiling.cop_line = 0;
    SAVEFREESV(comppad_name);
    SAVEFREESV(comppad);
    SAVEFREEOP(eval_root);

    DEBUG_x(dump_eval());

    /* compiled okay, so do it */

    RETURNOP(eval_start);
}

PP(pp_require)
{
    dSP;
    register CONTEXT *cx;
    SV *sv;
    char *name;
    char *tmpname;
    SV** svp;
    I32 gimme = G_SCALAR;
    FILE *tryrsfp = 0;

    sv = POPs;
    if (SvNIOK(sv) && !SvPOKp(sv)) {
	if (atof(patchlevel) + 0.000999 < SvNV(sv))
	    DIE("Perl %3.3f required--this is only version %s, stopped",
		SvNV(sv),patchlevel);
	RETPUSHYES;
    }
    name = SvPV(sv, na);
    if (!*name)
	DIE("Null filename used");
    if (op->op_type == OP_REQUIRE &&
      (svp = hv_fetch(GvHVn(incgv), name, SvCUR(sv), 0)) &&
      *svp != &sv_undef)
	RETPUSHYES;

    /* prepare to compile file */

    tmpname = savestr(name);
    if (*tmpname == '/' ||
	(*tmpname == '.' && 
	    (tmpname[1] == '/' ||
	     (tmpname[1] == '.' && tmpname[2] == '/'))))
    {
	tryrsfp = fopen(tmpname,"r");
    }
    else {
	AV *ar = GvAVn(incgv);
	I32 i;

	for (i = 0; i <= AvFILL(ar); i++) {
	    (void)sprintf(buf, "%s/%s",
		SvPVx(*av_fetch(ar, i, TRUE), na), name);
	    tryrsfp = fopen(buf, "r");
	    if (tryrsfp) {
		char *s = buf;

		if (*s == '.' && s[1] == '/')
		    s += 2;
		Safefree(tmpname);
		tmpname = savestr(s);
		break;
	    }
	}
    }
    compiling.cop_filegv = gv_fetchfile(tmpname);
    Safefree(tmpname);
    tmpname = Nullch;
    if (!tryrsfp) {
	if (op->op_type == OP_REQUIRE) {
	    sprintf(tokenbuf,"Can't locate %s in @INC", name);
	    if (instr(tokenbuf,".h "))
		strcat(tokenbuf," (change .h to .ph maybe?)");
	    if (instr(tokenbuf,".ph "))
		strcat(tokenbuf," (did you run h2ph?)");
	    DIE("%s",tokenbuf);
	}

	RETPUSHUNDEF;
    }

    /* Assume success here to prevent recursive requirement. */
    (void)hv_store(GvHVn(incgv), name, strlen(name),
	newSVsv(GvSV(compiling.cop_filegv)), 0 );

    ENTER;
    SAVETMPS;
    lex_start(sv_2mortal(newSVpv("",0)));
    rsfp = tryrsfp;
    name = savestr(name);
    SAVEFREEPV(name);
    SAVEI32(hints);
    hints = 0;
 
    /* switch to eval mode */

    push_return(op->op_next);
    PUSHBLOCK(cx, CXt_EVAL, SP);
    PUSHEVAL(cx, name, compiling.cop_filegv);

    compiling.cop_line = 0;

    PUTBACK;
    return doeval();
}

PP(pp_dofile)
{
    return pp_require(ARGS);
}

PP(pp_entereval)
{
    dSP;
    register CONTEXT *cx;
    dPOPss;
    I32 gimme = GIMME;
    char tmpbuf[32];
    STRLEN len;

    if (!SvPV(sv,len) || !len)
	RETPUSHUNDEF;

    ENTER;
    SAVETMPS;
    lex_start(sv);
 
    /* switch to eval mode */

    sprintf(tmpbuf, "_<(eval %d)", ++evalseq);
    compiling.cop_filegv = gv_fetchfile(tmpbuf+2);
    compiling.cop_line = 1;
    SAVEDELETE(defstash, savestr(tmpbuf), strlen(tmpbuf));

    push_return(op->op_next);
    PUSHBLOCK(cx, CXt_EVAL, SP);
    PUSHEVAL(cx, 0, compiling.cop_filegv);

    /* prepare to compile string */

    if (perldb && curstash != debstash)
	save_lines(GvAV(compiling.cop_filegv), linestr);
    PUTBACK;
    return doeval();
}

PP(pp_leaveeval)
{
    dSP;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register CONTEXT *cx;
    OP *retop;
    I32 optype;

    POPBLOCK(cx,newpm);
    POPEVAL(cx);
    retop = pop_return();

    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (SvFLAGS(TOPs) & SVs_TEMP)
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	}
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
	SP = MARK;
    }
    else {
	for (mark = newsp + 1; mark <= SP; mark++)
	    if (!(SvFLAGS(TOPs) & SVs_TEMP))
		*mark = sv_mortalcopy(*mark);
		/* in case LEAVE wipes old return values */
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    if (optype != OP_ENTEREVAL) {
	char *name = cx->blk_eval.old_name;

	if (!(gimme == G_SCALAR ? SvTRUE(*sp) : sp > newsp)) {
	    /* Unassume the success we assumed earlier. */
	    (void)hv_delete(GvHVn(incgv), name, strlen(name));

	    if (optype == OP_REQUIRE)
		retop = die("%s did not return a true value", name);
	}
    }

    lex_end();
    LEAVE;
    sv_setpv(GvSV(gv_fetchpv("@",TRUE, SVt_PV)),"");

    RETURNOP(retop);
}

#ifdef NOTYET
PP(pp_evalonce)
{
    dSP;
    SP = do_eval(st[1], OP_EVAL, curcop->cop_stash, TRUE,
	GIMME, arglast);
    if (eval_root) {
	SvREFCNT_dec(cSVOP->op_sv);
	op[1].arg_ptr.arg_cmd = eval_root;
	op[1].op_type = (A_CMD|A_DONT);
	op[0].op_type = OP_TRY;
    }
    RETURN;
}
#endif

PP(pp_entertry)
{
    dSP;
    register CONTEXT *cx;
    I32 gimme = GIMME;

    ENTER;
    SAVETMPS;

    push_return(cLOGOP->op_other->op_next);
    PUSHBLOCK(cx, CXt_EVAL, SP);
    PUSHEVAL(cx, 0, 0);
    eval_root = op;		/* Only needed so that goto works right. */

    in_eval = 1;
    sv_setpv(GvSV(gv_fetchpv("@",TRUE, SVt_PV)),"");
    RETURN;
}

PP(pp_leavetry)
{
    dSP;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register CONTEXT *cx;
    I32 optype;

    POPBLOCK(cx,newpm);
    POPEVAL(cx);
    pop_return();

    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	}
	else {
	    MEXTEND(mark,0);
	    *MARK = &sv_undef;
	}
	SP = MARK;
    }
    else {
	for (mark = newsp + 1; mark <= SP; mark++)
	    if (!(SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP)))
		*mark = sv_mortalcopy(*mark);
		/* in case LEAVE wipes old return values */
    }
    curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;
    sv_setpv(GvSV(gv_fetchpv("@",TRUE, SVt_PV)),"");
    RETURN;
}

/* Get system info. */

PP(pp_ghbyname)
{
#ifdef HAS_SOCKET
    return pp_ghostent(ARGS);
#else
    DIE(no_sock_func, "gethostbyname");
#endif
}

PP(pp_ghbyaddr)
{
#ifdef HAS_SOCKET
    return pp_ghostent(ARGS);
#else
    DIE(no_sock_func, "gethostbyaddr");
#endif
}

PP(pp_ghostent)
{
    dSP;
#ifdef HAS_SOCKET
    I32 which = op->op_type;
    register char **elem;
    register SV *sv;
    struct hostent *gethostbyname();
    struct hostent *gethostbyaddr();
#ifdef HAS_GETHOSTENT
    struct hostent *gethostent();
#endif
    struct hostent *hent;
    unsigned long len;

    EXTEND(SP, 10);
    if (which == OP_GHBYNAME) {
	hent = gethostbyname(POPp);
    }
    else if (which == OP_GHBYADDR) {
	int addrtype = POPi;
	SV *addrstr = POPs;
	STRLEN addrlen;
	char *addr = SvPV(addrstr, addrlen);

	hent = gethostbyaddr(addr, addrlen, addrtype);
    }
    else
#ifdef HAS_GETHOSTENT
	hent = gethostent();
#else
	DIE("gethostent not implemented");
#endif

#ifdef HOST_NOT_FOUND
    if (!hent)
	statusvalue = (U16)h_errno & 0xffff;
#endif

    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (hent) {
	    if (which == OP_GHBYNAME) {
		sv_setpvn(sv, hent->h_addr, hent->h_length);
	    }
	    else
		sv_setpv(sv, hent->h_name);
	}
	RETURN;
    }

    if (hent) {
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, hent->h_name);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	for (elem = hent->h_aliases; *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)hent->h_addrtype);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	len = hent->h_length;
	sv_setiv(sv, (I32)len);
#ifdef h_addr
	for (elem = hent->h_addr_list; *elem; elem++) {
	    XPUSHs(sv = sv_mortalcopy(&sv_no));
	    sv_setpvn(sv, *elem, len);
	}
#else
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpvn(sv, hent->h_addr, len);
#endif /* h_addr */
    }
    RETURN;
#else
    DIE(no_sock_func, "gethostent");
#endif
}

PP(pp_gnbyname)
{
#ifdef HAS_SOCKET
    return pp_gnetent(ARGS);
#else
    DIE(no_sock_func, "getnetbyname");
#endif
}

PP(pp_gnbyaddr)
{
#ifdef HAS_SOCKET
    return pp_gnetent(ARGS);
#else
    DIE(no_sock_func, "getnetbyaddr");
#endif
}

PP(pp_gnetent)
{
    dSP;
#ifdef HAS_SOCKET
    I32 which = op->op_type;
    register char **elem;
    register SV *sv;
    struct netent *getnetbyname();
    struct netent *getnetbyaddr();
    struct netent *getnetent();
    struct netent *nent;

    if (which == OP_GNBYNAME)
	nent = getnetbyname(POPp);
    else if (which == OP_GNBYADDR) {
	int addrtype = POPi;
	unsigned long addr = U_L(POPn);
	nent = getnetbyaddr((long)addr, addrtype);
    }
    else
	nent = getnetent();

    EXTEND(SP, 4);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (nent) {
	    if (which == OP_GNBYNAME)
		sv_setiv(sv, (I32)nent->n_net);
	    else
		sv_setpv(sv, nent->n_name);
	}
	RETURN;
    }

    if (nent) {
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, nent->n_name);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	for (elem = nent->n_aliases; *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)nent->n_addrtype);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)nent->n_net);
    }

    RETURN;
#else
    DIE(no_sock_func, "getnetent");
#endif
}

PP(pp_gpbyname)
{
#ifdef HAS_SOCKET
    return pp_gprotoent(ARGS);
#else
    DIE(no_sock_func, "getprotobyname");
#endif
}

PP(pp_gpbynumber)
{
#ifdef HAS_SOCKET
    return pp_gprotoent(ARGS);
#else
    DIE(no_sock_func, "getprotobynumber");
#endif
}

PP(pp_gprotoent)
{
    dSP;
#ifdef HAS_SOCKET
    I32 which = op->op_type;
    register char **elem;
    register SV *sv;
    struct protoent *getprotobyname();
    struct protoent *getprotobynumber();
    struct protoent *getprotoent();
    struct protoent *pent;

    if (which == OP_GPBYNAME)
	pent = getprotobyname(POPp);
    else if (which == OP_GPBYNUMBER)
	pent = getprotobynumber(POPi);
    else
	pent = getprotoent();

    EXTEND(SP, 3);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (pent) {
	    if (which == OP_GPBYNAME)
		sv_setiv(sv, (I32)pent->p_proto);
	    else
		sv_setpv(sv, pent->p_name);
	}
	RETURN;
    }

    if (pent) {
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, pent->p_name);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	for (elem = pent->p_aliases; *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)pent->p_proto);
    }

    RETURN;
#else
    DIE(no_sock_func, "getprotoent");
#endif
}

PP(pp_gsbyname)
{
#ifdef HAS_SOCKET
    return pp_gservent(ARGS);
#else
    DIE(no_sock_func, "getservbyname");
#endif
}

PP(pp_gsbyport)
{
#ifdef HAS_SOCKET
    return pp_gservent(ARGS);
#else
    DIE(no_sock_func, "getservbyport");
#endif
}

PP(pp_gservent)
{
    dSP;
#ifdef HAS_SOCKET
    I32 which = op->op_type;
    register char **elem;
    register SV *sv;
    struct servent *getservbyname();
    struct servent *getservbynumber();
    struct servent *getservent();
    struct servent *sent;

    if (which == OP_GSBYNAME) {
	char *proto = POPp;
	char *name = POPp;

	if (proto && !*proto)
	    proto = Nullch;

	sent = getservbyname(name, proto);
    }
    else if (which == OP_GSBYPORT) {
	char *proto = POPp;
	int port = POPi;

	sent = getservbyport(port, proto);
    }
    else
	sent = getservent();

    EXTEND(SP, 4);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (sent) {
	    if (which == OP_GSBYNAME) {
#ifdef HAS_NTOHS
		sv_setiv(sv, (I32)ntohs(sent->s_port));
#else
		sv_setiv(sv, (I32)(sent->s_port));
#endif
	    }
	    else
		sv_setpv(sv, sent->s_name);
	}
	RETURN;
    }

    if (sent) {
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, sent->s_name);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	for (elem = sent->s_aliases; *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
	PUSHs(sv = sv_mortalcopy(&sv_no));
#ifdef HAS_NTOHS
	sv_setiv(sv, (I32)ntohs(sent->s_port));
#else
	sv_setiv(sv, (I32)(sent->s_port));
#endif
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, sent->s_proto);
    }

    RETURN;
#else
    DIE(no_sock_func, "getservent");
#endif
}

PP(pp_shostent)
{
    dSP;
#ifdef HAS_SOCKET
    sethostent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "sethostent");
#endif
}

PP(pp_snetent)
{
    dSP;
#ifdef HAS_SOCKET
    setnetent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "setnetent");
#endif
}

PP(pp_sprotoent)
{
    dSP;
#ifdef HAS_SOCKET
    setprotoent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "setprotoent");
#endif
}

PP(pp_sservent)
{
    dSP;
#ifdef HAS_SOCKET
    setservent(TOPi);
    RETSETYES;
#else
    DIE(no_sock_func, "setservent");
#endif
}

PP(pp_ehostent)
{
    dSP;
#ifdef HAS_SOCKET
    endhostent();
    EXTEND(sp,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endhostent");
#endif
}

PP(pp_enetent)
{
    dSP;
#ifdef HAS_SOCKET
    endnetent();
    EXTEND(sp,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endnetent");
#endif
}

PP(pp_eprotoent)
{
    dSP;
#ifdef HAS_SOCKET
    endprotoent();
    EXTEND(sp,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endprotoent");
#endif
}

PP(pp_eservent)
{
    dSP;
#ifdef HAS_SOCKET
    endservent();
    EXTEND(sp,1);
    RETPUSHYES;
#else
    DIE(no_sock_func, "endservent");
#endif
}

PP(pp_gpwnam)
{
#ifdef HAS_PASSWD
    return pp_gpwent(ARGS);
#else
    DIE(no_func, "getpwnam");
#endif
}

PP(pp_gpwuid)
{
#ifdef HAS_PASSWD
    return pp_gpwent(ARGS);
#else
    DIE(no_func, "getpwuid");
#endif
}

PP(pp_gpwent)
{
    dSP;
#ifdef HAS_PASSWD
    I32 which = op->op_type;
    register SV *sv;
    struct passwd *pwent;

    if (which == OP_GPWNAM)
	pwent = getpwnam(POPp);
    else if (which == OP_GPWUID)
	pwent = getpwuid(POPi);
    else
	pwent = (struct passwd *)getpwent();

    EXTEND(SP, 10);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (pwent) {
	    if (which == OP_GPWNAM)
		sv_setiv(sv, (I32)pwent->pw_uid);
	    else
		sv_setpv(sv, pwent->pw_name);
	}
	RETURN;
    }

    if (pwent) {
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, pwent->pw_name);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, pwent->pw_passwd);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)pwent->pw_uid);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)pwent->pw_gid);
	PUSHs(sv = sv_mortalcopy(&sv_no));
#ifdef PWCHANGE
	sv_setiv(sv, (I32)pwent->pw_change);
#else
#ifdef PWQUOTA
	sv_setiv(sv, (I32)pwent->pw_quota);
#else
#ifdef PWAGE
	sv_setpv(sv, pwent->pw_age);
#endif
#endif
#endif
	PUSHs(sv = sv_mortalcopy(&sv_no));
#ifdef PWCLASS
	sv_setpv(sv, pwent->pw_class);
#else
#ifdef PWCOMMENT
	sv_setpv(sv, pwent->pw_comment);
#endif
#endif
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, pwent->pw_gecos);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, pwent->pw_dir);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, pwent->pw_shell);
#ifdef PWEXPIRE
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)pwent->pw_expire);
#endif
    }
    RETURN;
#else
    DIE(no_func, "getpwent");
#endif
}

PP(pp_spwent)
{
    dSP;
#ifdef HAS_PASSWD
    setpwent();
    RETPUSHYES;
#else
    DIE(no_func, "setpwent");
#endif
}

PP(pp_epwent)
{
    dSP;
#ifdef HAS_PASSWD
    endpwent();
    RETPUSHYES;
#else
    DIE(no_func, "endpwent");
#endif
}

PP(pp_ggrnam)
{
#ifdef HAS_GROUP
    return pp_ggrent(ARGS);
#else
    DIE(no_func, "getgrnam");
#endif
}

PP(pp_ggrgid)
{
#ifdef HAS_GROUP
    return pp_ggrent(ARGS);
#else
    DIE(no_func, "getgrgid");
#endif
}

PP(pp_ggrent)
{
    dSP;
#ifdef HAS_GROUP
    I32 which = op->op_type;
    register char **elem;
    register SV *sv;
    struct group *grent;

    if (which == OP_GGRNAM)
	grent = getgrnam(POPp);
    else if (which == OP_GGRGID)
	grent = getgrgid(POPi);
    else
	grent = (struct group *)getgrent();

    EXTEND(SP, 4);
    if (GIMME != G_ARRAY) {
	PUSHs(sv = sv_newmortal());
	if (grent) {
	    if (which == OP_GGRNAM)
		sv_setiv(sv, (I32)grent->gr_gid);
	    else
		sv_setpv(sv, grent->gr_name);
	}
	RETURN;
    }

    if (grent) {
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, grent->gr_name);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setpv(sv, grent->gr_passwd);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	sv_setiv(sv, (I32)grent->gr_gid);
	PUSHs(sv = sv_mortalcopy(&sv_no));
	for (elem = grent->gr_mem; *elem; elem++) {
	    sv_catpv(sv, *elem);
	    if (elem[1])
		sv_catpvn(sv, " ", 1);
	}
    }

    RETURN;
#else
    DIE(no_func, "getgrent");
#endif
}

PP(pp_sgrent)
{
    dSP;
#ifdef HAS_GROUP
    setgrent();
    RETPUSHYES;
#else
    DIE(no_func, "setgrent");
#endif
}

PP(pp_egrent)
{
    dSP;
#ifdef HAS_GROUP
    endgrent();
    RETPUSHYES;
#else
    DIE(no_func, "endgrent");
#endif
}

PP(pp_getlogin)
{
    dSP; dTARGET;
#ifdef HAS_GETLOGIN
    char *tmps;
    EXTEND(SP, 1);
    if (!(tmps = getlogin()))
	RETPUSHUNDEF;
    PUSHp(tmps, strlen(tmps));
    RETURN;
#else
    DIE(no_func, "getlogin");
#endif
}

/* Miscellaneous. */

PP(pp_syscall)
{
#ifdef HAS_SYSCALL
    dSP; dMARK; dORIGMARK; dTARGET;
    register I32 items = SP - MARK;
    unsigned long a[20];
    register I32 i = 0;
    I32 retval = -1;

    if (tainting) {
	while (++MARK <= SP) {
	    if (SvGMAGICAL(*MARK) && SvSMAGICAL(*MARK) && mg_find(*MARK, 't'))
		tainted = TRUE;
	}
	MARK = ORIGMARK;
	TAINT_PROPER("syscall");
    }

    /* This probably won't work on machines where sizeof(long) != sizeof(int)
     * or where sizeof(long) != sizeof(char*).  But such machines will
     * not likely have syscall implemented either, so who cares?
     */
    while (++MARK <= SP) {
	if (SvNIOK(*MARK) || !i)
	    a[i++] = SvIV(*MARK);
	else
	    a[i++] = (unsigned long)SvPVX(*MARK);
	if (i > 15)
	    break;
    }
    switch (items) {
    default:
	DIE("Too many args to syscall");
    case 0:
	DIE("Too few args to syscall");
    case 1:
	retval = syscall(a[0]);
	break;
    case 2:
	retval = syscall(a[0],a[1]);
	break;
    case 3:
	retval = syscall(a[0],a[1],a[2]);
	break;
    case 4:
	retval = syscall(a[0],a[1],a[2],a[3]);
	break;
    case 5:
	retval = syscall(a[0],a[1],a[2],a[3],a[4]);
	break;
    case 6:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5]);
	break;
    case 7:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6]);
	break;
    case 8:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
	break;
#ifdef atarist
    case 9:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]);
	break;
    case 10:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]);
	break;
    case 11:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10]);
	break;
    case 12:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10],a[11]);
	break;
    case 13:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10],a[11],a[12]);
	break;
    case 14:
	retval = syscall(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],
	  a[10],a[11],a[12],a[13]);
	break;
#endif /* atarist */
    }
    SP = ORIGMARK;
    PUSHi(retval);
    RETURN;
#else
    DIE(no_func, "syscall");
#endif
}

