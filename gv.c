/* $RCSfile: gv.c,v $$Revision: 4.1 $$Date: 92/08/07 18:26:39 $
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	gv.c,v $
 */

/*
 *   'Mercy!' cried Gandalf.  'If the giving of information is to be the cure
 * of your inquisitiveness, I shall spend all the rest of my days answering
 * you.  What more do you want to know?'
 *   'The names of all the stars, and of all living things, and the whole
 * history of of Middle-earth and Over-heaven and of the Sundering Seas,'
 * laughed Pippin.
 */

#include "EXTERN.h"
#include "perl.h"

extern char rcsid[];

GV *
gv_AVadd(gv)
register GV *gv;
{
    if (!GvAV(gv))
	GvAV(gv) = newAV();
    return gv;
}

GV *
gv_HVadd(gv)
register GV *gv;
{
    if (!GvHV(gv))
	GvHV(gv) = newHV();
    return gv;
}

GV *
gv_fetchfile(name)
char *name;
{
    char tmpbuf[1200];
    GV *gv;

    sprintf(tmpbuf,"::_<%s", name);
    gv = gv_fetchpv(tmpbuf, TRUE, SVt_PVGV);
    sv_setpv(GvSV(gv), name);
    if (*name == '/')
	SvMULTI_on(gv);
    if (perldb)
	hv_magic(GvHVn(gv_AVadd(gv)), gv, 'L');
    return gv;
}

void
gv_init(gv, stash, name, len, multi)
GV *gv;
HV *stash;
char *name;
STRLEN len;
int multi;
{
    register GP *gp;

    sv_upgrade(gv, SVt_PVGV);
    if (SvLEN(gv))
	Safefree(SvPVX(gv));
    Newz(602,gp, 1, GP);
    GvGP(gv) = gp_ref(gp);
    GvREFCNT(gv) = 1;
    GvSV(gv) = NEWSV(72,0);
    GvLINE(gv) = curcop->cop_line;
    GvFILEGV(gv) = curcop->cop_filegv;
    GvEGV(gv) = gv;
    sv_magic((SV*)gv, (SV*)gv, '*', name, len);
    GvSTASH(gv) = stash;
    GvNAME(gv) = nsavestr(name, len);
    GvNAMELEN(gv) = len;
    if (multi)
	SvMULTI_on(gv);
}

GV *
gv_fetchmeth(stash, name, len, level)
HV* stash;
char* name;
STRLEN len;
I32 level;
{
    AV* av;
    GV* topgv;
    GV* gv;
    GV** gvp;
    HV* lastchance;

    if (!stash)
	return 0;
    if (level > 100)
	croak("Recursive inheritance detected");

    gvp = (GV**)hv_fetch(stash, name, len, TRUE);

    DEBUG_o( deb("Looking for method %s in package %s\n",name,HvNAME(stash)) );
    topgv = *gvp;
    if (SvTYPE(topgv) != SVt_PVGV)
	gv_init(topgv, stash, name, len, TRUE);

    if (GvCV(topgv)) {
	if (!GvCVGEN(topgv) || GvCVGEN(topgv) >= sub_generation)
	    return topgv;
    }

    gvp = (GV**)hv_fetch(stash,"ISA",3,FALSE);
    if (gvp && (gv = *gvp) != (GV*)&sv_undef && (av = GvAV(gv))) {
	SV** svp = AvARRAY(av);
	I32 items = AvFILL(av) + 1;
	while (items--) {
	    SV* sv = *svp++;
	    HV* basestash = gv_stashsv(sv, FALSE);
	    if (!basestash) {
		if (dowarn)
		    warn("Can't locate package %s for @%s'ISA",
			SvPVX(sv), HvNAME(stash));
		continue;
	    }
	    gv = gv_fetchmeth(basestash, name, len, level + 1);
	    if (gv) {
		GvCV(topgv) = GvCV(gv);			/* cache the CV */
		GvCVGEN(topgv) = sub_generation;	/* valid for now */
		return gv;
	    }
	}
    }

    if (!level) {
	if (lastchance = gv_stashpv("UNIVERSAL", FALSE)) {
	    if (gv = gv_fetchmeth(lastchance, name, len, level + 1)) {
		GvCV(topgv) = GvCV(gv);			/* cache the CV */
		GvCVGEN(topgv) = sub_generation;	/* valid for now */
		return gv;
	    }
	}
    }

    return 0;
}

