ccflags="$ccflags -Olimit 2900"
libswanted=m
tmp=`(uname -a) 2>/dev/null`
case "$tmp" in
OSF1*)
    case "$tmp" in
    *mips)
	d_volatile=define
	;;
    *)
	cat <<EOFM
You are not supposed to know about that machine...
EOFM
	;; 
    esac
    ;;
esac
#toke_cflags='optimize="-g"'
regcomp_cflags='optimize="-g -O0"'
regexec_cflags='optimize="-g -O0"'
