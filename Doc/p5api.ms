.\" -ms
.\" use groff -t -ms perl5api.ms
.\" or tbl perl5api.ms | troff -ms
.\" -ms
.\"============ macro definitions ===========================================
.\"-----------------------------------------------------------------------
.\" cS -- start of code snippet
.de cS
.   sp
.   nr PS -1
.   RS
.   nf
.   ft CW
..
.\"-----------------------------------------------------------------------
.\" cE -- end of code snippet
.de cE
.  nr PS +1
.  RE
.  fi
.  ft R
.  sp
..
.\"-----------------------------------------------------------------------
.\" sH -- section heading
.\"       $1 - the title for the section heading
.de sH
.SH
\\$1
.XS \\n%
\fB\\$1\fP
.XE
..
.\" we don't want hyphenation thankyouverymuch
.nr HY 0
.\"============ end of macro definitions =================================
.na
.ds DY
.RP
.TL
Perl5 Application Programming Interface
.AU
Dean Roehrich
.AI
September 27, 1994
.AB
.LP
XS is a language used to create an extension interface between Perl and some
C library which one wishes to use with Perl.  The XS interface is combined
with the library to create a new library which can be linked to Perl.  An
XSUB is a function in the XS language and is the core component of the
Perl application interface.
.AE
.\" ----------- end of document header ----------------------------------
.\"
.ds CF - \\n% -
.ds CH \fIPerl5 Application Programming Interface\fP\\l'|0\\(ul'
.\" ----------- end of setup --------------------------------------------
.\"<TOC
.\"<name/X Introduction
.sH "Introduction"
.LP
XS is a language used to create an extension interface between Perl and some
C library which one wishes to use with Perl.  The XS interface is combined
with the library to create a new library which can be linked to Perl.  An
\fBXSUB\fR is a function in the XS language and is the core component of the
Perl application interface.
.\"<href typemaps=Typemap
.LP
The XS compiler is called \fBxsubpp\fR.  This compiler will embed the
constructs necessary to let an XSUB, which is really a C function in
disguise, manipulate Perl values and creates the glue necessary to let Perl
access the XSUB.  The compiler uses \fBtypemaps\fR to determine how to map C
function parameters and variables to Perl values.  The default typemap
handles many common C types.  A supplement typemap must be created to handle
special structures and types for the library being linked.
.LP
Many of the examples which follow will concentrate on creating an interface
between Perl and the ONC+ RPC bind library functions.  Specifically, the
rpcb_gettime() function will be used to demonstrate many features of
the XS language.  This function has two parameters; the first is an input
parameter and the second is an output parameter.  The function also returns
a status value.
.cS
bool_t rpcb_gettime(const char *host, time_t *timep);
.cE
.LP
From C this function will be called with the following statements.
.cS
#include <rpc/rpc.h>
bool_t status;
time_t timep;
status = rpcb_gettime( "localhost", &timep );
.cE
.LP
If an XSUB is created to offer a direct translation between this function
and Perl, then this XSUB will be used from Perl with the following code.  The
$status and $timep variables will contain the output of the function.
.cS
use RPC;
$status = rpcb_gettime( "localhost", $timep );
.cE
.\"<name XS=EXAMP1
.LP
The following XS file shows an XS subroutine, or XSUB, which demonstrates
one possible interface to the rpcb_gettime() function.  This XSUB represents
a direct translation between C and Perl and so preserves the interface even
from Perl.  This XSUB will be invoked from Perl with the usage shown above.
Note that the first three \fI#include\fR statements, for
\fBEXTERN.h\fR,
\fBperl.h\fR, and \fBXSUB.h\fR, will always be present at the beginning of
an XS file.  This approach and others will be expanded later in this
document.
.KS
.cS
\fB#include "EXTERN.h"\fP
\fB#include "perl.h"\fP
\fB#include "XSUB.h"\fP
#include <rpc/rpc.h>

MODULE = RPC\0\0PACKAGE = RPC

bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep
OUTPUT:
timep
.cE
.KE
.LP
.\"<href "Perl module"=Module
Any extension to Perl, including those containing XSUBs, should have a
Perl module to serve as the bootstrap which pulls the extension into Perl.
This module will export the extension's functions and variables to the Perl
program and will cause the extension's XSUBs to be linked into Perl.  The
following module will be used for most of the examples in this document and
should be used from Perl with the \fBuse\fR command as shown earlier.  Perl
modules are explained in more detail later in this document.
.KS
.cS
package RPC;

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT = qw( rpcb_gettime );

