/*
 *    Copyright (c) 1991, 1992, 1993, 1994 Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	perl.c,v $
 */

/*
 * "A ship then new they built for him/of mithril and of elven glass" --Bilbo
 */

#include "EXTERN.h"
#include "perl.h"
#include "perly.h"
#include "patchlevel.h"

/* Omit -- it causes too much grief on mixed systems.
#ifdef I_UNISTD
#include <unistd.h>
#endif
*/

char rcsid[] = "$RCSfile: perl.c,v $$Revision: 5.0 $$Date: 92/08/07 18:25:50 $\nPatch level: ###\n";

#ifdef IAMSUID
#ifndef DOSUID
#define DOSUID
#endif
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef DOSUID
#undef DOSUID
#endif
#endif

static void find_beginning _((void));
static void incpush _((char *));
static void init_debugger _((void));
static void init_lexer _((void));
static void init_main_stash _((void));
static void init_perllib _((void));
static void init_postdump_symbols _((int, char **, char **));
static void init_predump_symbols _((void));
static void init_stacks _((void));
static void open_script _((char *, bool, SV *));
static void validate_suid _((char *));

PerlInterpreter *
perl_alloc()
{
    PerlInterpreter *sv_interp;

    curinterp = 0;
    New(53, sv_interp, 1, PerlInterpreter);
    return sv_interp;
}

void
perl_construct( sv_interp )
register PerlInterpreter *sv_interp;
{
    char* s;

    if (!(curinterp = sv_interp))
	return;

#ifdef MULTIPLICITY
    Zero(sv_interp, 1, PerlInterpreter);
#endif

    /* Init the real globals? */
    if (!linestr) {
	linestr = NEWSV(65,80);
	sv_upgrade(linestr,SVt_PVIV);

	SvREADONLY_on(&sv_undef);

	sv_setpv(&sv_no,No);
	SvNV(&sv_no);
	SvREADONLY_on(&sv_no);

	sv_setpv(&sv_yes,Yes);
	SvNV(&sv_yes);
	SvREADONLY_on(&sv_yes);

#ifdef MSDOS
	/*
	 * There is no way we can refer to them from Perl so close them to save
	 * space.  The other alternative would be to provide STDAUX and STDPRN
	 * filehandles.
	 */
	(void)fclose(stdaux);
	(void)fclose(stdprn);
#endif
    }

#ifdef MULTIPLICITY
    chopset	= " \n-";
    copline	= NOLINE;
    curcop	= &compiling;
    dlmax	= 128;
    laststatval	= -1;
    laststype	= OP_STAT;
    maxscream	= -1;
    maxsysfd	= MAXSYSFD;
    nrs		= "\n";
    nrschar	= '\n';
    nrslen	= 1;
    rs		= "\n";
    rschar	= '\n';
    rsfp	= Nullfp;
    rslen	= 1;
    statname	= Nullsv;
    tmps_floor	= -1;
#endif

    uid = (int)getuid();
    euid = (int)geteuid();
    gid = (int)getgid();
    egid = (int)getegid();
    tainting = (euid != uid || egid != gid);
    if (s = strchr(rcsid,'#')) {
	(void)sprintf(s, "%d\n", PATCHLEVEL);
	sprintf(patchlevel,"%3.3s%2.2d", strchr(rcsid,'5'), PATCHLEVEL);
    }

    fdpid = newAV();	/* for remembering popen pids by fd */
    pidstatus = newHV();/* for remembering status of dead pids */

    init_stacks();
    ENTER;
}

void
perl_destruct(sv_interp)
register PerlInterpreter *sv_interp;
{
    I32 last_sv_count;
    HV *hv;

    if (!(curinterp = sv_interp))
	return;
    LEAVE;
    FREE_TMPS();

    if (sv_objcount) {
	/* We must account for everything.  First the syntax tree. */
	if (main_root) {
	    curpad = AvARRAY(comppad);
	    op_free(main_root);
	    main_root = 0;
	}
    }
    if (sv_objcount) {
	/*
	 * Try to destruct global references.  We do this first so that the
	 * destructors and destructees still exist.  Some sv's might remain.
	 * Non-referenced objects are on their own.
	 */
    
	dirty = TRUE;
	sv_clean_objs();
    }

#ifndef EMBED
    
    /* The exit() function will do everything that needs doing. */
    return;
    
#else
    
    /* Prepare to destruct main symbol table.  */
    hv = defstash;
    defstash = 0;
    SvREFCNT_dec(hv);

    FREE_TMPS();
#ifdef DEBUGGING
    if (scopestack_ix != 0)
	warn("Unbalanced scopes: %d more ENTERs than LEAVEs\n", scopestack_ix);
    if (savestack_ix != 0)
	warn("Unbalanced saves: %d more saves than restores\n", savestack_ix);
    if (tmps_floor != -1)
	warn("Unbalanced tmps: %d more allocs than frees\n", tmps_floor + 1);
    if (cxstack_ix != -1)
	warn("Unbalanced context: %d more PUSHes than POPs\n", cxstack_ix + 1);
#endif

    /* Now absolutely destruct everything, somehow or other, loops or no. */
    last_sv_count = 0;
    while (sv_count != 0 && sv_count != last_sv_count) {
	last_sv_count = sv_count;
	sv_clean_all();
    }
    if (sv_count != 0)
	warn("Scalars leaked: %d\n", sv_count);
    
#endif /* EMBED */
}

void
perl_free(sv_interp)
PerlInterpreter *sv_interp;
{
    if (!(curinterp = sv_interp))
	return;
    Safefree(sv_interp);
}
#ifndef STANDARD_C
char *getenv _((char *)); /* Usually in <stdlib.h> */
#endif

