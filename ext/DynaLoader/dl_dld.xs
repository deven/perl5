/*
 *    Written 3/1/94, Robert Sanders <Robert.Sanders@linux.org>
 *
 * based upon the file "dl.c", which is
 *    Copyright (c) 1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Date: 1994/03/07 00:21:43 $
 * $Source: /home/rsanders/src/perl5alpha6/RCS/dld_dl.c,v $
 * $Revision: 1.4 $
 * $State: Exp $
 *
 * $Log: dld_dl.c,v $
 * Revision 1.4  1994/03/07  00:21:43  rsanders
 * added min symbol count for load_libs and switched order so system libs
 * are loaded after app-specified libs.
 *
 * Revision 1.3  1994/03/05  01:17:26  rsanders
 * added path searching.
 *
 * Revision 1.2  1994/03/05  00:52:39  rsanders
 * added package-specified libraries.
 *
 * Revision 1.1  1994/03/05  00:33:40  rsanders
 * Initial revision
 *
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <dld.h>	/* GNU DLD header file */
#include <unistd.h>


#include "dlutils.c"	/* for SaveError() etc */


WARNING! THIS IS UNTESTED CODE. AN EXAMPLE ONLY.


#ifdef OLD_CODE_NO_LONGER_NEEDED

	This code is replaced bu the auto/PACKAGE/PACKABE.bs file.
	See DynaLoader.doc for details.

/* change this to suit your system.  Under Linux, define STATICALLY_LINKED
 * if you're linking perl statically.  You can have it defined even if
 * perl is linked with the shared libraries, but then you'll end up wasting
 * space because perl will end up loading two copies of the same code.
 */
#ifdef STATICALLY_LINKED
#define LOC_LIBC "/usr/lib/libc.a"
#define LOC_LIBM "/usr/lib/libm.a"
#else
#define LOC_LIBC "/usr/lib/libc.sa"
#define LOC_LIBM "/usr/lib/libm.sa"
#endif
#define LOC_LIBDBM  "/usr/lib/libdbm.a"
/* if there are other libraries that extensions may use on your system,
 * such as libnsl, etc., add them to this array before the NULL, in the
 * order in which you'd like to see them searched.
 */
static char * stdlibs[] = { LOC_LIBDBM, LOC_LIBC, LOC_LIBM, NULL };
#endif /* OLD_CODE_NO_LONGER_NEEDED */


/* utility function to combine dld_link() and SaveError()	*/
static int
load_file(filename)
	char *filename;
{
    int dlderr = dld_link(filename);
    if (!dlderr)
	return 0;
    if (dlderr == DLD_ENOFILE){
	SaveError("File %s not found", filename);
    }else{
   	SaveError("%s",dld_strerror(dlderr));
    }
    return dlderr;
}




MODULE = DynaLoader	PACKAGE = DynaLoader


void
dl_private_init()
	PPCODE:
	dl_generic_private_init();


char *
dl_load_file(filename)
    char *	filename
    CODE:
    int dlderr;
    DLDEBUG(1,fprintf(stderr,"dl_load_file(%s): ", filename));
    RETVAL = filename;
/* we should loop for each @dl_require_symbols entry
   something like:
	foreach(@dl_require_symbols)
	    dld_create_reference($_);
*/
    dlderr = load_file(filename);
/* we should loop for each @dl_resolve_using entry
   something like:
	foreach(@dl_resolve_using)
	    res = load_file($_);
*/
    DLDEBUG(2,fprintf(stderr," libref=%s\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL != 0)
	sv_setiv(ST(0), (IV)RETVAL);


void *
dl_find_symbol(libhandle, symbolname)
    void *		libhandle
    char *		symbolname
    CODE:
    DLDEBUG(2,fprintf(stderr,"dl_find_symbol(handle=%x, symbol=%s)\n",
	    libhandle, symbolname));
    RETVAL = dld_get_func(symbolname);
    DLDEBUG(2,fprintf(stderr,"  symbolref = %x\n", RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL)
	SaveError("dl_find_symbol: Unable to find '%s' symbol", symbolname) ;
    else
	sv_setiv(ST(0), (IV)RETVAL);


void
dl_undefined_symbols()
    PPCODE:
    int x;
    char **undef_syms = dld_list_undefined_sym();
    if (!undef_syms) {
	/* is this an error? */
	SaveError("dl_undefined_symbols: symbol list not returned");
	/* return empty list */
    }else{
	EXTEND(sp, dld_undefined_sym_count);
	for (x=0; x < dld_undefined_sym_count; x++)
		PUSHs(sv_2mortal(newSVpv(undef_syms[x]+1)));
	free(undef_syms);
    }


# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
	char *		perl_name
	void *		symref 
	char *		filename
	CODE:
	DLDEBUG(2,fprintf(stderr,"dl_install_xsub(name=%s, symref=%x)\n",
		perl_name, symref));
    ST(0)=sv_2mortal(newRV((SV*)newXS(perl_name, (void(*)())symref, filename)));


char *
dl_dl_error()
	CODE:
	RETVAL = LastError ;
	OUTPUT:
	  RETVAL

# end.
