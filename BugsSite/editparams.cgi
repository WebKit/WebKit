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
use lib ".";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT :admin);
use Bugzilla::User;

require "CGI.pl";

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

UserInGroup("tweakparams")
  || ThrowUserError("auth_failure", {group  => "tweakparams",
                                     action => "modify",
                                     object => "parameters"});

PutHeader("Edit parameters");

print "This lets you edit the basic operating parameters of bugzilla.\n";
print "Be careful!\n";
print "<p>\n";
print "Any item you check Reset on will get reset to its default value.\n";

print "<form method=post action=doeditparams.cgi><table>\n";

my $rowbreak = "<tr><td colspan=2><hr></td></tr>";
print $rowbreak;

foreach my $i (GetParamList()) {
    my $name = $i->{'name'};
    my $value = Param($name);
    print "<tr><th align=right valign=top>$name:</th><td>$i->{'desc'}</td></tr>\n";
    print "<tr><td valign=top><input type=checkbox name=reset-$name>Reset</td><td>\n";
    SWITCH: for ($i->{'type'}) {
        /^t$/ && do {
            print "<input size=80 name=$name value=\"" .
                value_quote($value) . "\">\n";
            last SWITCH;
        };
        /^l$/ && do {
            print "<textarea wrap=hard name=$name rows=10 cols=80>" .
                value_quote($value) . "</textarea>\n";
            last SWITCH;
        };
        /^b$/ && do {
            my $on;
            my $off;
            if ($value) {
                $on = "checked";
                $off = "";
            } else {
                $on = "";
                $off = "checked";
            }
            print "<input type=radio name=$name value=1 $on>On\n";
            print "<input type=radio name=$name value=0 $off>Off\n";
            last SWITCH;
        };
        /^m$/ && do {
            my @choices = @{$i->{'choices'}};
            ## showing 5 options seems like a nice round number; this should
            ## probably be configurable; if you care, file a bug ;-)
            my $boxSize = scalar(@choices) < 5 ? scalar(@choices) : 5;

            print "<select multiple size=\"$boxSize\" name=\"$name\">\n";

            foreach my $item (@choices) {
                my $selected = "";

                if (lsearch($value, $item) >= 0) {
                    $selected = "selected";
                }

                print "<option $selected value=\"" . html_quote($item) . "\">" .
                 html_quote($item) . "</option>\n";
            }

            print "</select>\n";
            last SWITCH;
        };
        /^s$/ && do {
            print "<select name=\"$name\">\n";
            my @choices = @{$i->{'choices'}};

            foreach my $item (@choices) {
                my $selected = "";

                if ($value eq $item) {
                    $selected = "selected";
                }

                print "<option $selected value=\"" . html_quote($item) . "\">" .
                  html_quote($item) . "</option>\n";

            }
            print "</select>\n";
            last SWITCH;
        };
        # DEFAULT
        print "<font color=red><blink>Unknown param type $i->{'type'}!!!</blink></font>\n";
    }
    print "</td></tr>\n";
    print $rowbreak;
}

print "<tr><th align=right valign=top>version:</th><td>
What version of Bugzilla this is. This can't be modified.
<tr><td></td><td>" . $Bugzilla::Config::VERSION . "</td></tr>";

print "</table>\n";

print "<input type=reset value=\"Reset form\"><br>\n";
print "<input type=submit value=\"Submit changes\">\n";

print "</form>\n";

print "<p><a href=query.cgi>Skip all this, and go back to the query page</a>\n";
PutFooter();
