all: man

MAN =	perl.man	\
	perlbook.man	\
	perldata.man	\
	perldebug.man	\
	perlform.man	\
	perlfunc.man	\
	perlipc.man	\
	perlmod.man	\
	perlop.man	\
	perlre.man	\
	perlrun.man	\
	perlsec.man	\
	perlstyle.man	\
	perlsub.man	\
	perlsyn.man	\
	perltrap.man	\
	perlvar.man

HTML =	perl.html	\
	perlbook.html	\
	perldata.html	\
	perldebug.html	\
	perlform.html	\
	perlfunc.html	\
	perlipc.html	\
	perlmod.html	\
	perlop.html	\
	perlre.html	\
	perlrun.html	\
	perlsec.html	\
	perlstyle.html	\
	perlsub.html	\
	perlsyn.html	\
	perltrap.html	\
	perlvar.html

man: $(MAN)

html: $(HTML)

.SUFFIXES: .pod .man

.pod.man:
	../miniperl pod2man $*.pod >$*.man

.SUFFIXES: .pod .html

.pod.html:
	../miniperl pod2html $*.pod >$*.html

clean:
	rm -f $(MAN) $(HTML)