bootstrap RPC;
1;
.cE
.KE
.LP
Throughout this document a variety of interfaces to the rpcb_gettime() XSUB
will be explored.  The XSUBs will take their parameters in different orders
or will take different numbers of parameters.  In each case the XSUB is an
abstraction between Perl and the real C rpcb_gettime() function, and the
XSUB must always ensure that the real rpcb_gettime() function is called with
the correct parameters.  This abstraction will allow the programmer to
create a more Perl-like interface to the C function.
.\"<line
.\"<name/X "The Anatomy of an XSUB"=Anatomy
.sH "The Anatomy of an XSUB"
.LP
.KS
The following XSUB allows a Perl program to access a C library function
called sin().  The XSUB will imitate the C function which takes a single
argument and returns a single value.
.cS
double
sin(x)
.in +2
double<tab>x
.cE
.KE
.LP
.\"<href typemaps=Typemap
.\"<href rpcb_gettime()=EXAMP1
The compiler expects a tab between the parameter name and its type, and any
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
.cS
.TS
center, tab(!);
l20 l.
INCORRECT!CORRECT

double!double
sin(\0x\0)!sin(x)
\0\0double\0\0x!\0\0double\0\0x
.TE
.cE
.KE
.\"<endtable
.KS
.LP
The function name and the return type must be placed on separate lines.
.LP
.\"<table
.cS
.TS
center, tab(!);
l20 l.
INCORRECT!CORRECT

double sin(x)!double
\0\0double\0\0x!sin(x)
!\0\0double\0\0x
.TE
.cE
.KE
.\"<endtable
.\"<name/X "The Argument Stack"=Stack
.sH "The Argument Stack"
.LP
The argument stack is used to store the values which are sent as parameters
to the XSUB and to store the XSUB's return value.  In reality all Perl
functions keep their values on this stack at the same time, each limited to
its own range of positions on the stack.  In this document the first
position on that stack which belongs to the active function will be referred
to as position 0 for that function.
.LP
XSUBs refer to their stack arguments with the macro \fBST(x)\fR, where
\fIx\fR refers to a position in this XSUB's part of the stack.  Position 0
for that function would be known to the XSUB as ST(0).  The XSUB's incoming
parameters and outgoing return values always begin at ST(0).  For many
simple cases the xsubpp compiler will generate the code necessary to handle
the argument stack by embedding code fragments found in the typemaps.  In
more complex cases the programmer must supply the code.
.\"<name/X "The RETVAL Variable"=RETVAL
.sH "The RETVAL Variable"
.LP
The RETVAL variable is a magic variable which always matches the return type
of the C library function.  The xsubpp compiler will supply this variable in
each XSUB and by default will use it to hold the return value of the C
library function being called.  In simple cases the value of RETVAL will be
placed in ST(0) of the argument stack where it can be received by Perl as the
return value of the XSUB.
.LP
If the XSUB has a return type of \fIvoid\fR then the compiler will not
supply a RETVAL variable for that function.  When using the PPCODE:
directive the RETVAL variable may not be needed.
.\"<name/X "The MODULE Keyword"=MODULE
.sH "The MODULE Keyword"
.LP
The MODULE keyword is used to start the XS code and to specify the package
of the functions which are being defined.  All text preceding the first
MODULE keyword is considered C code and is passed through to the output
untouched.  Every XS module will have a bootstrap function which is used to
hook the XSUBs into Perl.  The package name of this bootstrap
function will match the value of the last MODULE statement in the XS source
files.  The value of MODULE should always remain constant within the same XS
file, though this is not required.
.LP
The following example will start the XS code and will place all functions in
a package named RPC.
.cS
MODULE = RPC
.cE
.\"<name/X "The PACKAGE Keyword"=PACKAGE
.sH "The PACKAGE Keyword"
.LP
When functions within an XS source file must be separated into packages the
PACKAGE keyword should be used.  This keyword is used with the MODULE
keyword and must follow immediately after it when used.
.KS
.cS
MODULE = RPC  PACKAGE = RPC

[ XS code in package RPC ]

MODULE = RPC  PACKAGE = RPCB

[ XS code in package RPCB ]

MODULE = RPC  PACKAGE = RPC