GV *
gv_fetchmethod(stash, name)
HV* stash;
char* name;
{
    register char *nend;
    char *nsplit = 0;
    GV* gv;
    
    for (nend = name; *nend; nend++) {
	if (*nend == ':' || *nend == '\'')
	    nsplit = nend;
    }
    if (nsplit) {
	char ch;
	char *origname = name;
	name = nsplit + 1;
	ch = *nsplit;
	if (*nsplit == ':')
	    --nsplit;
	*nsplit = '\0';
	stash = gv_stashpv(origname,TRUE);
	*nsplit = ch;
    }
    gv = gv_fetchmeth(stash, name, nend - name, 0);
    if (!gv) {
	if (strEQ(name,"import") || strEQ(name,"unimport"))
	    gv = &sv_yes;
	else if (strNE(name, "AUTOLOAD")) {
	    gv = gv_fetchmeth(stash, "AUTOLOAD", 8, 0);
	    if (gv && GvCV(gv)) { /* One more chance... */
		SV *tmpstr = sv_newmortal();
		sv_catpv(tmpstr,HvNAME(stash));
		sv_catpvn(tmpstr,"::", 2);
		sv_catpvn(tmpstr, name, nend - name);
		sv_setsv(GvSV(gv), tmpstr);
	    }
	}
    }
    return gv;
}

HV*
gv_stashpv(name,create)
char *name;
I32 create;
{
    char tmpbuf[1234];
    HV *stash;
    GV *tmpgv;
    sprintf(tmpbuf,"%.*s::",1200,name);
    tmpgv = gv_fetchpv(tmpbuf,create, SVt_PVHV);
    if (!tmpgv)
	return 0;
    if (!GvHV(tmpgv))
	GvHV(tmpgv) = newHV();
    stash = GvHV(tmpgv);
    if (!HvNAME(stash))
	HvNAME(stash) = savestr(name);
    return stash;
}

HV*
gv_stashsv(sv,create)
SV *sv;
I32 create;
{
    return gv_stashpv(SvPV(sv,na), create);
}


