# Try to use libdbm.nfs.a since it has dbmclose.
yacc='/usr/bin/yacc -Sm11000'
libswanted=`echo dbm.nfs $libswanted | sed 's/ x$/ /'`
ccflags="$ccflags -W0 -U M_XENIX"
i_varargs=undef
d_rename='undef'