int
perl_parse(sv_interp, argc, argv, env)
PerlInterpreter *sv_interp;
int argc;
char **argv;
char **env;
{
    register SV *sv;
    register char *s;
    char *scriptname;
    VOL bool dosearch = FALSE;
    char *validarg = "";

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    croak("suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif
#endif

    if (!(curinterp = sv_interp))
	return 255;

    if (main_root)
	op_free(main_root);
    main_root = 0;

    origargv = argv;
    origargc = argc;
#ifndef VMS  /* VMS doesn't have environ array */
    origenviron = environ;
#endif

    switch (setjmp(top_env)) {
    case 1:
	statusvalue = 255;
    case 2:
	curstash = defstash;
	if (endav)
	    calllist(endav);
	return(statusvalue);	/* my_exit() was called */
    case 3:
	fprintf(stderr, "panic: top_env\n");
	return 1;
    }

    if (do_undump) {

	/* Come here if running an undumped a.out. */

	origfilename = savestr(argv[0]);
	do_undump = FALSE;
	cxstack_ix = -1;		/* start label stack again */
	init_postdump_symbols(argc,argv,env);
	return 0;
    }

    sv_setpvn(linestr,"",0);
    sv = newSVpv("",0);		/* first used for -I flags */
    SAVEFREESV(sv);
    init_main_stash();
    for (argc--,argv++; argc > 0; argc--,argv++) {
	if (argv[0][0] != '-' || !argv[0][1])
	    break;
#ifdef DOSUID
    if (*validarg)
	validarg = " PHOOEY ";
    else
	validarg = argv[0];
#endif
	s = argv[0]+1;
      reswitch:
	switch (*s) {
	case '0':
	case 'F':
	case 'a':
	case 'c':
	case 'd':
	case 'D':
	case 'i':
	case 'l':
	case 'n':
	case 'p':
	case 's':
	case 'T':
	case 'u':
	case 'U':
	case 'v':
	case 'w':
	    if (s = moreswitches(s))
		goto reswitch;
	    break;

	case 'e':
	    if (euid != uid || egid != gid)
		croak("No -e allowed in setuid scripts");
	    if (!e_fp) {
	        e_tmpname = savestr(TMPPATH);
		(void)mktemp(e_tmpname);
		if (!*e_tmpname)
		    croak("Can't mktemp()");
		e_fp = fopen(e_tmpname,"w");
		if (!e_fp)
		    croak("Cannot open temporary file");
	    }
	    if (argv[1]) {
		fputs(argv[1],e_fp);
		argc--,argv++;
	    }
	    (void)putc('\n', e_fp);
	    break;
	case 'I':
	    taint_not("-I");
	    sv_catpv(sv,"-");
	    sv_catpv(sv,s);
	    sv_catpv(sv," ");
	    if (*++s) {
		av_push(GvAVn(incgv),newSVpv(s,0));
	    }
	    else if (argv[1]) {
		av_push(GvAVn(incgv),newSVpv(argv[1],0));
		sv_catpv(sv,argv[1]);
		argc--,argv++;
		sv_catpv(sv," ");
	    }
	    break;
	case 'P':
	    taint_not("-P");
	    preprocess = TRUE;
	    s++;
	    goto reswitch;
	case 'S':
	    taint_not("-S");
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'x':
	    doextract = TRUE;
	    s++;
	    if (*s)
		cddir = savestr(s);
	    break;
	case '-':
	    argc--,argv++;
	    goto switch_end;
	case 0:
	    break;
	default:
	    croak("Unrecognized switch: -%s",s);
	}
    }
  switch_end:
    scriptname = argv[0];
    if (e_fp) {
	if (fflush(e_fp) || ferror(e_fp) || fclose(e_fp))
	    croak("Can't write to temp file for -e: %s", Strerror(errno));
	argc++,argv--;
	scriptname = e_tmpname;
    }
    else if (scriptname == Nullch) {
#ifdef MSDOS
	if ( isatty(fileno(stdin)) )
	    moreswitches("v");
#endif
	scriptname = "-";
    }

    init_perllib();

    open_script(scriptname,dosearch,sv);

    validate_suid(validarg);

    if (doextract)
	find_beginning();

    if (perldb)
	init_debugger();

    pad = newAV();
    comppad = pad;
    av_push(comppad, Nullsv);
    curpad = AvARRAY(comppad);
    padname = newAV();
    comppad_name = padname;
    comppad_name_fill = 0;
    min_intro_pending = 0;
    padix = 0;

    perl_init_ext();	/* in case linked C routines want magical variables */

    init_predump_symbols();
    if (!do_undump)
	init_postdump_symbols(argc,argv,env);

    init_lexer();

    /* now parse the script */

    error_count = 0;
    if (yyparse() || error_count) {
	if (minus_c)
	    croak("%s had compilation errors.\n", origfilename);
	else {
	    croak("Execution of %s aborted due to compilation errors.\n",
		origfilename);
	}
    }
    curcop->cop_line = 0;
    curstash = defstash;
    preprocess = FALSE;
    if (e_fp) {
	e_fp = Nullfp;
	(void)UNLINK(e_tmpname);
    }

    /* now that script is parsed, we can modify record separator */

    rs = nrs;
    rslen = nrslen;
    rschar = nrschar;
    rspara = (nrslen == 2);
    sv_setpvn(GvSV(gv_fetchpv("/", TRUE, SVt_PV)), rs, rslen);

    if (do_undump)
	my_unexec();

    if (dowarn)
	gv_check(defstash);

    LEAVE;
    FREE_TMPS();
    ENTER;
    restartop = 0;
    return 0;
}

int
perl_run(sv_interp)
PerlInterpreter *sv_interp;
{
    if (!(curinterp = sv_interp))
	return 255;
    switch (setjmp(top_env)) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	break;
    case 2:
	curstash = defstash;
	if (endav)
	    calllist(endav);
	FREE_TMPS();
	return(statusvalue);		/* my_exit() was called */
    case 3:
	if (!restartop) {
	    fprintf(stderr, "panic: restartop\n");
	    FREE_TMPS();
	    return 1;
	}
	if (stack != mainstack) {
	    dSP;
	    SWITCHSTACK(stack, mainstack);
	}
	break;
    }

    if (!restartop) {
	DEBUG_x(dump_all());
	DEBUG(fprintf(stderr,"\nEXECUTING...\n\n"));

	if (minus_c) {
	    fprintf(stderr,"%s syntax OK\n", origfilename);
	    my_exit(0);
	}
    }

    /* do it */

    if (restartop) {
	op = restartop;
	restartop = 0;
	run();
    }
    else if (main_start) {
	op = main_start;
	run();
    }

    my_exit(0);
    return 0;
}

