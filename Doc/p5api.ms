.\" use groff -t -ms perl5api.ms
.\" or tbl perl5api.ms | troff -ms
.na
.TL
Perl5 Application Program Interface
.AU
Dean Roehrich
.AI
August 9, 1994 Draft
.\" Stolen from -me.  This gives me horizontal lines.
.de hl
.br
\l'\\n(.lu-\\n(.iu'
.sp
..
.NH 1
Introduction
.LP
.nh
XS is used to create an extension interface between Perl and some library
which one wishes to use with Perl.  The XS interface is combined with the
library to create a new library which can be statically or dynamically
linked to Perl.  On systems which support dynamic linking the extension can
be linked with the help of the DynamicLoader, a built-in package in Perl
which handles shared libraries.
.\"<href typemaps=TYPEMAP
.LP
.nh
The XS compiler is called xsubpp.  This compiler will embed the constructs
necessary to let C functions manipulate Perl values and creates the glue
necessary to let Perl access those functions.  The compiler uses
typemaps to determine how to map C function parameters and variables
to Perl values.  The default typemap handles many common C types.  A
supplement typemap must be created to handle special structures and types
for the library being linked.
.IP
.KS
.TS
tab(!);
l s
l l.
.\"<href MODULE PACKAGE PREFIX CODE PPCODE OUTPUT
.\"<href CLEANUP NO_INIT THIS=C++ RETVAL
\fBXS Keywords\fR
MODULE=!Module name
PACKAGE=!Package name
PREFIX=!Prefixes to be stripped from Perl function names
CODE:!Code to be inserted into a function
PPCODE:!Same as CODE: but uses the argument stack for the return values
OUTPUT:!Variables to be returned as Perl values
CLEANUP:!Code to be inserted at the end of the function
NO_INIT!Do not initialize function parameter
THIS!C++ object pointer
RETVAL!Return value
.TE
.KE
.LP
.nh
Many of the examples which follow will concentrate on creating an interface
between Perl and the ONC+ RPC bind library functions.  Specifically, the
rpcb_gettime() function will be used to demonstrate many features of
the XS language.  This function has two parameters; the first is an input
parameter and the second is an output parameter.  The function also returns
a status value.
.LP
.ti +5
bool_t rpcb_gettime(const char *host, time_t *timep);
.LP
.\"<name XS=EXAMP1
The following XS file shows one XS subroutine, or \fBXSUB\fR, which
demonstrates one possible approach for a library containing this function.
Note that the first three include statements, for
\fBEXTERN.h\fR,
\fBperl.h\fR, and \fBXSUB.h\fR, will always be present at the beginning of
an XS file.
This approach and others will be expanded on later in this document.
.LP
.KS
.nf
.in +10
\fB#include "EXTERN.h"\fR
\fB#include "perl.h"\fR
\fB#include "XSUB.h"\fR
#include <rpc/rpc.h>

MODULE = RPC\0\0PACKAGE = RPC

bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep
OUTPUT:
timep
.in 0
.fi
.KE
.LP
.nh
The following typemap describes the behavior of the \fIbool_t\fR and
\fItime_t\fR types.  This shows that bool_t is type T_IV, an integer, and
.\"<href typemap=TYPEMAP
time_t is type T_NV, a long integer.  Consult the typemap section for more
information about typemaps.
.LP
.KS
.nf
.in +10
TYPEMAP
bool_t\0\0T_IV
time_t\0\0T_NV
.in 0
.fi
.KE
.LP
.\"<href modules=PModule
Finally, the following file is the basic Perl module used for the examples.
Perl modules are explained later in this document.
.LP
.KS
.nf
.in +10
package RPC;

require Exporter;
@ISA = (Exporter, DynamicLoader);
@EXPORT = qw( rpcb_gettime );

