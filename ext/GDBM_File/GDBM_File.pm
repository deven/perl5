package GDBM_File;

require Exporter;
require AutoLoader;
require DynaLoader;
@ISA = (Exporter, AutoLoader, DynaLoader);
@EXPORT = qw(
	GDBM_CACHESIZE
	GDBM_FAST
	GDBM_INSERT
	GDBM_NEWDB
	GDBM_READER
	GDBM_REPLACE
	GDBM_WRCREAT
	GDBM_WRITER
);

sub AUTOLOAD {
    if (@_ > 1) {
	$AutoLoader::AUTOLOAD = $AUTOLOAD;
	goto &AutoLoader::AUTOLOAD;
    }
    local($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    ($pack,$file,$line) = caller;
	    die "Your vendor has not defined GDBM_File macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap GDBM_File;

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

sub EXISTS {
    defined FETCH(@_);
}

sub CLEAR {
    my $key = FIRSTKEY(@_);
    my @keys;

    while (defined $key) {
	push @keys, $key;
	$key = NEXTKEY(@_, $key);
    }
    foreach $key (@keys) {
	DELETE(@_, $key);
    }
}

1;
__END__
