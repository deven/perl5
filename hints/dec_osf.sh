# hints/dec_osf.sh
optimize="-g"
ccflags="$ccflags -DSTANDARD_C -DDEBUGGING"
lddlflags='-shared -no_archive -expect_unresolved "*" -s'
