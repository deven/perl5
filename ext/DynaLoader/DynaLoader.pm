package DynaLoader;

# Read ext/DynaLoader/README and DynaLoader.doc for
# detailed information.
#
# Tim Bunce, August 1994

require Config;

# enable messages from DynaLoader perl code
$dl_debug = $ENV{'PERL_DL_DEBUG'} || 0;


$dl_so = $Config::Config{'so'}; # suffix for shared objects

@dl_require_symbols = ();       # names of symbols we need
@dl_resolve_using   = ();       # names of files to link with
@dl_library_path    = ();       # path to look for files

# Initialise @dl_library_path with the 'standard' library path
# for this platform as determined by Configure
push(@dl_library_path, split(' ',$Config::Config{'libpth'}));

# Add to @dl_library_path any extra directories we can gather from
# environment variables. So far LD_LIBRARY_PATH is the only known
# variable used for this purpose. Others may be added later.
push(@dl_library_path, split(/:/, $ENV{'LD_LIBRARY_PATH'}))
    if $ENV{'LD_LIBRARY_PATH'};


# No prizes for guessing why we don't say: 'bootstrap DynaLoader;' here.
boot_DynaLoader;

# Give the C code a chance to initialise itself incase it needs to.
# Ideally this should happen within boot_DynaLoader and not require
# an XSUB function but xsubpp does not provide any mechanism to
# add code to the boot_MODULE function.
&dl_private_init();


print STDERR "DynaLoader.pm loaded (@dl_library_path)\n"
    if ($dl_debug > 1);

1; # End of main code


# We can't use @ISA=(Autoloader) because many modules will say:
#    package XYZ; @ISA=(...,AutoLoader,DynaLoader); bootstrap XYZ;
# which means that the AutoLoader is called for XYZ::bootstrap.
# So we must define functions to call the AutoLoader directly.
# We get "Subroutine dl_findfile redefined" warnings with -w :-(

sub dl_findfile{
    require AutoLoader;
    $AutoLoader::AUTOLOAD = 'DynaLoader::dl_findfile';
    goto &AutoLoader::AUTOLOAD;
}


# Given the need for a bootstrap stub to call the AutoLoader it
# does not seem worthwhile having a stub for bootstrap so we
# always load it directly:

sub bootstrap{
    # use local vars to enable $module.bs script to edit values
    local($module) = @_;
    local($dir, $file);

    die "Usage: DynaLoader::bootstrap(module)"
	unless ($module);

    die "Can't load module $module, DynaLoader not linked into this perl"
	unless defined(dl_load_file);

    print STDERR "DynaLoader::bootstrap($module)\n" if ($dl_debug);

    foreach (@INC){
        $dir = "$_/auto/$module";
        next unless -d $dir; # skip over uninteresting directories
	# check for common case to avoid autoload of dl_findfile
	$file = "$dir/$module.$dl_so";
	last if -f $file;
        last if ($file = dl_findfile($dir, $module));
    }
    die "Can't find loadable object for module $module in \@INC"
        unless $file;

    @dl_require_symbols = ("boot_$module");

    if (-f "$dir/$module.bs"){
        local($osname, $dlsrc) = @Config::Config{'osname','dlsrc'};
        print STDERR "$dir/$module.bs($osname, $dlsrc)\n" if $dl_debug;
        do "$dir/$module.bs";
        warn $@ if $@;
    }

    my $libref = DynaLoader::dl_load_file($file) or
        die "Can't load '$file' for module $module: $dl_error\n";

    my(@unresolved)= dl_undefined_symbols();
    warn "Undefined symbols present after loading $file: @unresolved\n"
        if (@unresolved);

    my($boot_symbol_ref) = dl_find_symbol($libref, "boot_$module") or
        die "Can't find 'boot_$module' symbol in $file\n";

    dl_install_xsub("${module}::bootstrap", $boot_symbol_ref, $file);
    &{"${module}::bootstrap"};
}


# Let autosplit and the autoloader deal with these functions:
__END__


sub dl_findfile{
    my (@args) = @_;
    my (@dirs,  $dir);   # which directories to search
    my (@found);         # full paths to real files we have found

    print STDERR "dl_findfile(@args)\n" if $dl_debug;
    # accumulate directories but process files as they appear
    arg: foreach(@args){
        #  Special fast case: full filepath requires no search
        if (m:/: && -f $_){    push(@found,$_); next; }

        #  Deal with directories first
        if (m:^-L:){   s/-L//; push(@dirs, $_); next; }
        if (m:/:  ){           push(@dirs, $_); next; }

        #  Only files should get this far...
        my(@names, $name);    # what filenames to look for
        if (m:-l: ){          # convert -lname to appropriate library name
            s/-l//;
            push(@names,"$_.$dl_so");
        }else{                # Umm, a bare name. Try various alternatives:
            # these should be ordered with the most likely first
            push(@names,"$_.$dl_so")     unless m/\.$dl_so$/o;
            push(@names,"lib$_.$dl_so");
            push(@names,"$_.o")          unless m/\.(o|$dl_so)$/o;
            push(@names,"$_.a")          unless m/\.a$/;
            push(@names, $_);
        }
        foreach $dir (@dirs, @dl_library_path){
            next unless -d $dir;
            foreach $name (@names){
                print STDERR " checking $dir/$name\n" if $dl_debug;
                if (-f "$dir/$name"){
                    push(@found, "$dir/$name");
                    next arg; # no need to look any further
                }
            }
        }
    }
    if ($dl_debug){
        foreach(@dirs){
            print STDERR " invalid directory: $_\n" unless -d $_;
        }
        print STDERR "dl_findfile found: @found\n" if $dl_debug;
    }
    return $found[0] unless wantarray;
    @found;
}