GV *
gv_fetchpv(nambeg,add,sv_type)
char *nambeg;
I32 add;
I32 sv_type;
{
    register char *name = nambeg;
    register GV *gv = 0;
    GV**gvp;
    I32 len;
    register char *namend;
    HV *stash = 0;
    bool global = FALSE;
    char *tmpbuf;

    for (namend = name; *namend; namend++) {
	if ((*namend == '\'' && namend[1]) ||
	    (*namend == ':' && namend[1] == ':'))
	{
	    if (!stash)
		stash = defstash;
	    if (!SvREFCNT(stash))	/* symbol table under destruction */
		return Nullgv;

	    len = namend - name;
	    if (len > 0) {
		New(601, tmpbuf, len+2, char);
		*tmpbuf = '_';
		Copy(name, tmpbuf+1, len, char);
		tmpbuf[++len] = '\0';
		gvp = (GV**)hv_fetch(stash,tmpbuf,len,add);
		Safefree(tmpbuf);
		if (!gvp || *gvp == (GV*)&sv_undef)
		    return Nullgv;
		gv = *gvp;

		if (SvTYPE(gv) == SVt_PVGV)
		    SvMULTI_on(gv);
		else if (!add)
		    return Nullgv;
		else
		    gv_init(gv, stash, nambeg, namend - nambeg, (add & 2));

		if (!(stash = GvHV(gv)))
		    stash = GvHV(gv) = newHV();

		if (!HvNAME(stash))
		    HvNAME(stash) = nsavestr(nambeg, namend - nambeg);
	    }

	    if (*namend == ':')
		namend++;
	    namend++;
	    name = namend;
	    if (!*name)
		return gv ? gv : defgv;
	}
    }

    /* No stash in name, so see how we can default */

    if (!stash) {
	if (isIDFIRST(*name)) {
	    if (isUPPER(*name)) {
		if (*name > 'I') {
		    if (*name == 'S' && (
		      strEQ(name, "SIG") ||
		      strEQ(name, "STDIN") ||
		      strEQ(name, "STDOUT") ||
		      strEQ(name, "STDERR") ))
			global = TRUE;
		}
		else if (*name > 'E') {
		    if (*name == 'I' && strEQ(name, "INC"))
			global = TRUE;
		}
		else if (*name > 'A') {
		    if (*name == 'E' && strEQ(name, "ENV"))
			global = TRUE;
		}
		else if (*name == 'A' && (
		  strEQ(name, "ARGV") ||
		  strEQ(name, "ARGVOUT") ))
		    global = TRUE;
	    }
	    else if (*name == '_' && !name[1])
		global = TRUE;
	    if (global)
		stash = defstash;
	    else if ((COP*)curcop == &compiling) {
		if (!(hints & HINT_STRICT_VARS) || sv_type == SVt_PVCV)
		    stash = curstash;
	    }
	    else
		stash = curcop->cop_stash;
	}
	else
	    stash = defstash;
    }
    if (!SvREFCNT(stash))	/* symbol table under destruction */
	return Nullgv;

    /* By this point we should have a stash and a name */

    if (!stash)
	croak("Global symbol \"%s\" requires explicit package name", name);
    len = namend - name;
    if (!len)
	len = 1;
    gvp = (GV**)hv_fetch(stash,name,len,add);
    if (!gvp || *gvp == (GV*)&sv_undef)
	return Nullgv;
    gv = *gvp;
    if (SvTYPE(gv) == SVt_PVGV) {
	SvMULTI_on(gv);
	return gv;
    }

    /* Adding a new symbol */

    gv_init(gv, stash, name, len, add & 2);

    switch (sv_type) {
    case SVt_PVIO:
	GvIO(gv) = newIO();
	break;
    }

    /* set up magic where warranted */
    switch (*name) {
    case 'a':
    case 'b':
	if (len == 1)
	    SvMULTI_on(gv);
	break;
    case 'I':
	if (strEQ(name, "ISA")) {
	    AV* av = GvAVn(gv);
	    SvMULTI_on(gv);
	    sv_magic((SV*)av, (SV*)gv, 'I', Nullch, 0);
	    if (add & 2 && strEQ(nambeg,"AnyDBM_File::ISA") && AvFILL(av) == -1)
	    {
		char *pname;
		av_push(av, newSVpv(pname = "NDBM_File",0));
		gv_stashpv(pname, TRUE);
		av_push(av, newSVpv(pname = "DB_File",0));
		gv_stashpv(pname, TRUE);
		av_push(av, newSVpv(pname = "GDBM_File",0));
		gv_stashpv(pname, TRUE);
		av_push(av, newSVpv(pname = "SDBM_File",0));
		gv_stashpv(pname, TRUE);
		av_push(av, newSVpv(pname = "ODBM_File",0));
		gv_stashpv(pname, TRUE);
	    }
	}
	break;
#ifdef OVERLOAD
    case 'O':
        if (strEQ(name, "OVERLOAD")) {
            HV* hv = GvHVn(gv);
            SvMULTI_on(gv);
            sv_magic((SV*)hv, (SV*)gv, 'A', 0, 0);
        }
        break;
#endif /* OVERLOAD */
    case 'S':
	if (strEQ(name, "SIG")) {
	    HV *hv;
	    siggv = gv;
	    SvMULTI_on(siggv);
	    hv = GvHVn(siggv);
	    hv_magic(hv, siggv, 'S');

	    /* initialize signal stack */
	    signalstack = newAV();
	    AvREAL_off(signalstack);
	    av_extend(signalstack, 30);
	    av_fill(signalstack, 0);
	}
	break;

    case '&':
	if (len > 1)
	    break;
	ampergv = gv;
	sawampersand = TRUE;
	goto ro_magicalize;

    case '`':
	if (len > 1)
	    break;
	leftgv = gv;
	sawampersand = TRUE;
	goto ro_magicalize;

    case '\'':
	if (len > 1)
	    break;
	rightgv = gv;
	sawampersand = TRUE;
	goto ro_magicalize;

    case ':':
	if (len > 1)
	    break;
	sv_setpv(GvSV(gv),chopset);
	goto magicalize;

    case '[':
    case '#':
    case '*':
	if (dowarn && len == 1 && sv_type == SVt_PV)
	    warn("Use of $%s is deprecated", name);
	/* FALL THROUGH */
    case '!':
    case '?':
    case '^':
    case '~':
    case '=':
    case '-':
    case '%':
    case '.':
    case '(':
    case ')':
    case '<':
    case '>':
    case ',':
    case '\\':
    case '/':
    case '|':
    case '\004':
    case '\010':
    case '\t':
    case '\020':
    case '\024':
    case '\027':
    case '\006':
	if (len > 1)
	    break;
	goto magicalize;

    case '+':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      ro_magicalize:
	SvREADONLY_on(GvSV(gv));
      magicalize:
	sv_magic(GvSV(gv), (SV*)gv, 0, name, len);
	break;

    case '\014':
	if (len > 1)
	    break;
	sv_setpv(GvSV(gv),"\f");
	formfeed = GvSV(gv);
	break;
    case ';':
	if (len > 1)
	    break;
	sv_setpv(GvSV(gv),"\034");
	break;
    case ']':
	if (len == 1) {
	    SV *sv;
	    sv = GvSV(gv);
	    sv_upgrade(sv, SVt_PVNV);
	    sv_setpv(sv,rcsid);
	    SvNVX(sv) = atof(patchlevel);
	    SvNOK_on(sv);
	}
	break;
    }
    return gv;
}