void
my_exit(status)
I32 status;
{
    register CONTEXT *cx;
    I32 gimme;
    SV **newsp;

    statusvalue = (unsigned short)(status & 0xffff);
    if (cxstack_ix >= 0) {
	if (cxstack_ix > 0)
	    dounwind(0);
	POPBLOCK(cx,curpm);
	LEAVE;
    }
    longjmp(top_env, 2);
}

SV*
perl_get_sv(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PV);
    if (gv)
	return GvSV(gv);
    return Nullsv;
}

AV*
perl_get_av(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PVAV);
    if (create)
    	return GvAVn(gv);
    if (gv)
	return GvAV(gv);
    return Nullav;
}

HV*
perl_get_hv(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PVHV);
    if (create)
    	return GvHVn(gv);
    if (gv)
	return GvHV(gv);
    return Nullhv;
}

CV*
perl_get_cv(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PVCV);
    if (create && !GvCV(gv))
    	return newSUB(start_subparse(),
		      newSVOP(OP_CONST, 0, newSVpv(name,0)),
		      Nullop);
    if (gv)
	return GvCV(gv);
    return Nullcv;
}

/* Be sure to refetch the stack pointer after calling these routines. */

I32
perl_call_argv(subname, flags, argv)
char *subname;
I32 flags;		/* See G_* flags in cop.h */
register char **argv;	/* null terminated arg list */
{
    dSP;

    PUSHMARK(sp);
    if (argv) {
	while (*argv) {
	    XPUSHs(sv_2mortal(newSVpv(*argv,0)));
	    argv++;
	}
	PUTBACK;
    }
    return perl_call_pv(subname, flags);
}

I32
perl_call_pv(subname, flags)
char *subname;		/* name of the subroutine */
I32 flags;		/* See G_* flags in cop.h */
{
    return perl_call_sv((SV*)perl_get_cv(subname, TRUE), flags);
}

I32
perl_call_method(methname, flags)
char *methname;		/* name of the subroutine */
I32 flags;		/* See G_* flags in cop.h */
{
    dSP;
    OP myop;
    if (!op)
	op = &myop;
    XPUSHs(sv_2mortal(newSVpv(methname,0)));
    PUTBACK;
    pp_method();
    return perl_call_sv(*stack_sp--, flags);
}

/* May be called with any of a CV, a GV, or an SV containing the name. */
I32
perl_call_sv(sv, flags)
SV* sv;
I32 flags;		/* See G_* flags in cop.h */
{
    LOGOP myop;		/* fake syntax tree node */
    SV** sp = stack_sp;
    I32 oldmark = TOPMARK;
    I32 retval;
    jmp_buf oldtop;
    I32 oldscope;
    
    ENTER;
    SAVETMPS;

    SAVESPTR(op);
    op = (OP*)&myop;
    Zero(op, 1, LOGOP);
    EXTEND(stack_sp, 1);
    *++stack_sp = sv;
    oldscope = scopestack_ix;

    if (!(flags & G_NOARGS))
	myop.op_flags = OPf_STACKED;
    myop.op_next = Nullop;
    myop.op_flags |= OPf_KNOW;
    if (flags & G_ARRAY)
      myop.op_flags |= OPf_LIST;

    if (flags & G_EVAL) {
	Copy(top_env, oldtop, 1, jmp_buf);

	cLOGOP->op_other = op;
	pp_entertry();

    restart:
	switch (setjmp(top_env)) {
	case 0:
	    break;
	case 1:
	    statusvalue = 255;	/* XXX I don't think we use 1 anymore. */
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    curstash = defstash;
	    FREE_TMPS();
	    Copy(oldtop, top_env, 1, jmp_buf);
	    if (statusvalue)
		croak("Callback called exit");
	    my_exit(statusvalue);
	    /* NOTREACHED */
	case 3:
	    if (restartop) {
		op = restartop;
		goto restart;
	    }
	    stack_sp = stack_base + oldmark;
	    if (flags & G_ARRAY)
		retval = 0;
	    else {
		retval = 1;
		*++stack_sp = &sv_undef;
	    }
	    Copy(oldtop, top_env, 1, jmp_buf);
	    goto cleanup;
	}
    }

    if (op == (OP*)&myop)
	op = pp_entersub();
    if (op)
	run();
    retval = stack_sp - (stack_base + oldmark);

  cleanup:
    if (flags & G_EVAL) {
	if (scopestack_ix > oldscope) {
	    op = (OP*)&myop;
	    pp_leavetry();
	}
	Copy(oldtop, top_env, 1, jmp_buf);
    }
    if (flags & G_DISCARD) {
	FREE_TMPS();
	stack_sp = stack_base + oldmark;
	retval = 0;
    }
    LEAVE;
    return retval;
}

/* Older forms, here grandfathered. */

#ifdef DEPRECATED
I32
perl_callargv(subname, spix, gimme, argv)
char *subname;
register I32 spix;	/* current stack pointer index */
I32 gimme;		/* See G_* flags in cop.h */
register char **argv;	/* null terminated arg list, NULL for no arglist */
{
    stack_sp = stack_base + spix;
    PUSHMARK(stack_sp - numargs);
    return spix + perl_call_argv(subname, gimme, argv);
}

I32
perl_callpv(subname, spix, gimme, hasargs, numargs)
char *subname;
I32 spix;		/* stack pointer index after args are pushed */
I32 gimme;		/* See G_* flags in cop.h */
I32 hasargs;		/* whether to create a @_ array for routine */
I32 numargs;		/* how many args are pushed on the stack */
{
    stack_sp = stack_base + spix;
    PUSHMARK(stack_sp - numargs);
    return spix - numargs + perl_call_sv((SV*)perl_get_cv(subname, TRUE),
				gimme, hasargs, numargs);
}

I32
perl_callsv(sv, spix, gimme, hasargs, numargs)
SV* sv;
I32 spix;		/* stack pointer index after args are pushed */
I32 gimme;		/* See G_* flags in cop.h */
I32 hasargs;		/* whether to create a @_ array for routine */
I32 numargs;		/* how many args are pushed on the stack */
{
    stack_sp = stack_base + spix;
    PUSHMARK(stack_sp - numargs);
    return spix - numargs + perl_call_sv(sv, gimme, hasargs, numargs);
}
#endif

/* Require a module. */

