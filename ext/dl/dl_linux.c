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

#include <dld.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <unistd.h>

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

static char *
stdlibs[] = {
  LOC_LIBDBM,
  LOC_LIBC,
  LOC_LIBM,
  NULL
};

static char *
stdpaths[] = {
  "",				/* for absolute paths */
  "/usr/lib/",
  "/lib/",
  NULL
};

static void
dump_undefined_syms()
{
  char **undef_syms;
  int x;

  undef_syms = dld_list_undefined_sym();

  if (!undef_syms) {
    fprintf(stderr, "symbol list not returned\n");
    return;
  }

  fprintf(stderr, "dld_dl: There %s %d undefined symbol%s:\n", 
	  dld_undefined_sym_count > 1 ? "are" : "is",
	  dld_undefined_sym_count,
	  dld_undefined_sym_count > 1 ? "s" : "");

  for (x=0; x < dld_undefined_sym_count; x++)
    fprintf(stderr,"-- %s\n", undef_syms[x] + 1); /* strip the leading '_' */

  free(undef_syms);
}


void
load_libs(char **libtbl, int precount, int search)
{
  int i, p, maxp, dlderr;
  char libpath[1024];

  if (!libtbl) return;

  for (p=0; (dld_undefined_sym_count > precount) && stdpaths[p]; p++)
    {
      for (i=0; (dld_undefined_sym_count > precount) && libtbl[i]; i++) 
	{
	  sprintf(libpath, "%s%s", stdpaths[p], libtbl[i]);

	  if (! access(libpath, R_OK))
	    {
	      /* dump_undefined_syms(); */
	      /* fprintf(stderr, "undefined syms, linking in %s\n", libpath); */
	      dlderr = dld_link(libpath);
	  
	      if (dlderr && dlderr != DLD_ENOFILE)
		fprintf(stderr, "dld_link returned error on %s: %s\n",
			dld_link, dld_strerror(dlderr));
	    }
	}
      if (!search) break;
    }
}


static int
XS_DynamicLoader_bootstrap(ix, ax, items)
register int ix;
register int ax;
register int items;
{
    if (items < 1 || items > 1) {
	croak("Usage: DynamicLoader::bootstrap(package)");
    }
    {
	char*	package = SvPV(ST(1),na);
	int res = 0;
	int (*bootproc)();
	char pathname[1024];
	char bootname[128];
	char libname[128];
	AV *av = GvAVn(incgv);
	I32 i;
	char **private_libs;	/* package-private libraries */
	int already_undefined;

	/* we won't worry about pre-existing undefined symbols */
	/* actually, this is a cheesy method and we will, as we
	 * can't tell if 1 old symbol was defined and 1 new one
	 * was left undefined.  we'd actually have to compare
	 * symbols to see if any new ones were left undefined.
	 */
	already_undefined = dld_undefined_sym_count;

	sprintf(bootname, "boot_%s", package);
	dld_create_reference(bootname);

	for (i = 0; i <= AvFILL(av); i++) {
	    (void)sprintf(pathname, "%s/auto/%s/%s.o",
		SvPVx(*av_fetch(av, i, TRUE), na), package, package);
	    res = dld_link(pathname);

	    if (! res) /* reverse sense of dlopen() */
		break;
	    else if (res != DLD_ENOFILE)
	      fprintf(stderr, "dld_dl: tried to load %s and failed: %s\n", 
		     pathname, dld_strerror(res));
	}
	if (res)
	    croak("Can't find loadable object for package %s in @INC", package);

	bootproc = (int (*)())dld_get_func(bootname);
	if (!bootproc)
	    croak("Shared object %s contains no %s function", pathname, bootname);


	/* load any package-specified libraries */
	sprintf(libname, "lib_%s", package);
	private_libs = (char **) dld_get_symbol(libname);
	if (private_libs)
	  load_libs(private_libs, 0, 1);

	/* load standard system libraries */
	load_libs(stdlibs, 0, 0);

	if (dld_function_executable_p(bootname))
	  bootproc();
	else
	  croak("Function %s in shared object %s is not executable", 
		bootname, pathname);

	ST(0) = sv_mortalcopy(&sv_yes);
    }
    return ax;
}