void
gv_fullname(sv,gv)
SV *sv;
GV *gv;
{
    HV *hv = GvSTASH(gv);

    if (!hv)
	return;
    sv_setpv(sv, sv == (SV*)gv ? "*" : "");
    sv_catpv(sv,HvNAME(hv));
    sv_catpvn(sv,"::", 2);
    sv_catpvn(sv,GvNAME(gv),GvNAMELEN(gv));
}

void
gv_efullname(sv,gv)
SV *sv;
GV *gv;
{
    GV* egv = GvEGV(gv);
    HV *hv = GvSTASH(egv);

    if (!hv)
	return;
    sv_setpv(sv, sv == (SV*)gv ? "*" : "");
    sv_catpv(sv,HvNAME(hv));
    sv_catpvn(sv,"::", 2);
    sv_catpvn(sv,GvNAME(egv),GvNAMELEN(egv));
}

IO *
newIO()
{
    IO *io;
    GV *iogv;

    io = (IO*)NEWSV(0,0);
    sv_upgrade((SV *)io,SVt_PVIO);
    SvREFCNT(io) = 1;
    SvOBJECT_on(io);
    iogv = gv_fetchpv("FileHandle::", TRUE, SVt_PVIO);
    SvSTASH(io) = (HV*)SvREFCNT_inc(GvHV(iogv));
    return io;
}

void
gv_check(stash)
HV* stash;
{
    register HE *entry;
    register I32 i;
    register GV *gv;
    HV *hv;
    GV *filegv;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = entry->hent_next) {
	    if (isALPHA(*entry->hent_key)) {
		gv = (GV*)entry->hent_val;
		if (SvMULTI(gv))
		    continue;
		curcop->cop_line = GvLINE(gv);
		filegv = GvFILEGV(gv);
		curcop->cop_filegv = filegv;
		if (filegv && SvMULTI(filegv))	/* Filename began with slash */
		    continue;
		warn("Identifier \"%s::%s\" used only once: possible typo",
			HvNAME(stash), GvNAME(gv));
	    }
	    else if (*entry->hent_key == '_' &&
		(gv = (GV*)entry->hent_val) &&
		(hv = GvHV(gv)) && HvNAME(hv) && hv != defstash)
		     gv_check(hv);              /* nested package */
						      
	}
    }
}

GV *
newGVgen(pack)
char *pack;
{
    (void)sprintf(tokenbuf,"%s::_GEN_%ld",pack,(long)gensym++);
    return gv_fetchpv(tokenbuf,TRUE, SVt_PVGV);
}

/* hopefully this is only called on local symbol table entries */

GP*
gp_ref(gp)
GP* gp;
{
    gp->gp_refcnt++;
    return gp;

}

void
gp_free(gv)
GV* gv;
{
    IO *io;
    CV *cv;
    GP* gp;

    if (!gv || !(gp = GvGP(gv)))
	return;
    if (gp->gp_refcnt == 0) {
        warn("Attempt to free unreferenced glob pointers");
        return;
    }
    if (--gp->gp_refcnt > 0)
        return;

    SvREFCNT_dec(gp->gp_sv);
    SvREFCNT_dec(gp->gp_av);
    SvREFCNT_dec(gp->gp_hv);
    if ((io = gp->gp_io) && SvTYPE(io) != SVTYPEMASK) {
	do_close(gv,FALSE);
	SvREFCNT_dec(io);
    }
    if ((cv = gp->gp_cv) && !GvCVGEN(gv))
	SvREFCNT_dec(cv);
    Safefree(gp);
    GvGP(gv) = 0;
}

