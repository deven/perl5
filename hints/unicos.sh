case `uname -r` in
6.1*) shellflags="-m+65536" ;;
esac
ccflags="$ccflags -DHZ=__hertz -g"
libswanted=m
d_setregid='undef'
d_setreuid='undef'
