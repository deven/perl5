#!./miniperl
{
  package N;
  sub new {my ($obj,$n)=(shift,shift);
	if (ref $n eq "N") {print "New copy of $n\n"; return $n};
	print "New N=$n\n"; bless \$n}
  sub DESTROY {my $ref=shift; print "DESTROY `$ref' => `$$ref'\n"; undef $ref}
  sub string {
   $"="' `";
#   print "In N::string, arguments:\n";
#   print " `@_', assignment: `",!defined $_[2],"'\n";
   "N::=`" . ${$_[0]} . "'";
  }
  sub add {
   $"="' `";
   print "In N::add, arguments:";
   print " `@_', assignment: `",!defined $_[2],"'\n";
   new N ${$_[0]}+$_[1];
  }
  sub less {
   $"="' `";
   print "In N::less, arguments: `@_', assignment: `",!defined $_[2],"'\n";
   $_[2]? $_[1] < ${$_[0]} : ${$_[0]} < $_[1];
  }
  sub lesseq {
   $"="' `";
   print "In N::lesseq, arguments: `@_', assignment: `",!defined $_[2],"'\n";
   $_[2]? $_[1] <= ${$_[0]} : ${$_[0]} <= $_[1];
  }
  sub incr {my $n=shift;$$n++}
  sub not {new N !${$_[0]}}
  %OVERLOAD=("+" => "add", "<" => "less", '""' => \&string, "++" => "incr",
	"<=" => "lesseq", "!" => "not");
  #$main::aa=bless \$main::a;
  #@M::ISA=(N);
  #bless $main::bb=\$main::b;
}
{
$aa=new N 124;
$bb=new N 345;
$cc=$aa;
print "\$aa+=5: `",$aa+=5,"'\n";
print "++\$aa: `",++$aa,"'\n";
print "\$aa++: `",$aa++,"'\n";
print "\$aa: `",$aa,"'\n";
#$aa=1;
#$aa++;
#1+$bb;
print "\$aa: `",$aa,"' ref \$aa: `",ref $aa,"'\n";
print "\$aa: `",$aa,"'\n";
print "\$bb: `",$bb,"'\n";
print "\$aa+\$bb: `",$aa+$bb,"'\n";
print "\$aa<\$bb: `",($aa<$bb),"'\n";
print "\$bb: `",$bb,"'\n";
print "\$bb<\$aa: `",($bb<$aa),"'\n";
print "\$bb: `",$bb,"'\n";
print "\$aa<=\$bb: `",($aa<=$bb),"'\n";
print "\$bb<=\$aa: `",($bb<=$aa),"'\n";
print "\$cc: `",$cc,"'\n";
print "!\$cc: `",!$cc,"'\n";
print "\$cc: `",$cc,"'\n";
}
print "End!\n"