void
perl_requirepv(pv)
char* pv;
{
    UNOP myop;		/* fake syntax tree node */
    SV* sv;
    dSP;
    
    ENTER;
    SAVETMPS;
    SAVESPTR(op);
    sv = sv_newmortal();
    sv_setpv(sv, pv);
    op = (OP*)&myop;
    Zero(op, 1, UNOP);
    XPUSHs(sv);

    myop.op_type = OP_REQUIRE;
    myop.op_next = Nullop;
    myop.op_private = 1;
    myop.op_flags = OPf_KNOW;

    PUTBACK;
    if (op = pp_require())
	run();
    stack_sp--;
    FREE_TMPS();
    LEAVE;
}

void
magicname(sym,name,namlen)
char *sym;
char *name;
I32 namlen;
{
    register GV *gv;

    if (gv = gv_fetchpv(sym,TRUE, SVt_PV))
	sv_magic(GvSV(gv), (SV*)gv, 0, name, namlen);
}

#ifdef DOSISH
#define PERLLIB_SEP ';'
#else
#define PERLLIB_SEP ':'
#endif

static void
incpush(p)
char *p;
{
    char *s;

    if (!p)
	return;

    /* Break at all separators */
    while (*p) {
	/* First, skip any consecutive separators */
	while ( *p == PERLLIB_SEP ) {
	    /* Uncomment the next line for PATH semantics */
	    /* av_push(GvAVn(incgv), newSVpv(".", 1)); */
	    p++;
	}
	if ( (s = strchr(p, PERLLIB_SEP)) != Nullch ) {
	    av_push(GvAVn(incgv), newSVpv(p, (STRLEN)(s - p)));
	    p = s + 1;
	} else {
	    av_push(GvAVn(incgv), newSVpv(p, 0));
	    break;
	}
    }
}

/* This routine handles any switches that can be given during run */

