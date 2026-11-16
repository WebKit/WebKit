# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


package Support::Files;

use 5.10.1;
use strict;
use warnings;

use File::Find;

our @additional_files = ();

our @files = glob('*');
find(sub { push(@files, $File::Find::name) if $_ =~ /\.pm$/;}, qw(Bugzilla docs));
push(@files, 'extensions/create.pl', 'docs/makedocs.pl');

our @extensions =
    grep { $_ ne 'extensions/create.pl' && ! -e "$_/disabled" }
    glob('extensions/*');

foreach my $extension (@extensions) {
    find(sub { push(@files, $File::Find::name) if $_ =~ /\.pm$/;}, $extension);
}

our @test_files = glob('t/*.t');

sub isTestingFile {
    my ($file) = @_;
    my $exclude;

    if ($file =~ /\.cgi$|\.pl$|\.pm$/) {
        return 1;
    }
    my $additional;
    foreach $additional (@additional_files) {
        if ($file eq $additional) { return 1; }
    }
    return undef;
}

our (@testitems, @module_files);

foreach my $currentfile (@files) {
    if (isTestingFile($currentfile)) {
        push(@testitems, $currentfile);
    }
    push(@module_files, $currentfile) if $currentfile =~ /\.pm$/;
}


1;
