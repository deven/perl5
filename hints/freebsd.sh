# Based on info from
# Carl M. Fongheiser <cmf@ins.infonet.net>
# Date: Thu, 28 Jul 1994 19:17:05 -0500 (CDT)
case "$osvers" in
0.*|1.0*)
	usedl="$undef"
	;;
*)	usedl="$define"
	cccdlflags='-DPIC -fpic'
	lddlflags='-Bshareable'
	;;
esac