char *
moreswitches(s)
char *s;
{
    I32 numlen;

    switch (*s) {
    case '0':
	nrschar = scan_oct(s, 4, &numlen);
	nrs = nsavestr("\n",1);
	*nrs = nrschar;
	if (nrschar > 0377) {
	    nrslen = 0;
	    nrs = "";
	}
	else if (!nrschar && numlen >= 2) {
	    nrslen = 2;
	    nrs = "\n\n";
	    nrschar = '\n';
	}
	return s + numlen;
    case 'F':
	minus_F = TRUE;
	splitstr = savestr(s + 1);
	s += strlen(s);
	return s;
    case 'a':
	minus_a = TRUE;
	s++;
	return s;
    case 'c':
	minus_c = TRUE;
	s++;
	return s;
    case 'd':
	taint_not("-d");
	perldb = TRUE;
	s++;
	return s;
    case 'D':
#ifdef DEBUGGING
	taint_not("-D");
	if (isALPHA(s[1])) {
	    static char debopts[] = "psltocPmfrxuLHXD";
	    char *d;

	    for (s++; *s && (d = strchr(debopts,*s)); s++)
		debug |= 1 << (d - debopts);
	}
	else {
	    debug = atoi(s+1);
	    for (s++; isDIGIT(*s); s++) ;
	}
	debug |= 0x80000000;
#else
	warn("Recompile perl with -DDEBUGGING to use -D switch\n");
	for (s++; isDIGIT(*s); s++) ;
#endif
	/*SUPPRESS 530*/
	return s;
    case 'i':
	if (inplace)
	    Safefree(inplace);
	inplace = savestr(s+1);
	/*SUPPRESS 530*/
	for (s = inplace; *s && !isSPACE(*s); s++) ;
	*s = '\0';
	break;
    case 'I':
	taint_not("-I");
	if (*++s) {
	    av_push(GvAVn(incgv),newSVpv(s,0));
	}
	else
	    croak("No space allowed after -I");
	break;
    case 'l':
	minus_l = TRUE;
	s++;
	if (isDIGIT(*s)) {
	    ors = savestr("\n");
	    orslen = 1;
	    *ors = scan_oct(s, 3 + (*s == '0'), &numlen);
	    s += numlen;
	}
	else {
	    ors = nsavestr(nrs,nrslen);
	    orslen = nrslen;
	}
	return s;
    case 'n':
	minus_n = TRUE;
	s++;
	return s;
    case 'p':
	minus_p = TRUE;
	s++;
	return s;
    case 's':
	taint_not("-s");
	doswitches = TRUE;
	s++;
	return s;
    case 'T':
	tainting = TRUE;
	s++;
	return s;
    case 'u':
	do_undump = TRUE;
	s++;
	return s;
    case 'U':
	unsafe = TRUE;
	s++;
	return s;
    case 'v':
	fputs("\nThis is perl, version 5.0, Alpha 12h (unsupported)\n\n",stdout);
	fputs(rcsid,stdout);
	fputs("\nCopyright (c) 1989, 1990, 1991, 1992, 1993, 1994 Larry Wall\n",stdout);
#ifdef MSDOS
	fputs("MS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n",
	stdout);
#ifdef OS2
        fputs("OS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n",
        stdout);
#endif
#endif
#ifdef atarist
        fputs("atariST series port, ++jrb  bammi@cadence.com\n", stdout);
#endif
	fputs("\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5.0 source kit.\n",stdout);
#ifdef MSDOS
        usage(origargv[0]);
#endif
	exit(0);
    case 'w':
	dowarn = TRUE;
	s++;
	return s;
    case ' ':
	if (s[1] == '-')	/* Additional switches on #! line. */
	    return s+2;
	break;
    case 0:
    case '\n':
    case '\t':
	break;
    default:
	croak("Switch meaningless after -x: -%s",s);
    }
    return Nullch;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */

void
my_unexec()
{
#ifdef UNEXEC
    int    status;
    extern int etext;

    sprintf (buf, "%s.perldump", origfilename);
    sprintf (tokenbuf, "%s/perl", BIN);

    status = unexec(buf, tokenbuf, &etext, sbrk(0), 0);
    if (status)
	fprintf(stderr, "unexec of %s into %s failed!\n", tokenbuf, buf);
    my_exit(status);
#else
    ABORT();		/* for use with undump */
#endif
}

static void
init_main_stash()
{
    GV *gv;
    curstash = defstash = newHV();
    curstname = newSVpv("main",4);
    GvHV(gv = gv_fetchpv("_main",TRUE, SVt_PVHV)) = (HV*)SvREFCNT_inc(defstash);
    SvREADONLY_on(gv);
    HvNAME(defstash) = savestr("main");
    incgv = gv_HVadd(gv_AVadd(gv_fetchpv("INC",TRUE, SVt_PVAV)));
    SvMULTI_on(incgv);
    defgv = gv_fetchpv("_",TRUE, SVt_PV);
    curstash = defstash;
    compiling.cop_stash = defstash;
}

#ifdef CAN_PROTOTYPE
static void
open_script(char *scriptname, bool dosearch, SV *sv)
#else
static void
open_script(scriptname,dosearch,sv)
char *scriptname;
bool dosearch;
SV *sv;
#endif
{
    char *xfound = Nullch;
    char *xfailed = Nullch;
    register char *s;
    I32 len;

    if (dosearch && !strchr(scriptname, '/') && (s = getenv("PATH"))) {

	bufend = s + strlen(s);
	while (*s) {
#ifndef DOSISH
	    s = cpytill(tokenbuf,s,bufend,':',&len);
#else
#ifdef atarist
	    for (len = 0; *s && *s != ',' && *s != ';'; tokenbuf[len++] = *s++);
	    tokenbuf[len] = '\0';
#else
	    for (len = 0; *s && *s != ';'; tokenbuf[len++] = *s++);
	    tokenbuf[len] = '\0';
#endif
#endif
	    if (*s)
		s++;
#ifndef DOSISH
	    if (len && tokenbuf[len-1] != '/')
#else
#ifdef atarist
	    if (len && ((tokenbuf[len-1] != '\\') && (tokenbuf[len-1] != '/')))
#else
	    if (len && tokenbuf[len-1] != '\\')
#endif
#endif
		(void)strcat(tokenbuf+len,"/");
	    (void)strcat(tokenbuf+len,scriptname);
	    DEBUG_p(fprintf(stderr,"Looking for %s\n",tokenbuf));
	    if (stat(tokenbuf,&statbuf) < 0)		/* not there? */
		continue;
	    if (S_ISREG(statbuf.st_mode)
	     && cando(S_IRUSR,TRUE,&statbuf) && cando(S_IXUSR,TRUE,&statbuf)) {
		xfound = tokenbuf;              /* bingo! */
		break;
	    }
	    if (!xfailed)
		xfailed = savestr(tokenbuf);
	}
	if (!xfound)
	    croak("Can't execute %s", xfailed ? xfailed : scriptname );
	if (xfailed)
	    Safefree(xfailed);
	scriptname = xfound;
    }

    origfilename = savestr(e_fp ? "-e" : scriptname);
    curcop->cop_filegv = gv_fetchfile(origfilename);
    if (strEQ(origfilename,"-"))
	scriptname = "";
    if (preprocess) {
	char *cpp = CPPSTDIN;

	if (strEQ(cpp,"cppstdin"))
	    sprintf(tokenbuf, "%s/%s", SCRIPTDIR, cpp);
	else
	    sprintf(tokenbuf, "%s", cpp);
	sv_catpv(sv,"-I");
	sv_catpv(sv,PRIVLIB);
#ifdef MSDOS
	(void)sprintf(buf, "\
sed %s -e \"/^[^#]/b\" \
 -e \"/^#[ 	]*include[ 	]/b\" \
 -e \"/^#[ 	]*define[ 	]/b\" \
 -e \"/^#[ 	]*if[ 	]/b\" \
 -e \"/^#[ 	]*ifdef[ 	]/b\" \
 -e \"/^#[ 	]*ifndef[ 	]/b\" \
 -e \"/^#[ 	]*else/b\" \
 -e \"/^#[ 	]*elif[ 	]/b\" \
 -e \"/^#[ 	]*undef[ 	]/b\" \
 -e \"/^#[ 	]*endif/b\" \
 -e \"s/^#.*//\" \
 %s | %s -C %s %s",
	  (doextract ? "-e \"1,/^#/d\n\"" : ""),
#else
	(void)sprintf(buf, "\
%s %s -e '/^[^#]/b' \
 -e '/^#[ 	]*include[ 	]/b' \
 -e '/^#[ 	]*define[ 	]/b' \
 -e '/^#[ 	]*if[ 	]/b' \
 -e '/^#[ 	]*ifdef[ 	]/b' \
 -e '/^#[ 	]*ifndef[ 	]/b' \
 -e '/^#[ 	]*else/b' \
 -e '/^#[ 	]*elif[ 	]/b' \
 -e '/^#[ 	]*undef[ 	]/b' \
 -e '/^#[ 	]*endif/b' \
 -e 's/^[ 	]*#.*//' \
 %s | %s -C %s %s",
#ifdef LOC_SED
	  LOC_SED,
#else
	  "sed",
#endif
	  (doextract ? "-e '1,/^#/d\n'" : ""),
#endif
	  scriptname, tokenbuf, SvPV(sv, na), CPPMINUS);
	DEBUG_P(fprintf(stderr, "%s\n", buf));
	doextract = FALSE;
#ifdef IAMSUID				/* actually, this is caught earlier */
	if (euid != uid && !euid) {	/* if running suidperl */
#ifdef HAS_SETEUID
	    (void)seteuid(uid);		/* musn't stay setuid root */
#else
#ifdef HAS_SETREUID
	    (void)setreuid((Uid_t)-1, uid);
#else
#ifdef HAS_SETRESUID
	    (void)setresuid((Uid_t)-1, uid, (Uid_t)-1);
#else
	    setuid(uid);
#endif
#endif
#endif
	    if (geteuid() != uid)
		croak("Can't do seteuid!\n");
	}
#endif /* IAMSUID */
	rsfp = my_popen(buf,"r");
    }
    else if (!*scriptname) {
	taint_not("program input from stdin");
	rsfp = stdin;
    }
    else
	rsfp = fopen(scriptname,"r");
    if ((FILE*)rsfp == Nullfp) {
#ifdef DOSUID
#ifndef IAMSUID		/* in case script is not readable before setuid */
	if (euid && stat(SvPVX(GvSV(curcop->cop_filegv)),&statbuf) >= 0 &&
	  statbuf.st_mode & (S_ISUID|S_ISGID)) {
	    (void)sprintf(buf, "%s/sperl%s", BIN, patchlevel);
	    execv(buf, origargv);	/* try again */
	    croak("Can't do setuid\n");
	}
#endif
#endif
	croak("Can't open perl script \"%s\": %s\n",
	  SvPVX(GvSV(curcop->cop_filegv)), Strerror(errno));
    }
}

static void
validate_suid(validarg)
char *validarg;
{
    /* do we need to emulate setuid on scripts? */

    /* This code is for those BSD systems that have setuid #! scripts disabled
     * in the kernel because of a security problem.  Merely defining DOSUID
     * in perl will not fix that problem, but if you have disabled setuid
     * scripts in the kernel, this will attempt to emulate setuid and setgid
     * on scripts that have those now-otherwise-useless bits set.  The setuid
     * root version must be called suidperl or sperlN.NNN.  If regular perl
     * discovers that it has opened a setuid script, it calls suidperl with
     * the same argv that it had.  If suidperl finds that the script it has
     * just opened is NOT setuid root, it sets the effective uid back to the
     * uid.  We don't just make perl setuid root because that loses the
     * effective uid we had before invoking perl, if it was different from the
     * uid.
     *
     * DOSUID must be defined in both perl and suidperl, and IAMSUID must
     * be defined in suidperl only.  suidperl must be setuid root.  The
     * Configure script will set this up for you if you want it.
     */

#ifdef DOSUID
    char *s;

    if (fstat(fileno(rsfp),&statbuf) < 0)	/* normal stat is insecure */
	croak("Can't stat script \"%s\"",origfilename);
    if (statbuf.st_mode & (S_ISUID|S_ISGID)) {
	I32 len;

#ifdef IAMSUID
#ifndef HAS_SETREUID
	/* On this access check to make sure the directories are readable,
	 * there is actually a small window that the user could use to make
	 * filename point to an accessible directory.  So there is a faint
	 * chance that someone could execute a setuid script down in a
	 * non-accessible directory.  I don't know what to do about that.
	 * But I don't think it's too important.  The manual lies when
	 * it says access() is useful in setuid programs.
	 */
	if (access(SvPVX(GvSV(curcop->cop_filegv)),1))	/*double check*/
	    croak("Permission denied");
#else
	/* If we can swap euid and uid, then we can determine access rights
	 * with a simple stat of the file, and then compare device and
	 * inode to make sure we did stat() on the same file we opened.
	 * Then we just have to make sure he or she can execute it.
	 */
	{
	    struct stat tmpstatbuf;

	    if (
#ifdef HAS_SETREUID
		setreuid(euid,uid) < 0
#elif HAS_SETRESUID
		setresuid(euid,uid,(Uid_t)-1) < 0
#endif
		|| getuid() != euid || geteuid() != uid)
		croak("Can't swap uid and euid");	/* really paranoid */
	    if (stat(SvPVX(GvSV(curcop->cop_filegv)),&tmpstatbuf) < 0)
		croak("Permission denied");	/* testing full pathname here */
	    if (tmpstatbuf.st_dev != statbuf.st_dev ||
		tmpstatbuf.st_ino != statbuf.st_ino) {
		(void)fclose(rsfp);
		if (rsfp = my_popen("/bin/mail root","w")) {	/* heh, heh */
		    fprintf(rsfp,
"User %d tried to run dev %d ino %d in place of dev %d ino %d!\n\
(Filename of set-id script was %s, uid %d gid %d.)\n\nSincerely,\nperl\n",
			uid,tmpstatbuf.st_dev, tmpstatbuf.st_ino,
			statbuf.st_dev, statbuf.st_ino,
			SvPVX(GvSV(curcop->cop_filegv)),
			statbuf.st_uid, statbuf.st_gid);
		    (void)my_pclose(rsfp);
		}
		croak("Permission denied\n");
	    }
	    if (
#ifdef HAS_SETREUID
              setreuid(uid,euid) < 0
#elif defined(HAS_SETRESUID)
              setresuid(uid,euid,(Uid_t)-1) < 0
#endif
              || getuid() != uid || geteuid() != euid)
		croak("Can't reswap uid and euid");
	    if (!cando(S_IXUSR,FALSE,&statbuf))		/* can real uid exec? */
		croak("Permission denied\n");
	}
#endif /* HAS_SETREUID */
#endif /* IAMSUID */

	if (!S_ISREG(statbuf.st_mode))
	    croak("Permission denied");
	if (statbuf.st_mode & S_IWOTH)
	    croak("Setuid/gid script is writable by world");
	doswitches = FALSE;		/* -s is insecure in suid */
	curcop->cop_line++;
	if (fgets(tokenbuf,sizeof tokenbuf, rsfp) == Nullch ||
	  strnNE(tokenbuf,"#!",2) )	/* required even on Sys V */
	    croak("No #! line");
	s = tokenbuf+2;
	if (*s == ' ') s++;
	while (!isSPACE(*s)) s++;
	if (strnNE(s-4,"perl",4) && strnNE(s-9,"perl",4))  /* sanity check */
	    croak("Not a perl script");
	while (*s == ' ' || *s == '\t') s++;
	/*
	 * #! arg must be what we saw above.  They can invoke it by
	 * mentioning suidperl explicitly, but they may not add any strange
	 * arguments beyond what #! says if they do invoke suidperl that way.
	 */
	len = strlen(validarg);
	if (strEQ(validarg," PHOOEY ") ||
	    strnNE(s,validarg,len) || !isSPACE(s[len]))
	    croak("Args must match #! line");

#ifndef IAMSUID
	if (euid != uid && (statbuf.st_mode & S_ISUID) &&
	    euid == statbuf.st_uid)
	    if (!do_undump)
		croak("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* IAMSUID */

	if (euid) {	/* oops, we're not the setuid root perl */
	    (void)fclose(rsfp);
#ifndef IAMSUID
	    (void)sprintf(buf, "%s/sperl%s", BIN, patchlevel);
	    execv(buf, origargv);	/* try again */
#endif
	    croak("Can't do setuid\n");
	}

	if (statbuf.st_mode & S_ISGID && statbuf.st_gid != egid) {
#ifdef HAS_SETEGID
	    (void)setegid(statbuf.st_gid);
#else
#ifdef HAS_SETREGID
           (void)setregid((Gid_t)-1,statbuf.st_gid);
#else
#ifdef HAS_SETRESGID
           (void)setresgid((Gid_t)-1,statbuf.st_gid,(Gid_t)-1);
#else
	    setgid(statbuf.st_gid);
#endif
#endif
#endif
	    if (getegid() != statbuf.st_gid)
		croak("Can't do setegid!\n");
	}
	if (statbuf.st_mode & S_ISUID) {
	    if (statbuf.st_uid != euid)
#ifdef HAS_SETEUID
		(void)seteuid(statbuf.st_uid);	/* all that for this */
#else
#ifdef HAS_SETREUID
                (void)setreuid((Uid_t)-1,statbuf.st_uid);
#else
#ifdef HAS_SETRESUID
                (void)setresuid((Uid_t)-1,statbuf.st_uid,(Uid_t)-1);
#else
		setuid(statbuf.st_uid);
#endif
#endif
#endif
	    if (geteuid() != statbuf.st_uid)
		croak("Can't do seteuid!\n");
	}
	else if (uid) {			/* oops, mustn't run as root */
#ifdef HAS_SETEUID
          (void)seteuid((Uid_t)uid);
#else
#ifdef HAS_SETREUID
          (void)setreuid((Uid_t)-1,(Uid_t)uid);
#else
#ifdef HAS_SETRESUID
          (void)setresuid((Uid_t)-1,(Uid_t)uid,(Uid_t)-1);
#else
          setuid((Uid_t)uid);
#endif
#endif
#endif
	    if (geteuid() != uid)
		croak("Can't do seteuid!\n");
	}
	uid = (int)getuid();
	euid = (int)geteuid();
	gid = (int)getgid();
	egid = (int)getegid();
	tainting |= (euid != uid || egid != gid);
	if (!cando(S_IXUSR,TRUE,&statbuf))
	    croak("Permission denied\n");	/* they can't do this */
    }
#ifdef IAMSUID
    else if (preprocess)
	croak("-P not allowed for setuid/setgid script\n");
    else
	croak("Script is not setuid/setgid in suidperl\n");
#endif /* IAMSUID */
#else /* !DOSUID */
    if (euid != uid || egid != gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	fstat(fileno(rsfp),&statbuf);	/* may be either wrapped or real suid */
	if ((euid != uid && euid == statbuf.st_uid && statbuf.st_mode & S_ISUID)
	    ||
	    (egid != gid && egid == statbuf.st_gid && statbuf.st_mode & S_ISGID)
	   )
	    if (!do_undump)
		croak("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
	/* not set-id, must be wrapped */
    }
#endif /* DOSUID */
}

static void
find_beginning()
{
    register char *s;

    /* skip forward in input to the real script? */

    taint_not("-x");
    while (doextract) {
	if ((s = sv_gets(linestr, rsfp, 0)) == Nullch)
	    croak("No Perl script found in input\n");
	if (*s == '#' && s[1] == '!' && instr(s,"perl")) {
	    ungetc('\n',rsfp);		/* to keep line count right */
	    doextract = FALSE;
	    if (s = instr(s,"perl -")) {
		s += 6;
		/*SUPPRESS 530*/
		while (s = moreswitches(s)) ;
	    }
	    if (cddir && chdir(cddir) < 0)
		croak("Can't chdir to %s",cddir);
	}
    }
}

static void
init_debugger()
{
    GV* tmpgv;

    debstash = newHV();
    GvHV(gv_fetchpv("::_DB",TRUE, SVt_PVHV)) = debstash;
    curstash = debstash;
    dbargs = GvAV(gv_AVadd((tmpgv = gv_fetchpv("args",TRUE, SVt_PVAV))));
    SvMULTI_on(tmpgv);
    AvREAL_off(dbargs);
    DBgv = gv_fetchpv("DB",TRUE, SVt_PVGV);
    SvMULTI_on(DBgv);
    DBline = gv_fetchpv("dbline",TRUE, SVt_PVAV);
    SvMULTI_on(DBline);
    DBsub = gv_HVadd(tmpgv = gv_fetchpv("sub",TRUE, SVt_PVHV));
    SvMULTI_on(tmpgv);
    DBsingle = GvSV((tmpgv = gv_fetchpv("single",TRUE, SVt_PV)));
    SvMULTI_on(tmpgv);
    DBtrace = GvSV((tmpgv = gv_fetchpv("trace",TRUE, SVt_PV)));
    SvMULTI_on(tmpgv);
    DBsignal = GvSV((tmpgv = gv_fetchpv("signal",TRUE, SVt_PV)));
    SvMULTI_on(tmpgv);
    curstash = defstash;
}

static void
init_stacks()
{
    stack = newAV();
    mainstack = stack;			/* remember in case we switch stacks */
    AvREAL_off(stack);			/* not a real array */
    av_extend(stack,127);

    stack_base = AvARRAY(stack);
    stack_sp = stack_base;
    stack_max = stack_base + 127;

    New(54,markstack,64,I32);
    markstack_ptr = markstack;
    markstack_max = markstack + 64;

    New(54,scopestack,32,I32);
    scopestack_ix = 0;
    scopestack_max = 32;

    New(54,savestack,128,ANY);
    savestack_ix = 0;
    savestack_max = 128;

    New(54,retstack,16,OP*);
    retstack_ix = 0;
    retstack_max = 16;

    New(50,cxstack,128,CONTEXT);
    cxstack_ix	= -1;
    cxstack_max	= 128;

    New(50,tmps_stack,128,SV*);
    tmps_ix = -1;
    tmps_max = 128;

    DEBUG( {
	New(51,debname,128,char);
	New(52,debdelim,128,char);
    } )
}

static void
init_lexer()
{
    FILE* tmpfp = rsfp;

    lex_start(linestr);
    rsfp = tmpfp;
    subname = newSVpv("main",4);
}

static void
init_predump_symbols()
{
    GV *tmpgv;
    GV *othergv;

    sv_setpvn(GvSV(gv_fetchpv("\"", TRUE, SVt_PV)), " ", 1);

    stdingv = gv_fetchpv("STDIN",TRUE, SVt_PVIO);
    SvMULTI_on(stdingv);
    if (!GvIO(stdingv))
	GvIO(stdingv) = newIO();
    IoIFP(GvIO(stdingv)) = stdin;
    tmpgv = gv_fetchpv("stdin",TRUE, SVt_PVIO);
    GvIO(tmpgv) = (IO*)SvREFCNT_inc(GvIO(stdingv));
    SvMULTI_on(tmpgv);

    tmpgv = gv_fetchpv("STDOUT",TRUE, SVt_PVIO);
    SvMULTI_on(tmpgv);
    if (!GvIO(tmpgv))
	GvIO(tmpgv) = newIO();
    IoOFP(GvIO(tmpgv)) = IoIFP(GvIO(tmpgv)) = stdout;
    defoutgv = tmpgv;
    tmpgv = gv_fetchpv("stdout",TRUE, SVt_PVIO);
    GvIO(tmpgv) = (IO*)SvREFCNT_inc(GvIO(defoutgv));
    SvMULTI_on(tmpgv);

    othergv = gv_fetchpv("STDERR",TRUE, SVt_PVIO);
    SvMULTI_on(othergv);
    if (!GvIO(othergv))
	GvIO(othergv) = newIO();
    IoOFP(GvIO(othergv)) = IoIFP(GvIO(othergv)) = stderr;
    tmpgv = gv_fetchpv("stderr",TRUE, SVt_PVIO);
    GvIO(tmpgv) = (IO*)SvREFCNT_inc(GvIO(othergv));
    SvMULTI_on(tmpgv);

    statname = NEWSV(66,0);		/* last filename we did stat on */
}

static void
init_postdump_symbols(argc,argv,env)
register int argc;
register char **argv;
register char **env;
{
    char *s;
    SV *sv;
    GV* tmpgv;

    argc--,argv++;	/* skip name of script */
    if (doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    if (!argv[0][1])
		break;
	    if (argv[0][1] == '-') {
		argc--,argv++;
		break;
	    }
	    if (s = strchr(argv[0], '=')) {
		*s++ = '\0';
		sv_setpv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),s);
	    }
	    else
		sv_setiv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),1);
	}
    }
    toptarget = NEWSV(0,0);
    sv_upgrade(toptarget, SVt_PVFM);
    sv_setpvn(toptarget, "", 0);
    tmpgv = gv_fetchpv("\001",TRUE, SVt_PV);
    bodytarget = GvSV(tmpgv);
    sv_upgrade(bodytarget, SVt_PVFM);
    sv_setpvn(bodytarget, "", 0);
    formtarget = bodytarget;

    tainted = 1;
    if (tmpgv = gv_fetchpv("0",TRUE, SVt_PV)) {
	sv_setpv(GvSV(tmpgv),origfilename);
	magicname("0", "0", 1);
    }
    if (tmpgv = gv_fetchpv("\024",TRUE, SVt_PV))
	time(&basetime);
    if (tmpgv = gv_fetchpv("\030",TRUE, SVt_PV))
	sv_setpv(GvSV(tmpgv),origargv[0]);
    if (argvgv = gv_fetchpv("ARGV",TRUE, SVt_PVAV)) {
	SvMULTI_on(argvgv);
	(void)gv_AVadd(argvgv);
	av_clear(GvAVn(argvgv));
	for (; argc > 0; argc--,argv++) {
	    av_push(GvAVn(argvgv),newSVpv(argv[0],0));
	}
    }
    if (envgv = gv_fetchpv("ENV",TRUE, SVt_PVHV)) {
	HV *hv;
	SvMULTI_on(envgv);
	hv = GvHVn(envgv);
	hv_clear(hv);
#ifndef VMS  /* VMS doesn't have environ array */
	if (env != environ) {
	    environ[0] = Nullch;
	    hv_magic(hv, envgv, 'E');
	}
#endif
#ifdef DYNAMIC_ENV_FETCH
	HvNAME(hv) = savestr(ENV_HV_NAME);
#endif
	for (; *env; env++) {
	    if (!(s = strchr(*env,'=')))
		continue;
	    *s++ = '\0';
	    sv = newSVpv(s--,0);
	    sv_magic(sv, sv, 'e', *env, s - *env);
	    (void)hv_store(hv, *env, s - *env, sv, 0);
	    *s = '=';
	}
	hv_magic(hv, envgv, 'E');
    }
    tainted = 0;
    if (tmpgv = gv_fetchpv("$",TRUE, SVt_PV))
	sv_setiv(GvSV(tmpgv),(I32)getpid());

}