bootstrap RPC;
1;
.in 0
.fi
.KE
.\"<line
.NH 1
The XS Language
.LP
.nh
Many functions can be handled by specifying the function name, the return
type, and the parameter names and types.  The return value of the C function
will be stored in the variable RETVAL.  The return value will be converted
to a Perl value and will be placed in the argument stack.
.LP
.KS
The following XSUB is for a function which takes a single argument and
returns a single value.
.LP
.nf
.in +10
double
sin(x)
.in +2
double<tab>x
.in 0
.fi
.KE
.LP
.nh
.\"<href typemaps=TYPEMAP rpcb_gettime=EXAMP1
The compiler expects a tab between the variable name and its type, and any
or no whitespace before the type.  When using C pointers the indirection
operator * should be considered part of the type and the address operator &
should be considered part of the variable, as is demonstrated in the
rpcb_gettime() function above.  See the section on typemaps for more about
handling qualifiers and unary operators in C types.
.LP
The parameter list of a function must not have whitespace after the
open-parenthesis or before the close-parenthesis.  
.LP
.\"<table
.KS
.TS
center, tab(!);
l20 l.
INCORRECT!CORRECT

double!double
sin(\0x\0)!sin(x)
\0\0double\0\0x!\0\0double\0\0x
.TE
.KE
.\"<endtable
.KS
.LP
The function name and the return type must be placed on separate lines.
.LP
.\"<table
.TS
center, tab(!);
l20 l.
INCORRECT!CORRECT

double sin(x)!double
\0\0double x!sin(x)
!\0\0double x
.TE
.KE
.\"<endtable
.NH 2
The Argument Stack
.LP
.nh
The argument stack is used to store the values which are sent as parameters
to the XSUB and to store the return value.  In reality all Perl
functions keep their values on this stack at the same time, each limited to
its own range of positions on the stack.  In this document the first
position on that stack which belongs to the active function will be referred
to as position 0, or \fBST(0)\fR, for that function.
.LP
.nh
The XSUB's incoming parameters and outgoing return values always begin at
ST(0).  For many simple cases the compiler will generate the code necessary
to handle the argument stack.  In more complex cases the programmer must
supply the code.
.NH 2
.\"<name RETVAL
The RETVAL variable
.LP
.nh
The RETVAL variable is a magic variable which always matches the return type
of the function.  The compiler will supply this variable in each function
and by default will use it to hold the return value of the C library
function being called.  In simple cases the value of RETVAL will be placed
in ST(0) of the argument stack where it can be received by Perl as the
return value of the XSUB.
.LP
.nh
If the XSUB has a return type of \fIvoid\fR then the RETVAL variable
will not be supplied for that function.  When using the PPCODE: directive the
RETVAL variable may not be needed.
.NH 2
.\"<name MODULE
The MODULE Keyword
.LP
.nh
The MODULE keyword is used to start the XS code and to specify the package
of the functions which are being defined.  All text preceding the first
MODULE keyword is considered C code and is passed through to the output
untouched.  Every XS module will have a bootstrap function which is used to
hook the XSUBs into Perl.  The package name of this bootstrap
function will match the value of the last MODULE statement in the XS source
files.  The value of MODULE should always remain constant within the same XS
file, though this is not required.
.LP
.nh
The following example will start the XS code and will place all functions in
the RPC package.
.LP
.ti +5
MODULE = RPC
.NH 2
.\"<name PACKAGE
The PACKAGE Keyword
.LP
.nh
When functions within an XS source file must be separated into packages the
PACKAGE keyword should be used.  This keyword is used with the MODULE
keyword and must follow immediately after it when used.
.LP
.KS
.nf
.in +5
MODULE = RPC  PACKAGE = RPC

[ XS code in package RPC ]

MODULE = RPC  PACKAGE = RPCB

[ XS code in package RPCB ]

MODULE = RPC  PACKAGE = RPC

