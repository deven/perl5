all: man


POD =	../pod

MAN = 	bot.man		\
	internals.man

API =	p5api.ms

HTML =	bot.html	\
	internals.html	\
	p5api.html

man: $(MAN)

html: $(HTML)

p5api.html:
	../miniperl ms2ht p5api.ms > p5api.html

.SUFFIXES: .pod .man .html

.pod.html:
	../miniperl $(POD)/pod2html $*.pod >$*.html

.pod.man:
	../miniperl $(POD)/pod2man $*.pod >$*.man

clean:
	rm -f $(MAN) $(HTML)
