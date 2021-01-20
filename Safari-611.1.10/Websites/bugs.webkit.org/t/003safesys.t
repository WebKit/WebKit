# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


#################
#Bugzilla Test 3#
###Safesystem####

use 5.10.1;
use strict;
use warnings;

use lib 't';

use Support::Files;

use Test::More tests => scalar(@Support::Files::testitems);

# Capture the TESTOUT from Test::More or Test::Builder for printing errors.
# This will handle verbosity for us automatically.
my $fh;
{
    no warnings qw(unopened);  # Don't complain about non-existent filehandles
    if (-e \*Test::More::TESTOUT) {
        $fh = \*Test::More::TESTOUT;
    } elsif (-e \*Test::Builder::TESTOUT) {
        $fh = \*Test::Builder::TESTOUT;
    } else {
        $fh = \*STDOUT;
    }
}

my @testitems = @Support::Files::testitems; 
my $perlapp = "\"$^X\"";

foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries

    open(my $fh2, '<', $file);
    my $bang = <$fh2>;
    close $fh2;

    my $T = "";
    if ($bang =~ m/#!\S*perl\s+-.*T/) {
        $T = "T";
    }
    my $command = "$perlapp -c$T -It -MSupport::Systemexec $file 2>&1";
    my $loginfo=`$command`;
    if ($loginfo =~ /arguments for Support::Systemexec::(system|exec)/im) {
        ok(0,"$file DOES NOT use proper system or exec calls");
        print $fh $loginfo;
    } else {
        ok(1,"$file uses proper system and exec calls");
    }
}

exit 0;