[ XS code in package RPC ]
.in 0
.fi
.KE
.LP
Although this keyword is optional and in some cases provides redundant
information it should always be used.  Future changes in the DynamicLoader
interface may make it unclear which package a function belongs to, and the
addition of the PACKAGE keyword will protect against this possible ambiguity.
.NH 2
.\"<name PREFIX
The PREFIX Keyword
.LP
.nh
The PREFIX keyword designates prefixes which should be removed from the Perl
function names.  If the C function is \fBrpcb_gettime()\fR and the PREFIX
value is \fBrpcb_\fR then Perl will see this function as \fBgettime()\fR.
.LP
.nh
This keyword should follow the PACKAGE keyword when used.  If PACKAGE is not
used then PREFIX should follow the MODULE keyword.
.LP
.KS
.nf
.in +5
MODULE = RPC  PREFIX = rpc_

MODULE = RPC  PACKAGE = RPCB  PREFIX = rpcb_
.in 0
.fi
.KE
.NH 2
.\"<name OUTPUT
OUTPUT:
.LP
.nh
The OUTPUT: keyword indicates that certain values should be updated on the
argument stack when the XSUB terminates.  For simple functions, such
as the sin() function above, the RETVAL variable is automatically designated
as an output value.  In more complex functions the compiler will need help to
determine which variables are output variables.
.LP
.nh
This keyword will normally be used to complement the CODE: keyword.  The
RETVAL variable is not recognized as an output variable when the CODE:
keyword is present.  The OUTPUT: keyword is used in this situation to tell
the compiler that RETVAL really is an output variable.
.LP
.nh
The OUTPUT: keyword can also be used to indicate that function parameters are
output variables.  This may be necessary when a parameter has been updated
within the function and the programmer would like the update to be seen by
Perl.  If function parameters are listed under OUTPUT: with the RETVAL
variable then the RETVAL variable must be last one listed.
.LP
.KS
.nf
.in +10
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep
OUTPUT:
timep
.in 0
.fi
.KE
.LP
The OUTPUT: keyword will also allow an output parameter to be mapped to a
matching piece of code rather than to a typemap.
.LP
.KS
.nf
.in +10
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep
OUTPUT:
timep<tab>sv_setnv(ST(1), (double)timep);
.in 0
.fi
.KE
.NH 2
.\"<name CODE
CODE:
.LP
.nh
This keyword is used in more complicated functions which require
special handling for the C function.  The RETVAL variable is available but
will not be returned unless it is specified under the OUTPUT: keyword.
.LP
The following XSUB is for a function which requires special handling of
its parameters.
.LP
.KS
.nf
.in +10
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0timep
CODE:
.in +5
RETVAL = rpcb_gettime( host, &timep );
.in -5
OUTPUT:
timep
RETVAL
.in 0
.fi
.KE
.NH 2
.\"<name NO_INIT
NO_INIT
.LP
.nh
The NO_INIT keyword is used to indicate that a function parameter is being
used only as an output value.  The compiler will normally generate code to
read the values of all function parameters from the argument stack and
assign them to C variables upon entry to the function.  NO_INIT will tell
the compiler that some parameters are simply place-holders on the stack and
that they will be handled before the function terminates.
.LP
The following example shows a variation of the rpcb_gettime() function.
This function uses the timep variable only as an output variable and does not
care about its initial contents.
.LP
.KS
.nf
.in +5
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep = NO_INIT
OUTPUT:
timep
.in 0
.fi
.KE
.NH 2
Initializing Function Parameters
.LP
.nh
Function parameters are normally initialized with their values from the
argument stack.  The typemaps contain the code segments which are used to
transfer the Perl values to the C parameters.  The programmer, however, is
allowed to override the typemaps and supply alternate initialization code.
.LP
.nh
The following code demonstrates how to supply initialization code for
function parameters.  
.LP
.KS
.nf
.in +5
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host = \e"localhost\e";
time_t\0\0&timep = 0;
OUTPUT:
timep
.in 0
.fi
.KE
.LP
.nh
Note that the double-quotes for the host parameter must be backwhacked.  The
initialization code is eval'd by the compiler before it is added to the
output so anything which should be interpreted literally must be protected.
.LP
.nh
This should not be used to supply default values for parameters.  One would
normally use this when a function parameter must be processed by another
library function before it can be used.  Default parameters are covered in
the next section.  
.NH 2
Default Parameter Values
.LP
.nh
Default values can be specified for function parameters by placing an
assignment statement in the parameter list.  The default value may be a
number or a string.  Defaults should always be used on the right-most
parameters only.
.LP
.nh
To allow the Perl function for rpcb_gettime() to have a default host value
the parameters could be rearranged.
.LP
.ti +5
bool_t rpcb_gettime(time_t *timep, const char *host);
.LP
The Perl function should then have the following usage.
.LP
.nf
.in +5
$status = rpcb_gettime( $timep, $host );