static int
XS_DynamicLoader_unload(ix, ax, items)
int ix;
int ax;
int items;
{
  char *package = SvPV(ST(1),na);
  char tmpbuf[1024];
  AV *av = GvAVn(incgv);
  int res, i;

  fprintf(stderr, "in unload: %s\n", package);

  for (i = 0; i <= AvFILL(av); i++) {
    (void)sprintf(tmpbuf, "%s/auto/%s/%s.o",
		  SvPVx(*av_fetch(av, i, TRUE), na), package, package);

    res = dld_unlink_by_file(tmpbuf, 1);
    
    if (! res) /* reverse sense of dlopen() */
      break;
    else
      fprintf(stderr, "dl_unload: tried to unload %s and failed: %s\n", 
	      tmpbuf, dld_strerror(res));
  }

  ST(0) = sv_mortalcopy(res ? &sv_no : &sv_yes);
  return ax;
}

static int
XS_DynamicLoader_dld_link(ix, ax, items)
int ix;
int ax;
int items;
{
  char *package = SvPV(ST(1),na);
  int res;

  fprintf(stderr, "in dld_link: %s\n", package);
  res = dld_link(package);
  ST(0) = sv_mortalcopy(res ? &sv_no : &sv_yes);
  return ax;
}

static int
XS_DynamicLoader_dld_unlink(ix, ax, items)
int ix;
int ax;
int items;
{
  char *package = SvPV(ST(1),na);
  int res;

  fprintf(stderr, "in dld_unlink: %s\n", package);
  res = dld_unlink_by_file(package, 1);
  ST(0) = sv_mortalcopy(res ? &sv_no : &sv_yes);
  return ax;
}

static int
XS_DynamicLoader_dld_list_undefined_sym(ix, ax, items)
int ix;
int ax;
int items;
{
  fprintf(stderr, "in dld_list_undefined_sym\n");
  dump_undefined_syms();
  return ax;
}

static int
XS_DynamicLoader_dld_create_reference(ix, ax, items)
int ix;
int ax;
int items;
{
  char *symbol = SvPV(ST(1),na);
  int res;

  fprintf(stderr, "in dld_create_reference: %s\n", symbol);
  res = dld_create_reference(symbol);
  ST(0) = sv_mortalcopy(res ? &sv_no : &sv_yes);
  return ax;
}

static int
XS_DynamicLoader_dld_remove_defined_symbol(ix, ax, items)
int ix;
int ax;
int items;
{
  char *symbol = SvPV(ST(1),na);
  int res;

  fprintf(stderr, "in dld_remove_defined_symbol: %s\n", symbol);
  dld_remove_defined_symbol(symbol);
  ST(0) = sv_mortalcopy(&sv_yes);
  return ax;
}

int
boot_DynamicLoader(ix,ax,items)
int ix;
int ax;
int items;
{
    char* file = __FILE__;

    dld_init( dld_find_executable(origargv[0]) );
    newXSUB("DynamicLoader::bootstrap", 0, XS_DynamicLoader_bootstrap, file);
    newXSUB("DynamicLoader::unload", 0, XS_DynamicLoader_unload, file);
    newXSUB("DynamicLoader::dld_link", 0, XS_DynamicLoader_dld_link, file);
    newXSUB("DynamicLoader::dld_unlink", 0, XS_DynamicLoader_dld_unlink, file);
    newXSUB("DynamicLoader::dld_list_undefined_sym", 0, 
	    XS_DynamicLoader_dld_list_undefined_sym, file);
    newXSUB("DynamicLoader::dld_create_reference", 0, 
	    XS_DynamicLoader_dld_create_reference, file);
    newXSUB("DynamicLoader::dld_remove_defined_symbol", 0, 
	    XS_DynamicLoader_dld_remove_defined_symbol, file);
}
