usevfork=false
d_suidsafe=define
ccflags="$ccflags"
set `echo $glibpth | sed -e 's@/usr/ucblib@@'`
glibpth="$*"
case $PATH in
*/usr/ucb*:/usr/bin:*) cat <<END
NOTE:  Some people have reported problems with /usr/ucb/cc.  
Remove /usr/ucb from your PATH if you have difficulties.
END
;;
esac