$status = rpcb_gettime( $timep );
.in 0
.fi
.LP
.KS
The XSUB will look like the code which follows.  Note that both \fItimep\fR
and \fIRETVAL\fR are specified as OUTPUT: values.  When RETVAL is listed
with other values then it must be last one listed.
.LP
.nf
.in +5
bool_t
rpcb_gettime(timep,host="localhost")
.in +5
char *\0\0host
time_t\0\0timep = NO_INIT
CODE:
.in +5
RETVAL = rpcb_gettime( host, &timep );
.in -5
OUTPUT:
timep
RETVAL
.in 0
.fi
.KE
.NH 2
.\"<name PPCODE
PPCODE:
.LP
.nh
The PPCODE: keyword is an alternate form of the CODE: keyword and is used to
tell the compiler that the programmer is supplying the code to control the
argument stack for the function return values.  Occassionally one will want
an XSUB to return a list of values rather than a single value.  In
these cases one must explicitly push the list of values on the stack and
this keyword tells the compiler to expect this.  The PPCODE: and CODE:
keywords are not used together within the same XSUB.
.LP
Consider the rpcb_gettime() function.
.LP
.ti +5
bool_t rpcb_gettime(const char *host, time_t *timep);
.LP
The return type of bool_t is the status which indicates whether or not the
function succeeded.  The value of the variable timep will be set to
represent the current time for the host being queried.
.LP
.nh
This function can be left in its present form when converted to XS code, or
it can be made more interesting by having its 2 output values returned as a
single list.
.LP
.KS
.nf
.in +5
void
rpcb_gettime(host)
.in +5
char *\0\0host
PPCODE:
{
time_t	timep;
bool_t	status;
status = rpcb_gettime( host, &timep );
PUSHs(sv_2mortal(newSVnv(status)));
PUSHs(sv_2mortal(newSVnv(timep)));
}
.in 0
.fi
.KE
.LP
.nh
Notice that the programmer must supply the C code necessary to have the
rpcb_gettime() function called and to have the return values properly placed
on the argument stack.
.LP
.nh
The \fIvoid\fR return type for this function tells the xsubpp compiler that
the RETVAL variable is not needed or used and that it should not be
created.  In most scenarios the void return type should be used with the
PPCODE:  directive.
.LP
Now the rpcb_gettime() function can be used from Perl with the following
usage.
.LP
.ti +5
($status, $timep) = rpcb_gettime("localhost");
.NH 2
Returning Undef And Empty Lists
.LP
.nh
Occassionally the programmer will want to simply return \fBundef\fR or an
empty list if a function fails rather than a separate status value.  The
rpcb_gettime() function offers just this situation.  If the function
succeeds we would like to have it return the time and if it fails we would
like to have undef returned.  In the following Perl code the value of $timep
will either be undef or it will be a valid time.
.LP
.ti +5
$timep = rpcb_gettime( "localhost" );
.LP
.nh
The following XSUB uses the \fIvoid\fR return type to disable the
generation of the RETVAL variable and uses a CODE: block to indicate to the
compiler that the programmer has supplied all the necessary code.  The
sv_newmortal() call will initialize the return value to undef, making that
the default return value.
.LP
.KS
.nf
.in +5
void
rpcb_gettime(host)
.in +5
char *\0\0host
CODE:
{
time_t	timep;
bool_t x;
ST(0) = sv_newmortal();
if( rpcb_gettime( host, &timep ) )
.in +5
sv_setnv( ST(0), (double)timep);
.in -5
}
.in 0
.fi
.KE
.LP
The next example demonstrates how one would place an explicit undef in the
return value, should the need arise.
.LP
.KS
.nf
.in +5
void
rpcb_gettime(host)
.in +5
char *\0\0host
CODE:
{
time_t	timep;
bool_t x;
ST(0) = sv_newmortal();
if( rpcb_gettime( host, &timep ) ){
.in +5
sv_setnv( ST(0), (double)timep);
.in -5
}
else{
.in +5
ST(0) = &sv_undef;
.in -5
}
}
.in 0
.fi
.KE
.LP
To return an empty list one must use a PPCODE: block and then not push return
values on the stack.
.LP
.KS
.nf
.in +5
void
rpcb_gettime(host)
.in +5
char *\0\0host
PPCODE:
{
time_t	timep;
if( rpcb_gettime( host, &timep ) )
.in +5
PUSHs(sv_2mortal(newSVnv(timep)));
.in -5
else{
/* Nothing pushed on stack, so an empty */
/* list is implicitly returned. */
}
}
.in 0
.fi
.KE
.NH 2
.\"<name CLEANUP
CLEANUP:
.LP
.nh
This keyword can be used when an XSUB requires special
cleanup procedures before it terminates.  When the CLEANUP: keyword is used
it must follow any CODE:, PPCODE:, or OUTPUT: blocks which are present in
the XSUB.  The code specified for the cleanup block will be added to
the end of the function.
.LP
.nh
The previous example which shows how rpcb_gettime() can return undef can be
handled in the following manner.
.LP
.KS
.nf
.in +5
time_t
rpcb_gettime(host)
.in +5
char *\0\0host
CODE:
{
.in +5
time_t timep;
RETVAL = rpcb_gettime( host, &timep );
if( RETVAL == 1 )
.in +5
RETVAL = timep;
.in -10
}
CLEANUP:
if( RETVAL != 0 ){
.in +5
ST(0) = sv_newmortal();
sv_setnv(ST(0), (double)RETVAL);
.in -5
}
else{
.in +5
ST(0) = &sv_undef;
.in -5
}
.in 0
.fi
.KE
.NH 2
Inserting Comments and C Preprocessor Directives
.LP
.nh
Comments and C preprocessor directives are allowed within CODE:, PPCODE:,
and CLEANUP: blocks.  The compiler will pass the preprocessor directives
through untouched and will remove the commented lines.  Comments can be
added to XSUBs by placing a # at the beginning of the line.  Care
should be taken to avoid making the comment look like a C preprocessor
directive, lest it be interpreted as such.
.NH 2
.\"<name C++
Using XS With C++
.LP
.nh
If a function is defined as a C++ method then it will assume its first
argument is an object pointer.  The object pointer will be stored in a
variable called THIS.  The object should have been created by C++ with the
\fInew()\fR function and should be blessed by Perl with the sv_setptrobj()
macro.  The blessing of the object by Perl can be handled by the T_PTROBJ
typemap.
.LP
.nh
If the method is defined as static it will call the C++ function using the
class::method() syntax.  If the method is not static the function will be
called using the THIS->method() syntax.  
.\"<line
.NH 1
Interface Stategy
.LP
.nh
When designing an interface between Perl and a C library a straight
translation from C to XS is often sufficient.  The interface will often be
very C-like and occassionally non-intuitive, especially when the C function
modifies one of its parameters.  In cases where the programmer wishes to
create a more Perl-like interface the following strategy may help to
identify the more critical parts of the interface.
.LP
.nh
Identify functions which modify their parameters.  These functions may be
able to return lists to Perl, or may be candidates to return undef in case
of failure.
.LP
.nh
Identify which values are used only by the functions themselves.  If Perl
does not need to access the contents of the value then it may not be
necessary to provide a translation for that value from C to Perl.
.LP
.nh
Identify the C pointers in the function parameter lists and return values.
Some pointers can be handled with the & unary operator on the variable name
while others will require the use of the * operator on the type name.  In
general it is easier to work with the & operator.
.LP
.nh
Identify the C structures used by the functions.  In many cases it may be
helpful to use the T_PTROBJ typemap for these structures so they can be
manipulated as blessed objects in Perl.
.\"<line
.NH 1
.\"<name Module=PModule
The Perl Module
.LP
The Perl module is the link between the extension library, which was
generated from XS code, and the perl interpreter.  The module is used to
tell Perl what the extension library contains.  The name and package of the
module should match the name of the library.
.LP
The following is a Perl module for an extension containing some ONC+ RPC bind
library functions.
.LP
.KS
.nf
.in +5
package RPC;

