/*
 * "The Road goes ever on and on, down from the door where it began."
 */

#include "INTERN.h"
#include "perl.h"

static void xs_init _((void));

int
main(argc, argv, env)
int argc;
char **argv;
char **env;
{
    int exitstatus;
    PerlInterpreter *my_perl;

#ifdef VMS
    getredirection(&argc,&argv);
#endif

    my_perl = perl_alloc();
    if (!my_perl)
	exit(1);
    perl_construct( my_perl );

    exitstatus = perl_parse( my_perl, xs_init, argc, argv, env );
    if (exitstatus)
	exit( exitstatus );

    exitstatus = perl_run( my_perl );

    perl_destruct( my_perl );
    perl_free( my_perl );

    exit( exitstatus );
}

/* Register any extra external extensions */

static void
xs_init()
{
    /* Do not delete this line--writemain depends on it */
}
