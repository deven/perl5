/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */

/* $RCSfile: regexp.h,v $$Revision: 4.1 $$Date: 92/08/07 18:26:35 $
 *
 * $Log:	regexp.h,v $
 */

typedef struct regexp {
	char **startp;
	char **endp;
	SV *regstart;		/* Internal use only. */
	char *regstclass;
	SV *regmust;		/* Internal use only. */
	I32 regback;		/* Can regmust locate first try? */
	I32 minlen;		/* mininum possible length of $& */
	I32 prelen;		/* length of precomp */
	char *precomp;		/* pre-compilation regular expression */
	char *subbase;		/* saved string so \digit works forever */
	char *subbeg;		/* same, but not responsible for allocation */
	char *subend;		/* end of subbase */
	char reganch;		/* Internal use only. */
	char do_folding;	/* do case-insensitive match? */
	char lastparen;		/* last paren matched */
	char nparens;		/* number of parentheses */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

#define ROPT_ANCH 1
#define ROPT_SKIP 2
#define ROPT_IMPLICIT 4