require Exporter;
@ISA = (Exporter, DynamicLoader);
@EXPORT = qw( rpcb_gettime rpcb_getmaps rpcb_getaddr
		rpcb_rmtcall rpcb_set rpcb_unset );

bootstrap RPC;
1;
.in 0
.fi
.KE
.LP
The Exporter module should always be required and the @ISA array should
always include the Exporter and DynamicLoader classes.  The \fIbootstrap\fR
method will be inherited from the DynamicLoader class.
.\"<line
.NH 1
The h2xs Compiler
.LP
.nh
The h2xs compiler is designed to convert C header files in /usr/include into
Perl extensions.  This is similar to the h2ph compiler distributed with
earlier versions of Perl.  The compiler will create a directory under the
\fBext\fR directory of the Perl source and will populate it with a Makefile,
a Perl Module, and an XS source file.
.LP
.nh
The following command will create an extension called \fIRusers\fR from the
<rpcsvc/rusers.h> header.
.LP
.ti +5
h2xs rpcsvc/rusers
.LP
.nh
When the Rusers extension has been compiled and installed Perl can use it to
retrieve any \fI#define\fR statements which were in the C header.
.LP
.nf
.in +5
use Rusers;

print "RPC program number for rusers service: ";
print &RUSERSPROG, "\en";
.in 0
.fi
.\"<line
.NH 1
.\"<name Typemap=TYPEMAP
The Typemap
.IP
.KS
.TS
tab(!);
l s
l l.
\fBKeywords\fR
TYPEMAP!Maps XS types to C types
INPUT!Maps Perl values into C values
OUTPUT!Maps C values into Perl values
.TE
.IP
.TS
tab(!);
l s
l l.
\fBVariables\fR
$arg!Stack position of the Perl argument
$var!Name of the C variable
$type!Type of the C variable
$Package!Current PACKAGE value
.TE
.KE
.LP
.nh
A typemap may consist of 1 to 3 parts.  These parts may be in any order and
may be interspersed provided each section is properly labeled with the
appropriate keyword.
.LP
.nh
The first part is the TYPEMAP which is used to map XS types to C types.  The
compiler defaults to TYPEMAP mode when it reads a typemap file, so if the
TYPEMAP section is at the beginning of the file the keyword is not needed.
.LP
.nh
The second part is the INPUT section which maps the Perl values to the C
variables.  This consists of code segments which serve as the glue between
Perl and C.  The INPUT code is used upon entry to an XSUB.
.LP
.nh
The third part is the OUTPUT section.  This section provides the maps which
glue C variables to Perl values.  This is used to return the results of a C
function back to Perl.
.NH 2
Perl Objects And C Structures
.LP
.nh
When dealing with C structures one should select either \fBT_PTROBJ\fR or
\fBT_PTRREF\fR for the XS type.  Both types are designed to handle pointers
to complex objects.  The T_PTRREF type will allow the Perl object to be
unblessed while the T_PTROBJ type requires that the object be blessed.  By
using T_PTROBJ one can achieve a form of type-checking since the XSUB will
attempt to verify that the Perl object is of the expected type.
.LP
.nh
In Perl5 blessed objects are made to be unblessed before they are sent to
the class destructors.  Destructors can be specified for objects created
within XSUBs by specifying a function whose name ends in the word
\fBDESTROY\fR.  When objects are made blessed with the T_PTROBJ type the
compiler will assume that destructors should receive the type as an unblessed
T_PTRREF.  This type conversion will occur for any matching OBJ and REF
types.
.LP
.nh
The following XS code shows the getnetconfigent() function which is used
with ONC TIRPC.  This function will return a pointer to a C structure and
has the C prototype shown here.  The example will demonstrate how the C
pointer will become a Perl reference.  Perl will consider this reference to
be a pointer to a blessed object and will attempt to call a destructor for
the object.  A destructor will be provided in the XS source.
.LP
.ti +5
struct netconfig *getnetconfigent(const char *netid);
.LP
.nh
A typedef will be created for \fIstruct netconfig\fR.  The Perl object will
be blessed in a class matching the name of the C type, with the tag
\fBPtr\fR appended, and the name should not have embedded spaces if it will
be a Perl package name.  The destructor will be placed in the object's
package and the PREFIX keyword will be used to trim the name to the word
\fBDESTROY\fR as Perl will expect.  Error-checking within getnetconfigent()
has been omitted for this example.
.KS
.LP
.nf
.in +5
typedef struct netconfig Netconfig;

