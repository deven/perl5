#!/bin/sh

cat <<'END' >embed.h
/* This file is derived from global.sym and interp.sym */

/* (Doing namespace management portably in C is really gross.) */

#ifdef EMBED
#ifndef HIDE_GLOBAL_NAMES
#define HIDE_GLOBAL_NAMES 1
#endif
#endif

/* We may need to hide global names because of a name conflict 
   in an extension or because we are embedding perl within
   another program.
*/
#ifdef HIDE_GLOBAL_NAMES

/* globals we need to hide from the world */
END

sed <global.sym >>embed.h						\
	-e 's/[ 	]*#.*//'					\
	-e '/^[ 	]*$/d'						\
	-e 's/\(.*\)/#define \1		Perl_\1/'			\
	-e 's/\(................	\)	/\1/'

cat <<'END' >> embed.h

#endif /* HIDE_GLOBAL_NAMES */

/* Put interpreter specific symbols into a struct? */

#ifdef MULTIPLICITY

END


sed <interp.sym >>embed.h						\
	-e 's/[ 	]*#.*//'					\
	-e '/^[ 	]*$/d'						\
	-e 's/\(.*\)/#define \1		(curinterp->I\1)/'		\
	-e 's/\(................	\)	/\1/'

cat <<'END' >> embed.h

#else	/* not multiple, so translate interpreter symbols the other way... */

END

sed <interp.sym >>embed.h						\
	-e 's/[ 	]*#.*//'					\
	-e '/^[ 	]*$/d'						\
	-e 's/\(.*\)/#define I\1		\1/'			\
	-e 's/\(................	\)	/\1/'

cat <<'END' >> embed.h

#endif /* MULTIPLICITY */
END