#if defined(CRIPPLED_CC) && (defined(iAPX286) || defined(M_I286) || defined(I80286))
#define MICROPORT
#endif

#ifdef	MICROPORT	/* Microport 2.4 hack */
AV *GvAVn(gv)
register GV *gv;
{
    if (GvGP(gv)->gp_av) 
	return GvGP(gv)->gp_av;
    else
	return GvGP(gv_AVadd(gv))->gp_av;
}

HV *GvHVn(gv)
register GV *gv;
{
    if (GvGP(gv)->gp_hv)
	return GvGP(gv)->gp_hv;
    else
	return GvGP(gv_HVadd(gv))->gp_hv;
}
#endif			/* Microport 2.4 hack */

#ifdef OVERLOAD
/* Updates and caches the CV's */

bool
Gv_AMupdate(stash)
HV* stash;
{
  GV** gvp;
  HV* hv;
  GV* gv;
  CV* cv;
  MAGIC* mg=mg_find((SV*)stash,'c');
  AMT *amtp;

  if (mg && (amtp=((AMT*)(mg->mg_ptr)))->was_ok_am == amagic_generation &&
             amtp->was_ok_sub == sub_generation)
      return HV_AMAGIC(stash)? TRUE: FALSE;
  gvp=(GV**)hv_fetch(stash,"OVERLOAD",8,FALSE);
  sv_unmagic((SV*)stash, 'c');

  DEBUG_o( deb("Recalcing arithmetic magic in package %s\n",HvNAME(stash)) );

  if (gvp && ((gv = *gvp) != (GV*)&sv_undef && (hv = GvHV(gv)))) {
    int filled=0;
    int i;
    char *cp;
    AMT amt;
    SV* sv;
    SV** svp;

/*  if (*(svp)==(SV*)amagic_generation && *(svp+1)==(SV*)sub_generation) {
      DEBUG_o( deb("Arithmetic magic in package %s up-to-date\n",HvNAME(stash))
);
      return HV_AMAGIC(stash)? TRUE: FALSE;
    }*/

    amt.was_ok_am=amagic_generation;
    amt.was_ok_sub=sub_generation;
    amt.fallback=AMGfallNO;

    /* Work with "fallback" key, which we assume to be first in AMG_names */

    if ((cp=((char**)(*AMG_names))[0]) &&
	(svp=(SV**)hv_fetch(hv,cp,strlen(cp),FALSE)) && (sv = *svp)) {
      if (SvTRUE(sv)) amt.fallback=AMGfallYES;
      else if (SvOK(sv)) amt.fallback=AMGfallNEVER;
    }

    for (i=1;i<NofAMmeth*2;i++) {
      cv=0;

      if ( (cp=((char**)(*AMG_names))[i]) ) {
        svp=(SV**)hv_fetch(hv,cp,strlen(cp),FALSE);
        if (svp && ((sv = *svp) != (GV*)&sv_undef)) {
          switch (SvTYPE(sv)) {
            default:
              if (!SvROK(sv)) {
                if (!SvOK(sv)) break;
                gv = gv_fetchpv(SvPV(sv, na), FALSE, SVt_PVCV);
                if (gv) cv = GvCV(gv);
                break;
              }
              cv = (CV*)SvRV(sv);
              if (SvTYPE(cv) == SVt_PVCV)
                  break;
                /* FALL THROUGH */
            case SVt_PVHV:
            case SVt_PVAV:
	      die("Not a subroutine reference in %%OVERLOAD");
	      return FALSE;
            case SVt_PVCV:
                cv = (CV*)sv;
                break;
            case SVt_PVGV:
                if (!(cv = GvCV((GV*)sv)))
                    cv = sv_2cv(sv, &stash, &gv, TRUE);
                break;
          }
          if (cv) filled=1;
	  else {
	    die("Method for operation %s not found in package %s during blessing\n",
		cp,HvNAME(stash));
	    return FALSE;
	  }
        }
      }
      amt.table[i]=cv;
    }
    sv_magic((SV*)stash, 0, 'c', (char*)&amt, sizeof(amt));
    if (filled) {
/*    HV_badAMAGIC_off(stash);*/
      HV_AMAGIC_on(stash);
      return TRUE;
    }
  }
/*HV_badAMAGIC_off(stash);*/
  HV_AMAGIC_off(stash);
  return FALSE;
}

