package AutoLoader;
use Carp;

AUTOLOAD {
    my $name = "auto/$AUTOLOAD.al";
    $name =~ s#::#/#g;
    eval {require $name};
    if ($@) {
	$@ =~ s/ at .*\n//;
	croak $@;
    }
    goto &$AUTOLOAD;
}

1;
