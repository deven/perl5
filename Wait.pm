package Wait;

require Exporter;
require AutoLoader;
@ISA = (Exporter, AutoLoader, DynamicLoader);
@EXPORT = qw(
	WNOHANG
	WSTOPPED
	WUNTRACED
	_WSTOPPED
	__w_coredump
	__w_retcode
	__w_stopsig
	__w_stopval
	__w_termsig
	__wait
	w_coredump
	w_retcode
	w_stopsig
	w_stopval
	w_termsig
);

sub AUTOLOAD {
    if ($AUTOLOAD =~ /::(_?[a-z])/) {
	$AutoLoader::AUTOLOAD = $AUTOLOAD;
	goto &AutoLoader::AUTOLOAD
    }
    local $constname = $AUTOLOAD;
    $constname =~ s/.*:://;
    $val = constant($constname, $_[0]);
    if ($! != 0) {
	($pack,$file,$line) = caller;
	if ($! =~ /Invalid/) {
	    die "$constname is not a valid Wait macro at $file line $line.
";
	}
	else {
	    die "Your vendor has not defined Wait macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap Wait;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# procesed by the autosplit program.

__END__