[ XS code in package RPC ]
.cE
.KE
.LP
Although this keyword is optional and in some cases provides redundant
information it should always be used.  This keyword will ensure
that the XSUBs appear in the desired package.
.\"<name/X "The PREFIX Keyword"=PREFIX
.sH "The PREFIX Keyword"
.LP
The PREFIX keyword designates prefixes which should be removed from the Perl
function names.  If the C function is \fBrpcb_gettime()\fR and the PREFIX
value is \fBrpcb_\fR then Perl will see this function as \fBgettime()\fR.
.LP
This keyword should follow the PACKAGE keyword when used.  If PACKAGE is not
used then PREFIX should follow the MODULE keyword.
.KS
.cS
MODULE = RPC  PREFIX = rpc_

MODULE = RPC  PACKAGE = RPCB  PREFIX = rpcb_
.cE
.KE
.\"<name/X "The OUTPUT: Keyword"=OUTPUT
.sH "The OUTPUT: Keyword"
.LP
The OUTPUT: keyword indicates that certain function parameters should be
updated (new values made visible to Perl) when the XSUB terminates or that
certain values should be returned to the calling Perl function.  For simple
functions, such as the sin() function above, the RETVAL variable is
automatically designated as an output value.  In more complex functions the
xsubpp compiler will need help to determine which variables are output
variables.
.LP
This keyword will normally be used to complement the CODE: keyword.  The
RETVAL variable is not recognized as an output variable when the CODE:
keyword is present.  The OUTPUT: keyword is used in this situation to tell
the compiler that RETVAL really is an output variable.
.LP
The OUTPUT: keyword can also be used to indicate that function parameters are
output variables.  This may be necessary when a parameter has been modified
within the function and the programmer would like the update to be seen by
Perl.  If function parameters are listed under OUTPUT: along with the RETVAL
variable then the RETVAL variable must be the last one listed.
.KS
.cS
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep
OUTPUT:
timep
.cE
.KE
.LP
The OUTPUT: keyword will also allow an output parameter to be mapped to a
matching piece of code rather than to a typemap.
.KS
.cS
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep
OUTPUT:
timep<tab>sv_setnv(ST(1), (double)timep);
.cE
.KE
.\"<name/X "The CODE: Keyword"=CODE
.sH "The CODE: Keyword"
.LP
This keyword is used in more complicated XSUBs which require
special handling for the C function.  The RETVAL variable is available but
will not be returned unless it is specified under the OUTPUT: keyword.
.LP
The following XSUB is for a C function which requires special handling of
its parameters.  The Perl usage is given first.
.cS
$status = rpcb_gettime( "localhost", $timep );
.cE
.LP
The XSUB follows.
.KS
.cS
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
.cE
.KE
.LP
In many of the examples shown here the CODE: block (and other blocks) will
often be contained within braces ( \fB{\fR and \fB}\fR ).  This protects the
CODE: block from complex INPUT typemaps and ensures the resulting C code is
legal.
.\"<name/X "The NO_INIT Keyword"=NO_INIT
.sH "The NO_INIT Keyword"
.LP
The NO_INIT keyword is used to indicate that a function parameter is being
used only as an output value.  The xsubpp compiler will normally generate
code to read the values of all function parameters from the argument stack
and assign them to C variables upon entry to the function.  NO_INIT will tell
the compiler that some parameters will be used for output rather than for
input and that they will be handled before the function terminates.
.LP
The following example shows a variation of the rpcb_gettime() function.
This function uses the timep variable only as an output variable and does not
care about its initial contents.
.KS
.cS
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host
time_t\0\0&timep = NO_INIT
OUTPUT:
timep
.cE
.KE
.\"<name/X "Initializing Function Parameters"=Initializing
.sH "Initializing Function Parameters"
.LP
Function parameters are normally initialized with their values from the
argument stack.  The typemaps contain the code segments which are used to
transfer the Perl values to the C parameters.  The programmer, however, is
allowed to override the typemaps and supply alternate initialization code.
.LP
The following code demonstrates how to supply initialization code for
function parameters.  The initialization code is eval'd by the compiler
before it is added to the output so anything which should be interpreted
literally, such as double quotes, must be protected with backslashes.
.KS
.cS
bool_t
rpcb_gettime(host,timep)
.in +5
char *\0\0host = (char *)SvPV(ST(0),na);
time_t\0\0&timep = 0;
OUTPUT:
timep
.cE
.KE
.LP
This should not be used to supply default values for parameters.  One would
normally use this when a function parameter must be processed by another
library function before it can be used.  Default parameters are covered in
the next section.  
.\"<name/X "Default Parameter Values"=Defaults
.sH "Default Parameter Values"
.LP
Default values can be specified for function parameters by placing an
assignment statement in the parameter list.  The default value may be a
number or a string.  Defaults should always be used on the right-most
parameters only.
.LP
To allow the XSUB for rpcb_gettime() to have a default host value the
parameters to the XSUB could be rearranged.  The XSUB will then call the
real rpcb_gettime() function with the parameters in the correct order.  Perl
will call this XSUB with either of the following statements.
.KS
.cS
$status = rpcb_gettime( $timep, $host );

