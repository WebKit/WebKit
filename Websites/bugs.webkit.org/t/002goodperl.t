# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


#################
#Bugzilla Test 2#
####GoodPerl#####

use 5.10.1;
use strict;
use warnings;

use lib 't';

use Support::Files;

use Test::More tests => (scalar(@Support::Files::testitems)
                         + scalar(@Support::Files::test_files)) * 6;

my @testitems = (@Support::Files::test_files, @Support::Files::testitems);
my @require_taint = qw(email_in.pl importxml.pl mod_perl.pl whine.pl);

foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    if (! open (FILE, $file)) {
        ok(0,"could not open $file --WARNING");
    }
    my $file_line1 = <FILE>;
    close (FILE);

    $file =~ m/.*\.(.*)/;
    my $ext = $1;

    if ($file_line1 !~ m/^#\!/) {
        ok(1,"$file does not have a shebang");
    } else {
        my $flags;
        if (!defined $ext || $ext eq "pl") {
            # standalone programs aren't taint checked yet
            if (grep { $file eq $_ } @require_taint) {
                $flags = 'T';
            }
            else {
                $flags = '';
            }
        } elsif ($ext eq "pm") {
            ok(0, "$file is a module, but has a shebang");
            next;
        } elsif ($ext eq "cgi") {
            # cgi files must be taint checked
            $flags = 'T';
        } else {
            ok(0, "$file has shebang but unknown extension");
            next;
        }

        if ($file_line1 =~ m#^\#\!/usr/bin/perl(?:\s-(\w+))?$#) {
            my $file_flags = $1 || '';
            if ($flags eq $file_flags) {
                ok(1, "$file uses standard perl location" . ($flags ? " and -$flags flag" : ""));
            }
            elsif ($flags) {
                ok(0, "$file is MISSING -$flags flag --WARNING");
            }
            else {
                ok(0, "$file has unexpected -$file_flags flag --WARNING");
            }
        } else {
            ok(0,"$file uses non-standard perl location");
        }
    }
}

foreach my $file (@testitems) {
    my $found_use_perl = 0;
    my $found_use_strict = 0;
    my $found_use_warnings = 0;

    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    if (! open (FILE, $file)) {
        ok(0,"could not open $file --WARNING");
        next;
    }
    while (my $file_line = <FILE>) {
        $found_use_perl = 1 if $file_line =~ m/^\s*use 5.10.1/;
        $found_use_strict = 1 if $file_line =~ m/^\s*use strict/;
        $found_use_warnings = 1 if $file_line =~ m/^\s*use warnings/;
        last if ($found_use_perl && $found_use_strict && $found_use_warnings);
    }
    close (FILE);
    if ($found_use_perl) {
        ok(1,"$file requires Perl 5.10.1");
    } else {
        ok(0,"$file DOES NOT require Perl 5.10.1 --WARNING");
    }

    if ($found_use_strict) {
        ok(1,"$file uses strict");
    } else {
        ok(0,"$file DOES NOT use strict --WARNING");
    }

    if ($found_use_warnings) {
        ok(1,"$file uses warnings");
    } else {
        ok(0,"$file DOES NOT use warnings --WARNING");
    }
}

# Check to see that all error messages use tags (for l10n reasons.)
foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    if (! open (FILE, $file)) {
        ok(0,"could not open $file --WARNING");
        next;
    }
    my $lineno = 0;
    my $error = 0;
    
    while (!$error && (my $file_line = <FILE>)) {
        $lineno++;
        if ($file_line =~ /Throw.*Error\("(.*?)"/) {
            if ($1 =~ /\s/) {
                ok(0,"$file has a Throw*Error call on line $lineno 
                      which doesn't use a tag --ERROR");
                $error = 1;       
            }
        }
    }
    
    ok(1,"$file uses Throw*Error calls correctly") if !$error;
    
    close(FILE);
}

# Forbird the { foo => $cgi->param() } syntax, for security reasons.
foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next unless $file; # skip null entries
    if (!open(FILE, $file)) {
        ok(0, "could not open $file --WARNING");
        next;
    }
    my $lineno = 0;
    my @unsafe_args;

    while (my $file_line = <FILE>) {
        $lineno++;
        $file_line =~ s/^\s*(.+)\s*$/$1/; # Remove leading and trailing whitespaces.
        if ($file_line =~ /^[^#]+=> \$cgi\->param/) {
            push(@unsafe_args, "$file_line on line $lineno");
        }
    }

    if (@unsafe_args) {
        ok(0, "$file incorrectly passes a CGI argument to a hash --ERROR\n" .
              join("\n", @unsafe_args));
    }
    else {
        ok(1, "$file has no vulnerable hash syntax");
    }

    close(FILE);
}

exit 0;