/* During call to this subroutine stack can be reallocated. It is
 * advised to call SPAGAIN macro in your code after call */

SV*
amagic_call(left,right,method,flags)
SV* left;
SV* right;
int method;
int flags; 
{
  MAGIC *mg; 
  CV *cv; 
  CV **cvp=NULL, **ocvp=NULL;
  AMT *amtp, *oamtp;
  int fl=0, off, off1, lr=0, assign=AMGf_assign & flags, notfound=0, subst=0;
  int postpr=0;
  HV* stash;
  if (!(AMGf_noleft & flags) && SvAMAGIC(left)
      && (mg = mg_find((SV*)(stash=SvSTASH(SvRV(left))),'c'))
      && (ocvp = cvp = ((oamtp=amtp=(AMT*)mg->mg_ptr)->table))
      && (assign ?
             ((cv = cvp[off=method+1]) 
	      || ( amtp->fallback > AMGfallNEVER && /* fallback to
						     * usual method */
		  (fl = 1, cv = cvp[off=method]))):
             (1 && (cv = cvp[off=method]))  )) {
    lr=-1;			/* Call method for left argument */
  } else {
    if (cvp && amtp->fallback > AMGfallNEVER && flags & AMGf_unary) {
      int logic;

      /* look for substituted methods */
	 switch (method) {
	 case inc_amg:
	   if ((cv = cvp[off=add_ass_amg]) 
	       || ((cv = cvp[off=add_amg]) && (postpr=1))) {
	     right=&sv_yes; lr=-1; assign=1;
	   }
	   break;
	 case dec_amg:
	   if ((cv = cvp[off=subtr_ass_amg]) 
	       || ((cv = cvp[off=subtr_amg]) && (postpr=1))) {
	     right=&sv_yes; lr=-1; assign=1;
	   }
	   break;
	 case bool__amg:
	   (cv = cvp[off=numer_amg]) || (cv = cvp[off=string_amg]);
	   break;
	 case numer_amg:
	   (cv = cvp[off=string_amg]) || (cv = cvp[off=bool__amg]);
	   break;
	 case string_amg:
	   (cv = cvp[off=numer_amg]) || (cv = cvp[off=bool__amg]);
	   break;
	 case abs_amg:
	   if ((cvp[off1=lt_amg] || cvp[off1=lt_amg]) 
	       && ((cv = cvp[off=neg_amg]) || (cv = cvp[off=subtr_amg]))) {
	     if (off1==lt_amg) {
	       SV* lessp = amagic_call(left,
				       sv_2mortal(newSViv(0)),
				       lt_amg,AMGf_noright);
	       logic = SvTRUE(lessp);
	     } else {
	       SV* lessp = amagic_call(left,
				       sv_2mortal(newSViv(0)),
				       ncmp_amg,AMGf_noright);
	       logic = (SvNV(lessp) < 0);
	     }
	     if (logic) {
	       if (off==subtr_amg) {
		 right = left;
		 left = sv_2mortal(newSViv(0));
		 lr = 1;
	       }
	     } else {
	       return left;
	     }
	   }
	   break;
	 case neg_amg:
	   if (cv = cvp[off=subtr_amg]) {
	     right = left;
	     left = sv_2mortal(newSViv(0));
	     lr = 1;
	   }
	   break;
	 default:
	   goto not_found;
	 }
	 if (!cv) goto not_found;
    } else if (!(AMGf_noright & flags) && SvAMAGIC(right)
	       && (mg = mg_find((SV*)(stash=SvSTASH(SvRV(right))),'c'))
	       && (cvp = ((amtp=(AMT*)mg->mg_ptr)->table))
	       && (cv = cvp[off=method])) { /* Method for right
					     * argument found */
      lr=1;
    } else if (((ocvp && oamtp->fallback > AMGfallNEVER && (cvp=ocvp)) 
		|| (cvp && amtp->fallback > AMGfallNEVER && (lr=1)))
	       && !(flags & AMGf_unary)) {
				/* We look for substitution for
				 * comparison operations and
				 * concatendation */
      if (method==concat_amg || method==concat_ass_amg
	  || method==repeat_amg || method==repeat_ass_amg) {
	return NULL;		/* Delegate operation to string conversion */
      }
      off=-1;
      switch (method) {
	 case lt_amg:
	 case le_amg:
	 case gt_amg:
	 case ge_amg:
	 case eq_amg:
	 case ne_amg:
	   postpr = 1; off=ncmp_amg; break;
	 case slt_amg:
	 case sle_amg:
	 case sgt_amg:
	 case sge_amg:
	 case seq_amg:
	 case sne_amg:
	   postpr = 1; off=scmp_amg; break;
	 }
      if (off!=-1) cv = cvp[off];
      if (!cv) {
	goto not_found;
      }
    } else {
    not_found:			/* No method found, either report or die */
      if (ocvp && (cv=ocvp[nomethod_amg])) { /* Call report method */
	notfound = 1; lr = -1;
      } else if (cvp && (cv=cvp[nomethod_amg])) {
	notfound = 1; lr = 1;
      } else {
	char tmpstr[512];
	sprintf(tmpstr,"Operation `%s': no method found,\n\tleft argument %s%200s,\n\tright argument %s%200s",
		      ((char**)AMG_names)[off],
		      SvAMAGIC(left)? 
		        "in arithm-magical package ":
		        "has no arithmetic magic",
		      SvAMAGIC(left)? 
		        HvNAME(SvSTASH(SvRV(left))):
		        "",
		      SvAMAGIC(right)? 
		        "in arithm-magical package ":
		        "has no arithmetic magic",
		      SvAMAGIC(right)? 
		        HvNAME(SvSTASH(SvRV(right))):
		        "");
	if (amtp->fallback >= AMGfallYES) {
	  DEBUG_o( deb(tmpstr) );
	} else {
	  die(tmpstr);
	}
	return NULL;
      }
    }
  }
  if (!notfound) {
    DEBUG_o( deb("Operation `%s': method for %s argument found in package %s%s\n",
		 ((char**)AMG_names)[off],
		 (lr? "right": "left"),
		 HvNAME(stash), 
		 fl? ",\n\tassignment variant used": "") );
    /* Since we use shallow copy, we need to dublicate the contents,
       probably we need also to use user-supplied version of coping?
       */
    if (assign || method==inc_amg || method==dec_amg) RvDEEPCP(left);
  }
  {
    dSP;
    BINOP myop;
    SV* res;

    Zero(&myop, 1, BINOP);
    myop.op_last = (OP *) &myop;
    myop.op_next = Nullop;
    myop.op_flags = OPf_KNOW|OPf_STACKED;

    ENTER;
    SAVESPTR(op);
    op = (OP *) &myop;
    PUTBACK;
    pp_pushmark();

    EXTEND(sp, notfound + 5);
    PUSHs(lr>0? right: left);
    PUSHs(lr>0? left: right);
    PUSHs( assign ? &sv_undef : (lr>0? &sv_yes: &sv_no));
    if (notfound) {
      PUSHs( sv_2mortal(newSVpv(((char**)AMG_names)[off],0)) );
    }
    PUSHs((SV*)cv);
    PUTBACK;

    if (op = pp_entersub())
      run();
    LEAVE;
    SPAGAIN;

    res=POPs;
    PUTBACK;

    if (notfound) {
      /* sv_2mortal(res); */
      return NULL;
    }

    if (postpr) {
      int ans;
      switch (method) {
      case le_amg:
      case sle_amg:
	ans=SvIV(res)<=0; break;
      case lt_amg:
      case slt_amg:
	ans=SvIV(res)<0; break;
      case ge_amg:
      case sge_amg:
	ans=SvIV(res)>=0; break;
      case gt_amg:
      case sgt_amg:
	ans=SvIV(res)>0; break;
      case eq_amg:
      case seq_amg:
	ans=SvIV(res)==0; break;
      case ne_amg:
      case sne_amg:
	ans=SvIV(res)!=0; break;
      case inc_amg:
      case dec_amg:
	SvSetSV(left,res); return res; break;
      }
      return ans? &sv_yes: &sv_no;
    } else {
      return res;
    }
  }
}
#endif /* OVERLOAD */
