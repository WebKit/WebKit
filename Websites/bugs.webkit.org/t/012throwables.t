# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.



##################
#Bugzilla Test 12#
######Errors######

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib t);

use Bugzilla::Constants;
use Bugzilla::WebService::Constants;

use File::Spec;
use Support::Files;
use Support::Templates;
use Test::More;

my %Errors = ();

# Just a workaround for template errors handling. Define it as used.
push @{$Errors{code}{template_error}{used_in}{'Bugzilla/Error.pm'}}, 0;

# Define files to test. Each file would have a list of error messages, if any.
my %test_templates = ();
my %test_modules = ();

# Find all modules
foreach my $module (@Support::Files::testitems) {
    $test_modules{$module} = ();
}

# Find all error templates
# Process all files since otherwise handling template hooks would became too
# hairy. But let us do it only once.

foreach my $include_path (@include_paths) {
    foreach my $path (@{$actual_files{$include_path}}) {
        my $file = File::Spec->catfile($include_path, $path);
        $file =~ s/\s.*$//; # nuke everything after the first space
        $file =~ s|\\|/|g if ON_WINDOWS;  # convert \ to / in path if on windows
        $test_templates{$file} = () 
            if $file =~ m#global/(code|user)-error\.html\.tmpl#;

        # Make sure the extension is not disabled
        if ($file =~ m#^(extensions/[^/]+/)#) {
            $test_templates{$file} = ()
                if ! -e "${1}disabled"
                && $file =~ m#global/(code|user)-error-errors\.html\.tmpl#;
        }
    }
}

# Count the tests. The +1 is for checking the WS_ERROR_CODE errors.
my $tests = (scalar keys %test_modules) + (scalar keys %test_templates) + 1;
exit 0 if !$tests;

# Set requested tests counter.
plan tests => $tests;

# Collect all errors defined in templates
foreach my $file (keys %test_templates) {
    $file =~ m|template/([^/]+).*/global/([^/]+)-error(?:-errors)?\.html\.tmpl|;
    my $lang = $1;
    my $errtype = $2;

    if (! open (TMPL, $file)) {
        Register(\%test_templates, $file, "could not open file --WARNING");
        next;
    }
    
    my $lineno=0;
    while (my $line = <TMPL>) {
        $lineno++;
        if ($line =~ /\[%\s[A-Z]+\s*error\s*==\s*"(.+)"\s*%\]/) {
            my $errtag = $1;
            if ($errtag =~ /\s/) {
                Register(\%test_templates, $file, 
                "has an error definition \"$errtag\" at line $lineno with "
                . "space(s) embedded --ERROR");
            }
            else {
                push @{$Errors{$errtype}{$errtag}{defined_in}{$lang}{$file}}, $lineno;
            }
        }
    }
    close(TMPL);
}

# Collect all used errors from cgi/pm files
foreach my $file (keys %test_modules) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    if (! open (TMPL, $file)) {
        Register(\%test_modules, $file, "could not open file --WARNING");
        next;
    }

    my $lineno = 0;
    while (my $line = <TMPL>) {
        last if $line =~ /^__END__/; # skip the POD (at least in
                                        # Bugzilla/Error.pm)
        $lineno++;
        if ($line =~
/^[^#]*\b(Throw(Code|User)Error|(user_)?error\s+=>)\s*\(?\s*["'](.*?)['"]/) {
            my $errtype;
            # If it's a normal ThrowCode/UserError
            if ($2) {
                $errtype = lc($2);
            }
            # If it's an AUTH_ERROR tag
            else {
                $errtype = $3 ? 'user' : 'code';
            }
            my $errtag = $4;
            push @{$Errors{$errtype}{$errtag}{used_in}{$file}}, $lineno;
        }
    }
    
    close(TMPL);
}

# Now let us start the checks

foreach my $errtype (keys %Errors) {
    foreach my $errtag (keys %{$Errors{$errtype}}) {
        # Check for undefined tags
        if (!defined $Errors{$errtype}{$errtag}{defined_in}) {
            UsedIn($errtype, $errtag, "any");
        }
        else {
            # Check for all languages!!!
            my @langs = ();
            foreach my $lang (@languages) {
                if (!defined $Errors{$errtype}{$errtag}{defined_in}{$lang}) {
                    push @langs, $lang;
                }
            }
            if (scalar @langs) {
                UsedIn($errtype, $errtag, join(', ',@langs));
            }
            
            # Now check for tag usage in all DEFINED languages
            foreach my $lang (keys %{$Errors{$errtype}{$errtag}{defined_in}}) {
                if (!defined $Errors{$errtype}{$errtag}{used_in}) {
                    DefinedIn($errtype, $errtag, $lang);
                }
            }
        }
    }
}

# And make sure that everything defined in WS_ERROR_CODE
# is actually a valid error.
foreach my $err_name (keys %{WS_ERROR_CODE()}) {
    if (!defined $Errors{'code'}{$err_name} 
        && !defined $Errors{'user'}{$err_name})
    {
        Register(\%test_modules, 'WS_ERROR_CODE',
            "Error tag '$err_name' is used in WS_ERROR_CODE in"
            . " Bugzilla/WebService/Constants.pm"
            . " but not defined in any template, and not used in any code.");
    }
}

# Now report modules results
foreach my $file (sort keys %test_modules) {
    Report($file, @{$test_modules{$file}});
}

# And report WS_ERROR_CODE results
Report('WS_ERROR_CODE', @{$test_modules{'WS_ERROR_CODE'}});

# Now report templates results
foreach my $file (sort keys %test_templates) {
    Report($file, @{$test_templates{$file}});
}

sub Register {
    my ($hash, $file, $message, $warning) = @_;
    # If set to 1, $warning will avoid the test to fail.
    $warning ||= 0;
    push(@{$hash->{$file}}, {'message' => $message, 'warning' => $warning});
}

sub Report {
    my ($file, @errors) = @_;
    if (scalar @errors) {
        # Do we only have warnings to report or also real errors?
        my @real_errors = grep {$_->{'warning'} == 0} @errors;
        # Extract error messages.
        @errors = map {$_->{'message'}} @errors;
        if (scalar(@real_errors)) {
            ok(0, "$file has ". scalar(@errors) ." error(s):\n" . join("\n", @errors));
        }
        else {
            ok(1, "--WARNING $file has " . scalar(@errors) .
                  " unused error tag(s):\n" . join("\n", @errors));
        }
    }
    else {
        # This is used for both code and template files, so let's use
        # file-independent phrase
        ok(1, "$file uses error tags correctly");
    }
}

sub UsedIn {
    my ($errtype, $errtag, $lang) = @_;
    $lang = $lang || "any";
    foreach my $file (keys %{$Errors{$errtype}{$errtag}{used_in}}) {
        Register(\%test_modules, $file, 
            "$errtype error tag '$errtag' is used at line(s) (" 
            . join (',', @{$Errors{$errtype}{$errtag}{used_in}{$file}}) 
            . ") but not defined for language(s): $lang");
    }
}
sub DefinedIn {
    my ($errtype, $errtag, $lang) = @_;
    foreach my $file (keys %{$Errors{$errtype}{$errtag}{defined_in}{$lang}}) {
        Register(\%test_templates, $file, 
            "$errtype error tag '$errtag' is defined at line(s) ("
            . join (',', @{$Errors{$errtype}{$errtag}{defined_in}{$lang}{$file}}) 
            . ") but is not used anywhere", 1);
    }
}

exit 0;
