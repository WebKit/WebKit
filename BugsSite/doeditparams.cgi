#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 J. Paul Reed <preed@sigkill.com>

use strict;

use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT :admin $datadir);
use Bugzilla::User;

require "CGI.pl";

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

print $cgi->header();

UserInGroup("tweakparams")
  || ThrowUserError("auth_failure", {group  => "tweakparams",
                                     action => "modify",
                                     object => "parameters"});

PutHeader("Saving new parameters");

my $howto = "";

foreach my $i (GetParamList()) {
    my $name = $i->{'name'};
    my $value = $cgi->param($name);
    if (defined $cgi->param("reset-$name")) {
        $value = $i->{'default'};
    } else {
        if ($i->{'type'} eq 'm') {
            # This simplifies the code below
            $value = [ $cgi->param($name) ];
        } else {
            # Get rid of windows/mac-style line endings.
            $value =~ s/\r\n?/\n/g;

            # assume single linefeed is an empty string
            $value =~ s/^\n$//;
        }
    }
    my $changed;
    if ($i->{'type'} eq 'm') {
        my @old = sort @{Param($name)};
        my @new = sort @$value;
        if (scalar(@old) != scalar(@new)) {
            $changed = 1;
        } else {
            $changed = 0; # Assume not changed...
            for (my $cnt = 0; $cnt < scalar(@old); ++$cnt) {
                if ($old[$cnt] ne $new[$cnt]) {
                    # entry is different, therefore changed
                    $changed = 1;
                    last;
                }
            }
        }
    } else {
        $changed = ($value eq Param($name) ? 0 : 1);
    }
    if ($changed) {
        if (exists $i->{'checker'}) {
            my $ok = $i->{'checker'}->($value, $i);
            if ($ok ne "") {
                print "New value for " . html_quote($name) .
                  " is invalid: $ok<p>\n";
                print "Please hit <b>Back</b> and try again.\n";
                PutFooter();
                exit;
            }
        }
        print "Changed " . html_quote($name) . ".<br>\n";
        SetParam($name, $value);
        if (($name eq "shutdownhtml") && ($value ne "")) {
            # The system is down, inform the user how to restore it
            $howto = "<p>Bugzilla has now been shut down, to re-enable ".
                    "the system, please return to ".
                    "<a href=\"editparams.cgi\">editparams.cgi</a>.</p>";
        }
    }
}


WriteParams();

unlink "$datadir/versioncache";

print "<p>OK, done.</p>\n";
print $howto;
print "<a href=\"editparams.cgi\">Edit the params some more.</a><p>\n";
print "<a href=\"query.cgi\">Go back to the query page.</a>\n";
    
PutFooter();
