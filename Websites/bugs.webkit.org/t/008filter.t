# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

#################
#Bugzilla Test 8#
#####filter######

# This test scans all our templates for every directive. Having eliminated
# those which cannot possibly cause XSS problems, it then checks the rest
# against the safe list stored in the filterexceptions.pl file. 

# Sample exploit code: '>"><script>alert('Oh dear...')</script>

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib t);

use Bugzilla::Constants;
use Support::Templates;
use File::Spec;
use Test::More tests => $Support::Templates::num_actual_files;
use Cwd;

# Undefine the record separator so we can read in whole files at once
my $oldrecsep = $/;
my $topdir = cwd;
$/ = undef;
our %safe;

foreach my $path (@Support::Templates::include_paths) {
    $path =~ s|\\|/|g if ON_WINDOWS;  # convert \ to / in path if on windows
    $path =~ m|template/([^/]+)/([^/]+)|;
    my $lang = $1;
    my $flavor = $2;

    chdir $topdir; # absolute path
    my @testitems = Support::Templates::find_actual_files($path);
    chdir $topdir; # absolute path
    
    next unless @testitems;
    
    # Some people require this, others don't. No-one knows why.
    chdir $path; # relative path
    
    # We load a %safe list of acceptable exceptions.
    if (-r "filterexceptions.pl") {
        do "filterexceptions.pl";
        if (ON_WINDOWS) {
          # filterexceptions.pl uses / separated paths, while 
          # find_actual_files returns \ separated ones on Windows.
          # Here, we convert the filter exception hash to use \.
          foreach my $file (keys %safe) {
            my $orig_file = $file;
            $file =~ s|/|\\|g;
            if ($file ne $orig_file) {
              $safe{$file} = $safe{$orig_file};
              delete $safe{$orig_file};
            }
          }
        }
    }
    
    # We preprocess the %safe hash of lists into a hash of hashes. This allows
    # us to flag which members were not found, and report that as a warning, 
    # thereby keeping the lists clean.
    foreach my $file (keys %safe) {
        if (ref $safe{$file} eq 'ARRAY') {
            my $list = $safe{$file};
            $safe{$file} = {};
            foreach my $directive (@$list) {
                $safe{$file}{$directive} = 0;    
            }
        }
    }

    foreach my $file (@testitems) {
        # There are some files we don't check, because there is no need to
        # filter their contents due to their content-type.
        if ($file =~ /\.(pm|txt|rst|png)\.tmpl$/) {
            ok(1, "($lang/$flavor) $file is filter-safe");
            next;
        }

        # Read the entire file into a string
        open (FILE, "<$file") || die "Can't open $file: $!\n";
        my $slurp = <FILE>;
        close (FILE);

        my @unfiltered;

        # /g means we execute this loop for every match
        # /s means we ignore linefeeds in the regexp matches
        while ($slurp =~ /\[%(?:-|\+|~|=)?(.*?)(?:-|\+|~|=)?%\]/gs) {
            my $directive = $1;

            my @lineno = ($` =~ m/\n/gs);
            my $lineno = scalar(@lineno) + 1;

            if (!directive_ok($file, $directive)) {

              # This intentionally makes no effort to eliminate duplicates; to do
              # so would merely make it more likely that the user would not 
              # escape all instances when attempting to correct an error.
              push(@unfiltered, "$lineno:$directive");
            }
        }  

        my $fullpath = File::Spec->catfile($path, $file);
        
        if (@unfiltered) {
            my $uflist = join("\n  ", @unfiltered);
            ok(0, "($lang/$flavor) $fullpath has unfiltered directives:\n  $uflist\n--ERROR");
        }
        else {
            # Find any members of the exclusion list which were not found
            my @notfound;
            foreach my $directive (keys %{$safe{$file}}) {
                push(@notfound, $directive) if ($safe{$file}{$directive} == 0);    
            }

            if (@notfound) {
                my $nflist = join("\n  ", @notfound);
                ok(0, "($lang/$flavor) $fullpath - filterexceptions.pl has extra members:\n  $nflist\n" . 
                                                                  "--WARNING");
            }
            else {
                # Don't use the full path here - it's too long and unwieldy.
                ok(1, "($lang/$flavor) $file is filter-safe");
            }
        }
    }
}

sub directive_ok {
    my ($file, $directive) = @_;

    # Comments
    return 1 if $directive =~ /^#/;        

    # Remove any leading/trailing whitespace.
    $directive =~ s/^\s*//;
    $directive =~ s/\s*$//;

    # Empty directives are ok; they are usually line break helpers
    return 1 if $directive eq '';

    # Make sure we're not looking for ./ in the $safe hash
    $file =~ s#^\./##;

    # Exclude those on the nofilter list
    if (defined($safe{$file}{$directive})) {
        $safe{$file}{$directive}++;
        return 1;
    };

    # Directives
    return 1 if $directive =~ /^(IF|END|UNLESS|FOREACH|PROCESS|INCLUDE|
                                 BLOCK|USE|ELSE|NEXT|LAST|DEFAULT|
                                 ELSIF|SET|SWITCH|CASE|WHILE|RETURN|STOP|
                                 TRY|CATCH|FINAL|THROW|CLEAR|MACRO|FILTER)/x;

    # ? :
    if ($directive =~ /.+\?(.+):(.+)/) {
        return 1 if directive_ok($file, $1) && directive_ok($file, $2);
    }

    # + - * /
    return 1 if $directive =~ /[+\-*\/]/;

    # Numbers
    return 1 if $directive =~ /^[0-9]+$/;

    # Simple assignments
    return 1 if $directive =~ /^[\w\.\$\{\}]+\s+=\s+/;

    # Conditional literals with either sort of quotes 
    # There must be no $ in the string for it to be a literal
    return 1 if $directive =~ /^(["'])[^\$]*[^\\]\1/;
    return 1 if $directive =~ /^(["'])\1/;

    # Special values always used for numbers
    return 1 if $directive =~ /^[ijkn]$/;
    return 1 if $directive =~ /^count$/;
    
    # Params
    return 1 if $directive =~ /^Param\(/;
    
    # Hooks
    return 1 if $directive =~ /^Hook.process\(/;

    # Other functions guaranteed to return OK output
    return 1 if $directive =~ /^(time2str|url)\(/;

    # Safe Template Toolkit virtual methods
    return 1 if $directive =~ /\.(length$|size$|push\(|unshift\(|delete\()/;

    # Special Template Toolkit loop variable
    return 1 if $directive =~ /^loop\.(index|count)$/;
    
    # Branding terms
    return 1 if $directive =~ /^terms\./;
            
    # Things which are already filtered
    # Note: If a single directive prints two things, and only one is 
    # filtered, we may not catch that case.
    return 1 if $directive =~ /FILTER\ (html|csv|js|base64|css_class_quote|ics|
                                        quoteUrls|time|uri|xml|lower|html_light|
                                        obsolete|inactive|closed|unitconvert|
                                        txt|html_linebreak|none)\b/x;

    return 0;
}

$/ = $oldrecsep;

exit 0;
