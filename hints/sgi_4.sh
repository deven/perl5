optimize='-O1'
usemymalloc='y'
d_voidsig=define
usevfork=false
d_charsprf=undef
libswanted=`echo $libswanted | sed 's/c_s \(.*\)/\1 c_s/'`
ccflags="$ccflags -DLANGUAGE_C -DBSD_SIGNALS -cckr -signed"
