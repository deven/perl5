/* dl_none.xs
 * 
 * Stubs for platforms that do not support dynamic linking
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

MODULE = DynaLoader	PACKAGE = DynaLoader

void
dl_private_init()
	PPCODE:

char *
dl_dl_error()
	CODE:
	RETVAL = "Not implemented";
	OUTPUT:
	RETVAL

# end.
