use Exporter;
package TestHarness;
@ISA=(Exporter);
@EXPORT=qw(runtests);

sub runtests {
    local($|) = 1;
    my($test,$te,$ok,$next,$max,$totmax,
       $files,$pct,$user,$sys,$cuser,$csys);
    my $bad = 0;
    my $good = 0;
    my $total = @_;
    while ($test = shift) {
      $te = $test;
      chop($te);
      print "$te" . '.' x (20 - length($te));
      my $fh = "RESULTS";
      open($fh,"$^X -x $test|") || (print "can't run.\n");
      $ok = 0;
      $next = 0;
      while (<$fh>) {
          unless (/^#/) {
              if (/^1\.\.([0-9]+)/) {
                  $max = $1;
                  $totmax += $max;
                  $files += 1;
                  $next = 1;
                  $ok = 1;
              } else {
                  $next = $1, $ok = 0, last if /^not ok ([0-9]*)/;
                  if (/^ok (.*)/ && $1 == $next) {
                      $next = $next + 1;
                  } else {
                      $ok = 0;
                  }
              }
          }
      }
      $next -= 1;
      if ($ok && $next == $max) {
          print "ok\n";
          $good += 1;
      } else {
          $next += 1;
          print "FAILED on test $next\n";
          $bad += 1;
          $_ = $test;
      }
    }
    if ($bad == 0) {
      if ($ok) {
          print "All tests successful.\n";
      } else {
          die "FAILED--no tests were run for some reason.\n";
      }
    } else {
      $pct = sprintf("%.2f", $good / $total * 100);
      if ($bad == 1) {
          warn "Failed 1 test, $pct% okay.\n";
      } else {
          die "Failed $bad/$total tests, $pct% okay.\n";
      }
    }
    ($user,$sys,$cuser,$csys) = times;
    print sprintf("u=%g  s=%g  cu=%g  cs=%g  files=%d  tests=%d\n",
                $user,$sys,$cuser,$csys,$files,$totmax);
}