$status = rpcb_gettime( $timep );
.cE
.KE
.LP
.KS
The XSUB will look like the code which follows.  A CODE: block is used to
call the real rpcb_gettime() function with the parameters in the correct
order for that function.
.cS
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
.cE
.KE
.\"<name/X "Variable-length Parameter Lists"=VARARGS
.sH "Variable-length Parameter Lists"
.LP
XSUBs can have variable-length parameter lists by specifying an ellipsis (...)
in the parameter list.  This use of the ellipsis is similar to that found in
ANSI C.  The programmer is able to determine the number of arguments passed
to the XSUB by examining the \fBitems\fR variable which the xsubpp compiler
supplies for all XSUBs.  By using this mechanism one can create an XSUB
which accepts a list of parameters of unknown length.
.LP
The \fIhost\fR parameter for the rpcb_gettime() XSUB can be optional so the
ellipsis can be used to indicate that the XSUB will take a variable number
of parameters.  Perl should be able to call this XSUB with either of the
following statments.
.KS
.cS
$status = rpcb_gettime( $timep, $host );

$status = rpcb_gettime( $timep );
.cE
.KE
.LP
The XS code, with ellipsis, follows.
.KS
.cS
bool_t
rpcb_gettime(timep, ...)
.in +5
time_t\0\0timep = NO_INIT
CODE:
{
char *host = "localhost";

if( items > 1 )
.in +5
host = (char *)SvPV(ST(1), na);
.in -5
RETVAL = rpcb_gettime( host, &timep );
}
OUTPUT:
timep
RETVAL
.cE
.KE
.\"<name/X "The PPCODE: Keyword"=PPCODE
.sH "The PPCODE: Keyword"
.LP
The PPCODE: keyword is an alternate form of the CODE: keyword and is used to
tell the xsubpp compiler that the programmer is supplying the code to
control the argument stack for the XSUBs return values.  Occassionally
one will want an XSUB to return a list of values rather than a single
value.  In these cases one must use PPCODE: and then explicitly push the
list of values on the stack.  The PPCODE: and CODE:  keywords are not used
together within the same XSUB.
.LP
The following XSUB will call the C rpcb_gettime() function and will return
its two output values, timep and status, to Perl as a single list.
.KS
.cS
void
rpcb_gettime(host)
.in +5
char *\0\0host
PPCODE:
{
time_t	timep;
bool_t	status;
status = rpcb_gettime( host, &timep );
EXTEND(sp, 2);
PUSHs(sv_2mortal(newSVnv(status)));
PUSHs(sv_2mortal(newSVnv(timep)));
}
.cE
.KE
.LP
Notice that the programmer must supply the C code necessary to have the real
rpcb_gettime() function called and to have the return values properly placed
on the argument stack.
.LP
The \fIvoid\fR return type for this function tells the xsubpp compiler that
the RETVAL variable is not needed or used and that it should not be
created.  In most scenarios the void return type should be used with the
PPCODE:  directive.
.LP
The EXTEND() macro is used to make room on the argument stack for 2 return
values.  The PPCODE: directive causes the xsubpp compiler to create a stack
pointer called \fBsp\fR, and it is this pointer which is being used in the
EXTEND() macro.  The values are then pushed onto the stack with the PUSHs()
macro.
.LP
Now the rpcb_gettime() function can be used from Perl with the following
statement.
.cS
($status, $timep) = rpcb_gettime("localhost");
.cE
.\"<name/X "Returning Undef And Empty Lists"=Undef
.sH "Returning Undef And Empty Lists"
.LP
Occassionally the programmer will want to simply return \fBundef\fR or an
empty list if a function fails rather than a separate status value.  The
rpcb_gettime() function offers just this situation.  If the function
succeeds we would like to have it return the time and if it fails we would
like to have undef returned.  In the following Perl code the value of $timep
will either be undef or it will be a valid time.
.cS
$timep = rpcb_gettime( "localhost" );
.cE
.LP
The following XSUB uses the \fIvoid\fR return type to disable the
generation of the RETVAL variable and uses a CODE: block to indicate to the
compiler that the programmer has supplied all the necessary code.  The
sv_newmortal() call will initialize the return value to undef, making that
the default return value.
.KS
.cS
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
.cE
.KE
.LP
The next example demonstrates how one would place an explicit undef in the
return value, should the need arise.
.KS
.cS
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
.cE
.KE
.LP
To return an empty list one must use a PPCODE: block and then not push return
values on the stack.
.KS
.cS
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
.cE
.KE
.\"<name/X "The CLEANUP: Keyword"=CLEANUP
.sH "The CLEANUP: Keyword"
.LP
This keyword can be used when an XSUB requires special
cleanup procedures before it terminates.  When the CLEANUP: keyword is used
it must follow any CODE:, PPCODE:, or OUTPUT: blocks which are present in
the XSUB.  The code specified for the cleanup block will be added as the
last statements in the XSUB.
.\"<name/X "The BOOT: Keyword"=BOOT
.sH "The BOOT: Keyword"
.LP
The BOOT: keyword is used to add code to the extension's bootstrap
function.  The bootstrap function is generated by the xsubpp compiler and
normally holds the statements necessary to register any XSUBs with Perl.
With the BOOT: keyword the programmer can tell the compiler to add extra
statements to the bootstrap function.
.LP
This keyword may be used anytime after the first MODULE keyword and should
appear on a line by itself.  The first blank line after the keyword will
terminate the code block.
.KS
.cS
BOOT:
# The following message will be printed when the
# bootstrap function executes.
printf("Hello from the bootstrap!\en");
.cE
.KE
.\"<name/X "Inserting Comments and C Preprocessor Directives"=Comments
.sH "Inserting Comments and C Preprocessor Directives"
.LP
Comments and C preprocessor directives are allowed within CODE:, PPCODE:,
BOOT:, and CLEANUP: blocks.  The compiler will pass the preprocessor
directives through untouched and will remove the commented lines.  Comments
can be added to XSUBs by placing a # at the beginning of the line.  Care
should be taken to avoid making the comment look like a C preprocessor
directive, lest it be interpreted as such.
.\"<name/X "Using XS With C++"=C++
.sH "Using XS With C++"
.LP
If a function is defined as a C++ method then it will assume its first
argument is an object pointer.  The object pointer will be stored in a
variable called THIS.  The object should have been created by C++ with the
\fInew()\fR function and should be blessed by Perl with the sv_setptrobj()
macro.  The blessing of the object by Perl can be handled by the T_PTROBJ
typemap.
.LP
If the method is defined as static it will call the C++ function using the
class::method() syntax.  If the method is not static the function will be
called using the THIS->method() syntax.
.\"<line
.\"<name/X "Perl Variables"=Variables
.sH "Perl Variables"
.LP
The following demonstrates how the Perl variable $host can be accessed from
an XSUB.  The function \fBperl_get_sv()\fR is used to obtain a pointer to the
variable, known as an \fBSV\fR (Scalar Variable) internally.  The package
name \fIRPC\fR will be added to the name of the variable so perl_get_sv()
will know in which package $host can be found.  If the package name is not
supplied then perl_get_sv() will search package \fImain\fR for the variable.
The macro \fBSvPVX()\fR is then used to dereference the SV to obtain a char*
pointer to its contents.
.KS
.cS
void
rpcb_gettime()
.in +5
PPCODE:
{
char *host;
SV *hostsv;
time_t timep;

hostsv = perl_get_sv( "RPC::host", FALSE );
if( hostsv != NULL ){
.in +5
host = SvPVX( hostsv );
if( rpcb_gettime( host, &timep ) )
.in +5
PUSHs(sv_2mortal(newSVnv(timep)));
.in -10
}
}
.cE
.KE
.LP
This Perl code can be used to call that XSUB.
.KS
.cS
$RPC::host = "localhost";
$timep = rpcb_gettime();
.cE
.KE
.LP
In the above example the SV contained a C \fIchar*\fR but a Perl scalar
variable may also contain numbers and references.  If the SV is expected to
have a C \fIint\fR then the macro \fBSvIVX()\fR should be used to dereference
the SV.  When the SV contains a C \fIdouble\fR then \fBSvNVX()\fR should be
used.
.LP
The macro \fBSvRV()\fR can be used to dereference an SV when it is a Perl
reference.  The result will be another SV which points to the actual Perl
variable.  This can then be dereferenced with SvPVX(), SvNVX(), or SvIVX().
The following XSUB will use SvRV().
.KS
.cS
void
rpcb_gettime()
.in +5
PPCODE:
{
char *host;
SV *rv;
SV *hostsv;
time_t timep;

rv = perl_get_sv( "RPC::host", FALSE );
if( rv != NULL ){
.in +5
hostsv = SvRV( rv );
host = SvPVX( hostsv );
if( rpcb_gettime( host, &timep ) )
.in +5
PUSHs(sv_2mortal(newSVnv(timep)));
.in -10
}
}
.cE
.KE
.LP
This Perl code will create a variable $RPC::host which is a reference to
$MY::host.  The variable $MY::host contains the hostname which will be used.
.KS
.cS
$MY::host = "localhost";
$RPC::host = \e$MY::host;
$timep = rpcb_gettime();
.cE
.KE
.LP
The second argument to perl_get_sv() will normally be \fBFALSE\fR as shown
in the above examples.  An argument of \fBTRUE\fR will cause variables to be
created if they do not already exist.  One should not use TRUE unless steps
are taken to deal with a possibly empty SV.
.LP
XSUBs may use \fBperl_get_av()\fR, \fBperl_get_hv()\fR, and
\fBperl_get_cv()\fR to access Perl arrays, hashes, and code values.
.\"<line
.\"<name/X "Interface Stategy"=Interface
.sH "Interface Stategy"
.LP
When designing an interface between Perl and a C library a straight
translation from C to XS is often sufficient.  The interface will often be
very C-like and occassionally non-intuitive, especially when the C function
modifies one of its parameters.  In cases where the programmer wishes to
create a more Perl-like interface the following strategy may help to
identify the more critical parts of the interface.
.LP
Identify the C functions which modify their parameters.  The XSUBs for these
functions may be able to return lists to Perl, or may be candidates to
return undef or an empty list in case of failure.
.LP
Identify which values are used only by the C and XSUB functions themselves.
If Perl does not need to access the contents of the value then it may not be
necessary to provide a translation for that value from C to Perl.
.LP
Identify the pointers in the C function parameter lists and return values.
Some pointers can be handled in XS with the & unary operator on the variable
name while others will require the use of the * operator on the type name.
In general it is easier to work with the & operator.
.LP
Identify the structures used by the C functions.  In many cases it may be
helpful to use the T_PTROBJ typemap for these structures so they can be
manipulated by Perl as blessed objects.
.\"<line
.\"<name/X "The Perl Module"=Module
.sH "The Perl Module"
.LP
The Perl module is the link between the extension library, which was
generated from XS code, and the Perl interpreter.  The module is used to
tell Perl what the extension library contains.  The name and package of the
module should match the name of the library.
.LP
The following is a Perl module for an extension containing some ONC+ RPC bind
library functions.
.KS
.cS
package RPC;

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT = qw( rpcb_gettime rpcb_getmaps rpcb_getaddr
		rpcb_rmtcall rpcb_set rpcb_unset );

