package SDBM_File;

require Exporter;
require DynaLoader;
@ISA = (Exporter, DynaLoader);
@EXPORT = qw(new FETCH STORE DELETE FIRSTKEY NEXTKEY error clearerr);

bootstrap SDBM_File;

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
