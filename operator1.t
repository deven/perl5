#!./miniperl -D16
{
  package N;
  sub new {
    my ($obj,$n)=(shift,shift); bless $obj=\$n;
    print "New N=$n\n"; print $obj; print "\n"; $obj}
  sub DESTROY {my $ref=shift; print "DESTROY `$ref' => `$$ref'\n"; undef $ref}
  sub string {
   "N::=`" . ${$_[0]} . "'";
  }
  sub less {
   $"="' `";
   print "In N::less, arguments: `@_', assignment: `",!defined $_[2],"'\n";
   $_[2]? ${$_[0]} > $_[1]: ${$_[0]} < $_[1];
  }
  %OVERLOAD=('""' => \&string, "<" => "less");
}
{
$aa=new N 124;
#$bb=new N 345;
$bb=45;
print "\$aa=$aa, \$bb=$bb\n";
print "(\$aa<\$bb) = ",($aa<$bb),"\n";
print "\$aa=$aa, \$bb=$bb\n";
}
print "End!\n"
