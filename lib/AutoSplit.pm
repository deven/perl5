package AutoSplit;

require 5.000;
require Exporter;

use Config;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(&autosplit &autosplit_lib_modules);
@EXPORT_OK = qw($Verbose $Keep);

# for portability warn about names longer than $maxlen
$Maxlen  = 8;	# 8 for dos, 11 (14-".al") for SYSVR3
$Verbose = 1;	# 0=none, 1=minimal, 2=list .al files
$Keep    = 0;

$maxflen = 255;
$maxflen = 14 if $Config{'d_flexfnam'} ne 'define';


sub autosplit{
    my($file, $autodir) = @_;
    autosplit_file($file, $autodir, $Keep, 1, 0);
}



# This function is used during perl building/installation
# ./miniperl -e 'use AutoSplit; autosplit_modules(@ARGV)' ...

sub autosplit_lib_modules{
    my(@modules) = @_; # list of Module names
    foreach(@modules){
	s#::#/#g;	# incase specified as ABC::XYZ
	s#^lib/##; # incase specified as lib/*.pm
	autosplit_file("lib/$_", "lib/auto", $Keep, 1, 1);
    }
    0;
}


# private functions

sub autosplit_file{
    my($filename, $autodir, $keep, $check_for_autoloader, $check_mod_time) = @_;
    my(@names);

    # where to write output files
    $autodir = "lib/auto" unless $autodir;
    die "autosplit directory $autodir does not exist" unless -d $autodir;

    # allow just a package name to be used
    $filename .= ".pm" unless ($filename =~ m/\.pm$/);

    open(IN, "<$filename") || die "AutoSplit: Can't open $filename: $!\n";
    my($pm_mod_time) = (stat($filename))[9];
    my($autoloader_seen) = 0;
    while (<IN>) {
	# record last package name seen
	$package = $1 if (m/^\s*package\s+([\w:]+)\s*;/);
	++$autoloader_seen if m/^\s*use\s+AutoLoader\b/;
	++$autoloader_seen if m/\bISA\s*=.*\bAutoLoader\b/;
	last if /^__END__/;
    }
    return 0 if ($check_for_autoloader && !$autoloader_seen);
    $_ or die "Can't find __END__ in $filename\n";

    $package or die "Can't find 'package Name;' in $filename\n";

    my($modpname) = $package; $modpname =~ s#::#/#g;
    my($al_ts_file) = "$autodir/$modpname/autosplit.ts";

    die "Package $package does not match filename $filename"
	    unless ($filename =~ m/$modpname.pm$/);

    if ($check_mod_time){
	my($al_ts_time) = (stat("$al_ts_file"))[9] || 1;
	if ($al_ts_time >= $pm_mod_time){
	    print "AutoSplit skipped ($al_ts_file newer that $filename)\n"
		if ($Verbose >= 2);
	    return undef;	# one undef, not a list
	}
    }

    my($from) = ($Verbose>=2) ? "$filename => " : "";
    print "AutoSpliting $package ($from$autodir/$modpname)\n"
	if $Verbose;

    unless (-d "$autodir/$modpname"){
	local($", @p)="/";
	foreach(split(/\//,"$autodir/$modpname")){
	    push(@p, $_);
	    next if -d "@p";
	    mkdir("@p",0777) or die "AutoSplit unable to mkdir @p: $!";
	}
    }

    # We must try to deal with some SVR3 systems with a limit of 14
    # characters for file names. Sadly we *cannot* simply truncate all
    # file names to 14 characters on these systems because we *must*
    # create filenames which exactly match the names used by AutoLoader.pm.
    # This is a problem because some systems silently truncate the file
    # names while others treat long file names as an error.

    open(OUT,">/dev/null"); # avoid 'not opened' warning
    while (<IN>) {
	if (/^sub ([\w:]+)/) {
	    print OUT "1;\n";
	    my($lname, $sname) = ($1, substr($1,0,$maxflen-3));
	    my($lpath) = "$autodir/$modpname/$lname.al";
	    my($spath) = "$autodir/$modpname/$sname.al";
	    unless(open(OUT, ">$lpath")){
		open(OUT, ">$spath") or die "Can't create $spath: $!\n";
		push(@names, $sname);
		print "  writing $spath (with truncated name)\n"
			if ($Verbose>=1);
	    }else{
		push(@names, $lname);
		print "  writing $lpath\n" if ($Verbose>=2);
	    }
	    print OUT "# NOTE: Derived from $filename.  ",
			"Changes made here will be lost.\n";
	    print OUT "package $package;\n\n";
	}
	print OUT $_;
    }
    print OUT "1;\n";
    close(OUT);
    close(IN);

    if (!$keep){  # don't keep any obsolete *.al files in the directory
	my(%names);
	@names{@names} = @names;
	opendir(OUTDIR,"$autodir/$modpname");
	foreach(sort readdir(OUTDIR)){
	    next unless /\.al$/;
	    my($subname) = m/(.*)\.al$/;
	    next if $names{substr($subname,0,$maxflen-3)};
	    my($file) = "$autodir/$modpname/$_";
	    print "  deleting $file\n" if ($Verbose>=2);
	    unlink $file or carp "Unable to delete $file: $!";
	}
	closedir(OUTDIR);
    }

    open(TS,">$al_ts_file") or
	carp "AutoSplit: unable to create timestamp file ($al_ts_file): $!";
    print TS "Timestamp marker created by AutoSplit for $filename";
    close(TS);

    check_unique($package, $Maxlen, 1, @names);

    @names;
}


sub check_unique{
    my($module, $maxlen, $warn, @names) = @_;
    my(%notuniq) = ();
    my(%shorts)  = ();
    my(@toolong) = grep(length > $maxlen, @names);

    foreach(@toolong){
	my($trunc) = substr($_,0,$maxlen);
	$notuniq{$trunc}=1 if $shorts{$trunc};
	$shorts{$trunc} = ($shorts{$trunc}) ? "$shorts{$trunc}, $_" : $_;
    }
    if (%notuniq && $warn){
	print "$module: some names are not unique when truncated to $maxlen characters:\n";
	foreach(keys %notuniq){
	    print " $shorts{$_} truncate to $_\n";
	}
    }
    %notuniq;
}

1;
__END__

# test functions so AutoSplit.pm can be applied to itself:
sub test1{ "test 1\n"; }
sub test2{ "test 2\n"; }
sub test3{ "test 3\n"; }
sub test4{ "test 4\n"; }


