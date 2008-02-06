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
# Contributor(s): Dawn Endico    <endico@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>
#                 Dave Miller    <justdave@bugzilla.org>

use strict;

use lib qw(.);

require "CGI.pl";

use vars qw($template $userid);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Bug;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::BugMail;

my $cgi = Bugzilla->cgi;

unless ( Param("move-enabled") ) {
  print $cgi->header();
  PutHeader("Move Bugs");
  print "\n<P>Sorry. Bug moving is not enabled here. ";
  print "If you need to move a bug, contact " . Param("maintainer");
  PutFooter();
  exit;
}

my $user = Bugzilla->login(LOGIN_REQUIRED);

if (!defined $cgi->param('buglist')) {
  print $cgi->header();
  PutHeader("Move Bugs");
  print "Move bugs either from the bug display page or perform a ";
  print "<A HREF=\"query.cgi\">query</A> and change several bugs at once.\n";
  print "If you don't see the move button, then you either aren't ";
  print "logged in or aren't permitted to.";
  PutFooter();
  exit;
}

unless ($user->is_mover) {
  print $cgi->header();
  PutHeader("Move Bugs");
  print "<P>You do not have permission to move bugs<P>\n";
  PutFooter();
  exit;
}

my @bugs;
my $exporterid = $user->id;

print $cgi->header();
PutHeader("Move Bugs");

foreach my $id (split(/:/, scalar($cgi->param('buglist')))) {
  my $bug = new Bugzilla::Bug($id, $exporterid);
  push @bugs, $bug;
  if (!$bug->error) {
    my $fieldid = GetFieldID("bug_status");
    my $cur_status= $bug->bug_status;
    SendSQL("INSERT INTO bugs_activity " .
            "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
            "($id,$exporterid,now(),$fieldid,'$cur_status','RESOLVED')");
    $fieldid = GetFieldID("resolution");
    my $cur_res= $bug->resolution;
    SendSQL("INSERT INTO bugs_activity " .
            "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
            "($id,$exporterid,now(),$fieldid,'$cur_res','MOVED')");

    SendSQL("UPDATE bugs SET bug_status =\"RESOLVED\",
                             resolution =\"MOVED\",
                             delta_ts = NOW()
              WHERE bug_id=\"$id\"");

    my $comment = "";
    if (defined $cgi->param('comment') && $cgi->param('comment') !~ /^\s*$/) {
        $comment .= $cgi->param('comment') . "\n\n";
    }
    $comment .= "Bug moved to " . Param("move-to-url") . ".\n\n";
    $comment .= "If the move succeeded, " . $user->login . " will receive a mail\n";
    $comment .= "containing the number of the new bug in the other database.\n";
    $comment .= "If all went well,  please mark this bug verified, and paste\n";
    $comment .= "in a link to the new bug. Otherwise, reopen this bug.\n";
    SendSQL("INSERT INTO longdescs (bug_id, who, bug_when, thetext) VALUES " .
        "($id,  $exporterid, now(), " . SqlQuote($comment) . ")");

    print "<P>Bug $id moved to " . Param("move-to-url") . ".<BR>\n";
    Bugzilla::BugMail::Send($id, { 'changer' => $user->login });
  }
}

my $buglist = $cgi->param('buglist');
$buglist =~ s/:/,/g;
my $host = Param("urlbase");
$host =~ s#http://([^/]+)/.*#$1#;
my $to = Param("move-to-address");
$to =~ s/@/\@/;
my $msg = "To: $to\n";
my $from = Param("moved-from-address");
$from =~ s/@/\@/;
$msg .= "From: Bugzilla <" . $from . ">\n";
$msg .= "Subject: Moving bug(s) $buglist\n\n";

my @fieldlist = (Bugzilla::Bug::fields(), 'group', 'long_desc', 'attachment');
my %displayfields;
foreach (@fieldlist) {
    $displayfields{$_} = 1;
}

$template->process("bug/show.xml.tmpl", { bugs => \@bugs,
                                          displayfields => \%displayfields,
                                        }, \$msg)
  || ThrowTemplateError($template->error());

$msg .= "\n";

Bugzilla::BugMail::MessageToMTA($msg);
PutFooter();