bootstrap RPC;
1;
.cE
.KE
.LP
The RPC extension contains the functions found in the @EXPORT list.  By
using the \fBExporter\fR module the RPC module can make these function names
visible to the rest of the Perl program.  The \fBDynaLoader\fR module will
allow the RPC module to bootstrap the extension library.  To load this
extension and make the functions available, the following Perl statement
should be used.
.cS
use RPC;
.cE
.LP
For more information about the DynaLoader consult its documentation in the
ext/DynaLoader directory in the Perl source.
.\"<line
.\"<name/X "Perl Objects And C Structures"=Objects
.sH "Perl Objects And C Structures"
.LP
When dealing with C structures one should select either \fBT_PTROBJ\fR or
\fBT_PTRREF\fR for the XS type.  Both types are designed to handle pointers
to complex objects.  The T_PTRREF type will allow the Perl object to be
unblessed while the T_PTROBJ type requires that the object be blessed.  By
using T_PTROBJ one can achieve a form of type-checking since the XSUB will
attempt to verify that the Perl object is of the expected type.
.LP
The following XS code shows the getnetconfigent() function which is used
with ONC TIRPC.  The getnetconfigent() function will return a pointer to a C
structure and has the C prototype shown below.  The example will demonstrate
how the C pointer will become a Perl reference.  Perl will consider this
reference to be a pointer to a blessed object and will attempt to call a
destructor for the object.  A destructor will be provided in the XS source
to free the memory used by getnetconfigent().  Destructors in XS can be
created by specifying an XSUB function whose name ends with the word
\fBDESTROY\fR.  XS destructors can be used to free memory which may have been
malloc'd by another XSUB.
.cS
struct netconfig *getnetconfigent(const char *netid);
.cE
.LP
A typedef will be created for \fIstruct netconfig\fR.  The Perl object will
be blessed in a class matching the name of the C type, with the tag
\fBPtr\fR appended, and the name should not have embedded spaces if it will
be a Perl package name.  The destructor will be placed in a class
corresponding to the class of the object and the PREFIX keyword will be used
to trim the name to the word DESTROY as Perl will expect.
.KS
.cS
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
.cE
.KE
.LP
This example requires the following typemap entry.
.\"<href typemap=Typemap
Consult the typemap section for more information about adding new typemaps
for an extension.
.KS
.cS
TYPEMAP
Netconfig *\0\0T_PTROBJ
.cE
.KE
.LP
This example will be used with the following Perl statements.
.KS
.cS
use RPC;
$netconf = getnetconfigent("udp");
.cE
.KE
.LP
When Perl destroys the object referenced by $netconf it will send the object
to the supplied XSUB DESTROY function.  Perl cannot determine, and does not
care, that this object is a C struct and not a Perl object.  In this sense,
there is no difference between the object created by the getnetconfigent()
XSUB and an object created by a normal Perl subroutine.
.\"<line
.\"<name/X "C Headers and Perl"=Headers
.sH "C Headers and Perl"
.LP
The h2xs compiler is designed to convert C header files in /usr/include into
Perl extensions.  This compiler will create a directory under the
\fBext\fR directory of the Perl source and will populate it with a Makefile,
a Perl Module, an XS source file, and a MANIFEST file.
.LP
The following command will create an extension called \fIRusers\fR from the
<rpcsvc/rusers.h> header.
.cS
h2xs rpcsvc/rusers
.cE
.LP
When the Rusers extension has been compiled and installed Perl can use it to
retrieve any \fI#define\fR statements which were in the C header.
.KS
.cS
use Rusers;
print "RPC program number for rusers service: ";
print &RUSERSPROG, "\en";
.cE
.KE
.\"<line
.\"<name/X "Creating A New Extension"=Extensions
.sH "Creating A New Extension"
.LP
The h2xs compiler can generate template source files and Makefiles.  These
templates offer a suitable starting point for most extensions.  The
following example demonstrates how one might use h2xs to create an extension
containing the RPC functions in this document.
.LP
The extension will not use autoloaded functions and will not define
constants, so the -A option will be given to h2xs.  When run
from the Perl source directory, the h2xs compiler will create the directory
ext/RPC and will populate it with files called RPC.xs, RPC.pm, Makefile.PL,
and MANIFEST.  The XS code for the RPC functions should be added to the
RPC.xs file.  The @EXPORT list in RPC.pm should be updated to include the
functions from RPC.xs.
.cS
h2xs -An RPC
.cE
.LP
To compile the extension for dynamic loading the following command should be
executed from the ext/RPC directory.
.cS
make dynamic
.cE
.LP
If the extension will be statically linked into the perl binary then the
makefile (use \fImakefile\fR, not Makefile) in the Perl source directory
should be edited to add \fBext/RPC/RPC.a\fR to the \fBstatic_ext\fR variable.
Before making this change Perl should have already been built.  After the
makefile has been updated the following command should be executed from the
Perl source directory.
.cS
make
.cE
.LP
Perl's \fBConfigure\fR script can also be used to add extensions.  The
extension should be placed in the ext directory under the Perl source before
Perl has been built and prior to running Configure.  When Configure is run
it will find the extension along with the other extensions in the ext
directory and will add it to the list of extensions to be built.  When
make is run the extension will be built along with the other extensions.
.LP
Configure recognizes extensions if they have an XS source file which matches
the name of the extension directory.  If the extension directory includes a
MANIFEST file Configure will search that file for any \fB.SH\fR files and
extract them after it extracts all the other .SH files listed in the main
MANIFEST.  The main Perl Makefile will then run \fImake\fR in the
extension's directory if it finds an XS file matching the name of the
extension's directory.
.\"<line
.\"<name/X "The Typemap"=Typemap
.sH "The Typemap"
.LP
The typemap is a collection of code fragments which are used by the
xsubpp compiler to map C function parameters and values to Perl values.  The
typemap file may consist of three sections labeled \fBTYPEMAP\fR,
\fBINPUT\fR, and \fBOUTPUT\fR.  The INPUT section tells the compiler how to
translate Perl values into variables of certain C types.  The OUTPUT section
tells the compiler how to translate the values from certain C types into
values Perl can understand.  The TYPEMAP section tells the compiler which of
the INPUT and OUTPUT code fragments should be used to map a given C type to
a Perl value.  Each of the sections of the typemap must be preceded by one
of the TYPEMAP, INPUT, or OUTPUT keywords.
.LP
The default typemap in the ext directory of the Perl source contains many
useful types which can be used by Perl extensions.  Some extensions define
additional typemaps which they keep in their own directory.  These
additional typemaps may reference INPUT and OUTPUT maps in the main
typemap.  The xsubpp compiler will allow the extension's own typemap to
override any mappings which are in the default typemap.
.bp
.LP
Most extensions which require a custom typemap will need only the TYPEMAP
section of the typemap file.  The custom typemap used in the
getnetconfigent() example shown earlier demonstrates what may be the typical
use of extension typemaps.  That typemap is used to equate a C structure
with the T_PTROBJ typemap.  The typemap used by getnetconfigent() is shown
here.  Note that the C type is separated from the XS type with a tab and that
the C unary operator * is considered to be a part of the C type name.
.KS
.cS
TYPEMAP
Netconfig *<tab>T_PTROBJ
.cE
.KE
.\" ****************************************************
.\" ****************************************************
.\"<line
.bp
.\"<name/X "Appendix A"=Appendix_A
.sH "Appendix A"
.LP
.KS
File \fBRPC.xs\fR: Interface to some ONC+ RPC bind library functions.
.cS
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <rpc/rpc.h>

