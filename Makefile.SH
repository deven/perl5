case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

case "$d_dosuid" in
*define*) suidperl='suidperl' ;;
*) suidperl='';;
esac

: Configure sets byacc=byacc if byacc is not found.  We reset it to ''
case "$byacc" in
''|'byacc') byacc='';;
esac

: Prepare dependency lists for Makefile.
dynamic_list=' '
for f in $dynamic_ext; do
    dynamic_list="$dynamic_list ${f}.$so"
done

static_list=' '
for f in $static_ext; do
    static_list="$static_list $f.a"
done

echo "Extracting Makefile (with variable substitutions)"
$spitshell >Makefile <<'!NO!SUBS!'
# $Makefile.SH,v $Revision: 4.1 $Date: 92/08/07 17:18:08 $
# This file is derived from Makefile.SH.  Any changes made here will
# be lost the next time you run Configure.
#  Makefile is used to generate makefile.  The only difference
#  is that makefile has the dependencies filled in at the end.
#
# $Log:	Makefile.SH,v $
# 
!NO!SUBS!

$spitshell >>Makefile <<!GROK!THIS!
# I now supply perly.c with the kits, so don't remake perly.c without byacc
BYACC = $byacc
CC = $cc
bin = $installbin
scriptdir = $scriptdir
privlib = $installprivlib
mansrc = $mansrc
manext = $manext
LDFLAGS = $ldflags
CLDFLAGS = $ldflags

SMALL = $small
LARGE = $large $split
mallocsrc = $mallocsrc
mallocobj = $mallocobj
LNS = $lns
RMS = rm -f
ranlib = $ranlib

# The following are used to build and install shared libraries for
# dynamic loading.
LDDLFLAGS = $lddlflags
CCDLFLAGS = $ccdlflags
CCCDLFLAGS = $cccdlflags
SHLIBSUFFIX = .$so

dynamic_ext = $dynamic_list
static_ext = $static_list
ext = \$(dynamic_ext) \$(static_ext)

libs = $libs $cryptlib

public = perl $suidperl

shellflags = $shellflags

## To use an alternate make, set \$altmake in config.sh.
MAKE = ${altmake-make}
!GROK!THIS!

## In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

CCCMD = `sh $(shellflags) cflags $@`

private = 

scripts =

manpages = perl.man

util =

sh = Makefile.SH cflags.SH embed_h.SH makedepend.SH makedir.SH writemain.SH

h1 = EXTERN.h INTERN.h XSUB.h av.h config.h cop.h cv.h dosish.h 
h2 = embed.h form.h gv.h handy.h hv.h keywords.h mg.h op.h
h3 = opcode.h patchlevel.h perl.h perly.h pp.h proto.h regcomp.h
h4 = regexp.h scope.h sv.h unixish.h util.h
h = $(h1) $(h2) $(h3) $(h4)

c1 = av.c scope.c op.c doop.c doio.c dump.c hv.c
c2 = $(mallocsrc) mg.c perl.c perly.c pp.c regcomp.c regexec.c
c3 = gv.c sv.c taint.c toke.c util.c deb.c run.c

c = $(c1) $(c2) $(c3) miniperlmain.c perlmain.c

s1 = av.c scope.c op.c doop.c doio.c dump.c hv.c
s2 = $(mallocsrc) mg.c perl.c perly.c pp.c regcomp.c regexec.c
s3 = gv.c sv.c taint.c toke.c util.c deb.c run.c perly.c

saber = $(s1) $(s2) $(s3)

obj1 = av.o scope.o op.o doop.o doio.o dump.o hv.o
obj2 = $(mallocobj) mg.o perly.o pp.o regcomp.o regexec.o
obj3 = gv.o sv.o taint.o toke.o util.o deb.o run.o

obj = $(obj1) $(obj2) $(obj3)

lintflags = -hbvxac

addedbyconf = UU

# grrr
SHELL = /bin/sh

.c.o:
	$(CCCMD) $*.c

