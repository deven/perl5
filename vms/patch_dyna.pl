#!./miniperl
#
# Replace the Unix version of dl_findfile() in DynaLoader.pm with the
# VMS version.  The two versions differ sufficiently that it's difficult
# to fold them into a single routine, so we make them completely different
# for now.

sub crump {
  close OUT;
  unlink "patch_DynaLoader.tmp";
  die "@_\n";
}

if ($#ARGV == -1) { $src = 'DynaLoader.pm'; }
else { $src = $ARGV[0]; }
die "Can't find source $src" unless -f $src;

if ($#ARGV < 1) { $targ = 'DynaLoader.pm'; }
else { $targ = $ARGV[1]; }


open (IN,"$src") || die "Can't open $src: $!\n";
open (OUT,">patch_DynaLoader.tmp")|| die "Can't open patch_DynaLoader.tmp: $!\n";

while (<IN>) {
   print OUT;
   last if /__END__/;
}
if (eof(IN)) { &crump("Can't find __END__ token in $src"); };

while (<IN>) {
  last if /^sub dl_findfile/;
  print OUT;
}
if (eof(IN)) { &crump("Can't find start of original dl_findfile in $src"); };

while (<IN>) { last if /^\}/; }
if (eof(IN)) { &crump("Can't find end of original dl_findfile in $src"); };

while (<DATA>) {
  next if /^#/;
  print OUT;
}

while (<IN>) { print OUT; }

close IN;
close OUT;
rename "patch_DynaLoader.tmp",$targ;

__END__
#  VMS version of dl_findfile - differs from Unix version in two respects:
#     - uses XSUB dl_expandspec to generate full file specifications
#       according to the rules lib$find_image_symbol will apply, so caller
#       doesn't get the wrong filespec from this routine.
#     - according to VMS/lib$find_image_symbol convention, prefers plain
#       filenames (i.e. candidates for logical name translation) to specs
#       containing device, directory, type, or version information.
sub dl_findfile {
  my (@args) = @_;
  my (@dirs,  $dir);   # which directories to search
  my (@names, $name);  # file names (no dev,dir,type)
  my (@specs, $spec);  # full file specifications
  my (@found, $found); # full paths to real files we have found

  print "dl_findfile(@args)\n" if $dl_debug;

  # sort out the types of filespec in the arglist
  foreach (@args) {
    if    (/^-L(.*)/)                { push(@dirs,$1);  }
    elsif (/^-l(.*)/)                { push(@names,$1 . ".$dl_so"); }
    elsif (/(\]|]>|\:|\/)(.*)/) {
      if ($2)                        { push(@specs,$_); }
      else                           { push(@dirs,$_);  }
    }
    elsif (/(?\.|\;)/)               { push(@specs,$_); }
    else                             { push(@names,$_); }
  }

  # now look for the files - give preference to names only
  nameck: foreach $name (@names) {
    foreach $dir (@dirs, @dl_library_path) {
      last nameck if ($found[0] && !wantarray);
      if ($found = &dl_expandspec($name,$dir . ".$dl_so") {
        push(@found,$found);
        next nameck;
      }
      if ($found = &dl_expandspec($name,$dir . '.exe') {
        push(@found,$found);
        next nameck;
      }
    }
  }

  specck: foreach $spec (@specs) {
    my ($dirpath,$type,@types,@ckdirs);

    # if Basename.pm is ever added to perl distribution, replace this
    # block with ($dirpath,$spec,$type) = basename($spec,'\..*');
    while ($spec =~ /([^\]\>\/]*)([\]\>\/])/) {
      $dirpath .= $1 . $2;
      $spec = $';
    }
    if (!$dirpath) {
      while ($spec =~ /([^\:]*)\:/) {
        $dirpath .= $1 . ':';
        $spec = $';
      }
    }
    if ($spec =~ /(\..*)/) {
      $type = $1;
      $spec = $`;
    }
    @types = ($type,".$dl_so",'.exe');
    if ($dirpath) { @ckdirs = ($dirpath); }
    else { @ckdirs = (@dirs,@dl_library_path);

    foreach $dir (@ckdirs) {
      foreach $type (@types) {
        last specck if ($found[0] && !wantarray);
        if ($found = &dl_expandspec($name,$dir . $type)) {
          push(@found,$found);
          next specck;
        }
      }
    }
  }

  print STDERR "dl_findfile found: @found\n" if $dl_debug;
  return $found[0] unless wantarray;
  @found;
}

