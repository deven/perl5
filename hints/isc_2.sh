# Interactive Unix Version 2.2.
#  Compile perl entirely in posix mode. 
#  Andy Dougherty		doughera@lafcol.lafayette.edu
#  Sat Jul  2 15:37:35 EDT 1994
#
# Use Configure -Dcc=gcc to use gcc
#
hintfile=isc_2
set X `echo $libswanted | sed -e 's/ c / /' -e 's/ c_s / /' -e 's/ PW / /'`
shift
libswanted="$*"
case "$cc" in
*gcc*)	;;
*)	ccflags="$ccflags -Xp -D_POSIX_SOURCE"
	ldflags="$ldflags -Xp"
    	;;
esac
doio_cflags='ccflags="$ccflags -DENOTSOCK=103"'
pp_cflags='ccflags="$ccflags -DENOTSOCK=103"'
