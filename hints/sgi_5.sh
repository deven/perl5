# sgi_5.sh
i_time='define'
ccflags="$ccflags -D_POSIX_SOURCE -ansiposix -mips2 -D_BSD_TYPES -Olimit 2000"
lddlflags="-shared"
usedl='y'