all: makefile miniperl lib/Config.pm $(public) $(dynamic_ext)
	cd x2p; $(MAKE) all
	touch all

# Phony target to force checking subdirectories.
FORCE:

!NO!SUBS!

: Now on to the rest of the Makefile.
$spitshell >>Makefile <<'!NO!SUBS!'
# The $& notation tells Sequent machines that it can do a parallel make,
# and is harmless otherwise.

miniperl: $& miniperlmain.o perl.o $(obj)
	$(CC) $(LARGE) $(CLDFLAGS) -o miniperl miniperlmain.o perl.o $(obj) $(libs)

perlmain.c: miniperlmain.c config.sh makefile
	sh writemain $(static_ext) > tmp
	sh mv-if-diff tmp perlmain.c

perlmain.o: perlmain.c

# The file ext.libs is a list of libraries that must be linked in
# for static extensions, e.g. -lm -lgdbm, etc.  The individual
# static extension Makefile.SH's add to it.
ext.libs:	$(static_ext)
	-@test -f ext.libs || touch ext.libs

perl: $& perlmain.o perl.o $(obj) $(static_ext) ext.libs
	$(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o perl perlmain.o perl.o $(obj) $(static_ext) $(libs) `cat ext.libs`

pureperl: $& perlmain.o perl.o $(obj) $(static_ext) ext.libs
	purify $(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o pureperl perlmain.o perl.o $(obj) $(static_ext) $(libs) `cat ext.libs`

quantperl: $& perlmain.o perl.o $(obj) $(static_ext) ext.libs
	quantify $(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o quantperl perlmain.o perl.o $(obj) $(static_ext) $(libs) `cat ext.libs`

libperl.rlb: libperl.a
	$(ranlib) libperl.a
	touch libperl.rlb

libperl.a: $& perl.o $(obj)
	ar rcuv libperl.a $(obj)
	$(ranlib) libperl.a

# This version, if specified in Configure, does ONLY those scripts which need
# set-id emulation.  Suidperl must be setuid root.  It contains the "taint"
# checks as well as the special code to validate that the script in question
# has been invoked correctly.

suidperl: $& sperl.o perlmain.o $(obj) $(static_ext) ext.libs
	$(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o suidperl perlmain.o sperl.o $(obj) $(static_ext) $(libs) `cat ext.libs`

lib/Config.pm: config.sh miniperl
	./miniperl configpm

saber: $(saber)
	# load $(saber)
	# load /lib/libm.a

sperl.o: perl.c perly.h patchlevel.h $(h)
	$(RMS) sperl.c
	$(LNS) perl.c sperl.c
	$(CCCMD) -DIAMSUID sperl.c
	$(RMS) sperl.c

opcode.h: opcode.pl
	- perl opcode.pl

embed.h: embed_h.SH global.sym interp.sym
	sh embed_h.SH

install: all
	./perl installperl
!NO!SUBS!

: Only print out the rules for running byacc if the user _has_ byacc.
: Otherwise, comment them out.  Users who really know what they are
: doing can uncomment them and run yacc or bison or whatever.
: Configure sets byacc=byacc if byacc is not found.
case "$byacc" in
''|'byacc')
	comment1='#' 
	comment2='' ;;
*)	comment1=''
	comment2='#' ;;
esac

$spitshell >>Makefile <<!GROK!THIS!

perly.h: perly.c
	@ echo Dummy dependency for dumb parallel make
	touch perly.h

# I now supply perly.c with the kits, so the following section is
# commented out if you don't have byacc.

${comment1}perly.c: 	perly.y perly.c.diff
${comment1}	@ echo 'Expect' 117 shift/reduce and 1 reduce/reduce conflict
${comment1}	\$(BYACC) -d perly.y
${comment1}	sh \$(shellflags) ./perly.fixer y.tab.c perly.c
${comment1}	mv y.tab.h perly.h
${comment1}	echo 'extern YYSTYPE yylval;' >>perly.h

# This version is used only if you do not have byacc.
${comment2}perly.c:	perly.y
${comment2}	touch perly.c

!GROK!THIS!

$spitshell >>Makefile <<'!NO!SUBS!'
# Extensions:
# Names added to $(dynamic_ext) or $(static_ext) will automatically
# get built.  There should ordinarily be no need to change any of 
# this part of makefile.
# The dummy dependency is a place holder in case $(dynamic_ext) or
# $(static_ext) is empty.

d_dummy $(dynamic_ext):	FORCE
	@x=`echo "$@" | sed 's/$(SHLIBSUFFIX)//g'` ; \
	if test -d ext/$$x; then \
	echo " "; echo "	Making $@"; \
	cd ext/$$x; \
	if test ! -f Makefile ; then \
	test -f Makefile.SH && sh Makefile.SH ; \
	fi ; \
	$(MAKE) dynamic ; \
	cd ../.. ; \
	fi

s_dummy $(static_ext):	FORCE
	@x=`echo "$@" | sed 's/\.a//g'` ; \
	if test -d ext/$$x; then \
	echo " "; echo "	Making $@"; \
	cd ext/$$x; \
	if test ! -f Makefile ; then \
	test -f Makefile.SH && sh Makefile.SH ; \
	fi ; \
	$(MAKE) static CCCDLFLAGS="" ; \
	cd ../.. ; \
	fi

clean:
	rm -f *.o *.a all perl miniperl
	rm -f perlmain.c
	rm -f ext.libs ext/util/extlibist
	-cd x2p; $(MAKE) clean
	-@for x in ext/* ; do \
	if test -f $$x/Makefile; then \
	echo "	Making clean in $$x"; \
	cd $$x; $(MAKE) clean ; cd ../.. ; \
	fi ; \
	done

realclean: clean
	-cd x2p; $(MAKE) realclean
	-@for x in ext/* ; do \
	if test -f $$x/Makefile; then \
	echo "	Making realclean in $$x"; \
	cd $$x; $(MAKE) realclean ; cd ../.. ; \
	fi ; \
	done
	rm -f *.orig */*.orig *~ */*~ core t/core
	rm -rf $(addedbyconf)
	rm -f Makefile cflags makedepend makedir writemain
	rm -f config.h t/perl makefile makefile.old
	rm -f x2p/Makefile x2p/makefile x2p/makefile.old x2p/cflags
	rm -f lib/Config.pm
	rm -f h2ph h2ph.man c2ph pstruct
	rm -rf .config

# The following lint has practically everything turned on.  Unfortunately,
# you have to wade through a lot of mumbo jumbo that can't be suppressed.
# If the source file has a /*NOSTRICT*/ somewhere, ignore the lint message
# for that spot.

lint: perly.c $(c)
	lint $(lintflags) $(defs) perly.c $(c) > perl.fuzz

makefile:	Makefile
	make depend

# When done, touch perlmain.c so that it doesn't get remade each time.
depend: makedepend
	- test -f perly.h || cp /dev/null perly.h
	./makedepend
	- test -s perly.h || /bin/rm -f perly.h
	- test -s perlmain.c && touch perlmain.c
	cd x2p; $(MAKE) depend

test: miniperl perl lib/Config.pm $(dynamic_ext)
	- cd t && chmod +x TEST */*.t
	- cd t && (rm -f perl; $(LNS) ../perl perl) && ./perl TEST </dev/tty

clist:	$(c)
	echo $(c) | tr ' ' '\012' >.clist

hlist:  $(h)
	echo $(h) | tr ' ' '\012' >.hlist

shlist: $(sh)
	echo $(sh) | tr ' ' '\012' >.shlist

# AUTOMATICALLY GENERATED MAKE DEPENDENCIES--PUT NOTHING BELOW THIS LINE
# If this runs make out of memory, delete /usr/include lines.
!NO!SUBS!

$eunicefix Makefile
case `pwd` in
*SH)
    $rm -f ../Makefile
    ln Makefile ../Makefile
    ;;
esac
rm -f makefile