typedef struct netconfig Netconfig;

MODULE = RPC\0\0PACKAGE = RPC

void
rpcb_gettime(host="localhost")
.in +5
char *	host
CODE:
{
time_t	timep;
ST(0) = sv_newmortal();
if( rpcb_gettime( host, &timep ) )
.in +5
sv_setnv( ST(0), (double)timep );
.in -5
}
.in -5

Netconfig *
getnetconfigent(netid="udp")
.in +5
char *\0\0netid
.in -5

MODULE = RPC\0\0PACKAGE = NetconfigPtr\0\0PREFIX = rpcb_

void
rpcb_DESTROY(netconf)
.in +5
Netconfig *\0\0netconf
CODE:
printf("NetconfigPtr::DESTROY\en");
free( netconf );
.cE
.KE
.\" **************************
.KS
.LP
File \fBtypemap\fR: Custom typemap for RPC.xs.
.cS
TYPEMAP
Netconfig *\0\0T_PTROBJ
.cE
.KE
.bp
.\" **************************
.KS
.LP
File \fBRPC.pm\fR: Perl module for the RPC extension.
.cS
package RPC;

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(rpcb_gettime getnetconfigent);

bootstrap RPC;
1;
.cE
.KE
.\" **************************
.KS
.LP
File \fBrpctest.pl\fR: Perl test program for the RPC extension.
.cS
use RPC;

$netconf = getnetconfigent();
$a = rpcb_gettime();
print "time = $a\en";
print "netconf = $netconf\en";

$netconf = getnetconfigent("tcp");
$a = rpcb_gettime("poplar");
print "time = $a\en";
print "netconf = $netconf\en";
.cE
.KE
.\" ****************************************************
.\" ****************************************************
.\" .bp
.\" .\"<line
.\" .\"<name/X TODO
.\" .sH "TODO"
.\" .LP
.\" .nf
.\" .in +5
.\" autosplit/autoloader
.\" .in 0
.\" .fi
.\"======================================================================
.\" Generate a table of contents
.\"======================================================================
.bp
.nr % 1
.af % i
.PX
.ex