static void
init_perllib()
{
    char *s;
    if (!tainting) {
	s = getenv("PERL5LIB");
	if (s)
	    incpush(s);
	else
	    incpush(getenv("PERLLIB"));
    }

#ifndef PRIVLIB
#define PRIVLIB "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
    incpush(PRIVLIB);
    av_push(GvAVn(incgv),newSVpv(".",1));
}

void
calllist(list)
AV* list;
{
    jmp_buf oldtop;

    Copy(top_env, oldtop, 1, jmp_buf);

    while (AvFILL(list) >= 0) {
	CV *cv = (CV*)av_shift(list);

	SAVEFREESV(cv);
	switch (setjmp(top_env)) {
	case 0:
	    PUSHMARK(stack_sp);
	    perl_call_sv((SV*)cv, G_DISCARD);
	    break;
	case 1:
	    statusvalue = 255;	/* XXX I don't think we use 1 anymore. */
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    curstash = defstash;
	    if (endav)
		calllist(endav);
	    FREE_TMPS();
	    Copy(oldtop, top_env, 1, jmp_buf);
	    if (statusvalue) {
		if (list == beginav)
		    croak("BEGIN failed--execution aborted");
		else
		    croak("END failed--execution aborted");
	    }
	    my_exit(statusvalue);
	    /* NOTREACHED */
	    return;
	case 3:
	    if (!restartop) {
		fprintf(stderr, "panic: restartop\n");
		FREE_TMPS();
		break;
	    }
	    Copy(oldtop, top_env, 1, jmp_buf);
	    longjmp(top_env, 3);
	}
    }

    Copy(oldtop, top_env, 1, jmp_buf);
}

