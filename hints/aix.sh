d_setrgid='undef'
d_setruid='undef'
alignbytes=8
: Versions prior to 3.2.5 can not handle -O for pp.c
case "$osvers" in
3.2.0|3.2.4) pp_cflags='optimize="-g"' ;;
esac
