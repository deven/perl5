#!/usr/bin/perl
#
#  mms2make.pl
#  Version 1.0 06-Aug-1994
#  Charles Bailey  bailey@genetics.upenn.edu
#
#  Convert Descrip.MMS file to Makefile:
#    - minor syntax changes
#    - replace $(MMS$*) macros with &* macros
#    - comment out .ifdef blocks
#      (block with ##default## in same line as .ifdef or .else
#       is not commented out)

if ($#ARGV > -1 && $ARGV[0] =~ /^[\-\/]trim/i) {
  $do_trim = 1;
  shift @ARGV;
}
$infile = $#ARGV > -1 ? $ARGV[0] : "Descrip.MMS";
$outfile = ($#ARGV > 0 ? $ARGV[1] : "Makefile.");

open(INFIL,$infile) || die "Can't open $infile: $!\n"; 
open(OUTFIL,">$outfile") || die "Can't open $outfile: $!\n"; 

print OUTFIL "#> This file produced from $infile by $0\n";
print OUTFIL "#> Lines beginning with \"#>\" were commented out during the\n";
print OUTFIL "#> conversion process.  For more information, see $0\n";
print OUTFIL "#>\n";

while (<INFIL>) {
  s/$infile/$outfile/eoi;
  if (/^\#/) { 
    if (!/^\#\:/) {print OUTFIL;}
    next;
  }
  $prefix = $commenting[$depth];
  if (($new = /^\.ifdef/i) || /^\.else/i){
    $prefix = 1;
    $depth++ if $new;
    $commenting[$depth] = (!/\#\#default\#\#/);
  }
  elsif (/^\.endif/i) {
    $prefix = 1;
    warn "Error in .ifdef/.endif nesting\n" unless $depth-- >= 0;
  }
  s/\$\(mms\$source\)/\&\</i;
  s/\$\(mms\$target\)/\&\@/i;
  if (!($prefix && $do_trim)) {
    $out = ($prefix ? "#> " : "") . $_;
    print OUTFIL "$out";
  }
}

close INFIL;
close OUTFIL;