MODULE = RPC\0\0PACKAGE = RPC

Netconfig *
getnetconfigent(netid)
.in +5
char *\0\0netid
.in -5

MODULE = RPC\0\0PACKAGE = NetconfigPtr\0\0PREFIX = rpcb_

void
rpcb_DESTROY(netconf)
.in +5
Netconfig *\0\0netconf
CODE:
printf("Now in NetconfigPtr::DESTROY\en");
free( netconf );
.in 0
.fi
.KE
.LP
.nh
This example requires the following typemap entry.
.LP
.KS
.nf
.in +10
TYPEMAP
Netconfig *\0\0T_PTROBJ
.in 0
.fi
.KE
.\"<line
.NH 1
Creating A New Extension
.LP
.nh
The h2xs compiler can generate template source files and Makefiles.  These
templates may not address all the needs of the extension writer but should
offer a suitable starting point for most extensions.  The following example
demonstrates how one might use h2xs to create an extension containing the
RPC functions in this document.
.LP
.nh
The extension will not use autoloaded functions and will not define
constants, so the -A option will be given to h2xs.  The -n option will be
used to tell h2xs that this extension's name should be \fIRPC\fR.  When run
from the Perl source directory, the h2xs compiler will create the directory
ext/RPC and will populate it with files called RPC.xs, RPC.pm, and
Makefile.SH.  The XS code for the RPC functions should be added to the
RPC.xs file.  The @EXPORT list in RPC.pm should be updated to include the
functions from RPC.xs.
.LP
.ti +5
h2xs -An RPC
.LP
.nh
To compile the extension for dynamic loading the following command should be
executed from the ext/RPC directory.
.LP
.ti +5
make dynamic
.LP
.nh
If the extension will be statically linked into the perl binary then the
make command should be changed to the following.
.LP
.ti +5
make static
.LP
.nh
[More on this process]
.\"<line
.NH 1
TODO
.LP
.nf
.in +5
Building
Hooking into perlmain.c
typemaps
autosplit/autoloader
.in 0
.fi
.ex
