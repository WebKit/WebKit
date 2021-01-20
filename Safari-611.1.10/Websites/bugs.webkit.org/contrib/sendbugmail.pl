#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::BugMail;
use Bugzilla::User;

my $dbh = Bugzilla->dbh;

sub usage {
    say STDERR "Usage: $0 bug_id user_email";
    exit;
}

if (($#ARGV < 1) || ($#ARGV > 2)) {
    usage();
}

# Get the arguments.
my $bugnum = $ARGV[0];
my $changer = $ARGV[1];

# Validate the bug number.
if (!($bugnum =~ /^(\d+)$/)) {
  say STDERR "Bug number \"$bugnum\" not numeric.";
  usage();
}

detaint_natural($bugnum);

my ($id) = $dbh->selectrow_array("SELECT bug_id FROM bugs WHERE bug_id = ?", 
                                 undef, $bugnum);

if (!$id) {
  say STDERR "Bug number $bugnum does not exist.";
  usage();
}

# Validate the changer address.
my $match = Bugzilla->params->{'emailregexp'};
if ($changer !~ /$match/) {
    say STDERR "Changer \"$changer\" doesn't match email regular expression.";
    usage();
}
my $changer_user = new Bugzilla::User({ name => $changer });
unless ($changer_user) {
    say STDERR "\"$changer\" is not a valid user.";
    usage();
}

# Send the email.
my $outputref = Bugzilla::BugMail::Send($bugnum, {'changer' => $changer_user });

# Report the results.
my $sent = scalar(@{$outputref->{sent}});

if ($sent) {
    say "email sent to $sent recipients:";
} else {
    say "No email sent.";
}

foreach my $sent (@{$outputref->{sent}}) {
  say "  $sent";
}

# This document is copyright (C) 2004 Perforce Software, Inc.  All rights
# reserved.
# 
# Redistribution and use of this document in any form, with or without
# modification, is permitted provided that redistributions of this
# document retain the above copyright notice, this condition and the
# following disclaimer.
# 
# THIS DOCUMENT IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# DOCUMENT, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
