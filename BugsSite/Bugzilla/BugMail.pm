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
# Contributor(s): Terry Weissman <terry@mozilla.org>,
#                 Bryce Nesbitt <bryce-mozilla@nextbus.com>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Alan Raetz <al_raetz@yahoo.com>
#                 Jacob Steenhagen <jake@actex.net>
#                 Matthew Tuck <matty@chariot.net.au>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 J. Paul Reed <preed@sigkill.com>
#                 Gervase Markham <gerv@gerv.net>

use strict;

package Bugzilla::BugMail;

use base qw(Exporter);
@Bugzilla::BugMail::EXPORT = qw(
    PerformSubsts
);

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::Util;
use Bugzilla::User;

use Mail::Mailer;
use Mail::Header;

# We need these strings for the X-Bugzilla-Reasons header
# Note: this hash uses "," rather than "=>" to avoid auto-quoting of the LHS.
my %rel_names = (REL_ASSIGNEE          , "AssignedTo", 
                 REL_REPORTER          , "Reporter",
                 REL_QA                , "QAcontact",
                 REL_CC                , "CC",
                 REL_VOTER             , "Voter");

# This code is really ugly. It was a commandline interface, then it was moved.
# This really needs to be cleaned at some point.

my %nomail;

my $sitespec = '@'.Param('urlbase');
$sitespec =~ s/:\/\//\./; # Make the protocol look like part of the domain
$sitespec =~ s/^([^:\/]+):(\d+)/$1/; # Remove a port number, to relocate
if ($2) {
    $sitespec = "-$2$sitespec"; # Put the port number back in, before the '@'
}

# I got sick of adding &:: to everything.
# However, 'Yuck!'
# I can't require, cause that pulls it in only once, so it won't then be
# in the global package, and these aren't modules, so I can't use globals.pl
# Remove this evilness once our stuff uses real packages.
sub AUTOLOAD {
    no strict 'refs';
    use vars qw($AUTOLOAD);
    my $subName = $AUTOLOAD;
    $subName =~ s/.*::/::/; # remove package name
    *$AUTOLOAD = \&$subName;
    goto &$AUTOLOAD;
}

# This is run when we load the package
if (open(NOMAIL, '<', "$datadir/nomail")) {
    while (<NOMAIL>) {
        $nomail{trim($_)} = 1;
    }
    close(NOMAIL);
}

sub FormatTriple {
    my ($a, $b, $c) = (@_);
    $^A = "";
    my $temp = formline << 'END', $a, $b, $c;
^>>>>>>>>>>>>>>>>>>|^<<<<<<<<<<<<<<<<<<<<<<<<<<<|^<<<<<<<<<<<<<<<<<<<<<<<<<<<~~
END
    ; # This semicolon appeases my emacs editor macros. :-)
    return $^A;
}
    
sub FormatDouble {
    my ($a, $b) = (@_);
    $a .= ":";
    $^A = "";
    my $temp = formline << 'END', $a, $b;
^>>>>>>>>>>>>>>>>>> ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<~~
END
    ; # This semicolon appeases my emacs editor macros. :-)
    return $^A;
}

# This is a bit of a hack, basically keeping the old system()
# cmd line interface. Should clean this up at some point.
#
# args: bug_id, and an optional hash ref which may have keys for:
# changer, owner, qa, reporter, cc
# Optional hash contains values of people which will be forced to those
# roles when the email is sent.
# All the names are email addresses, not userids
# values are scalars, except for cc, which is a list
# This hash usually comes from the "mailrecipients" var in a template call.
sub Send($;$) {
    my ($id, $forced) = (@_);

    # This only works in a sub. Probably something to do with the
    # require abuse we do.
    GetVersionTable();

    return ProcessOneBug($id, $forced);
}

sub ProcessOneBug($$) {
    my ($id, $forced) = (@_);

    my @headerlist;
    my %values;
    my %defmailhead;
    my %fielddescription;

    my $msg = "";

    my $dbh = Bugzilla->dbh;
     
    SendSQL("SELECT name, description, mailhead FROM fielddefs " .
            "ORDER BY sortkey");
    while (MoreSQLData()) {
        my ($field, $description, $mailhead) = (FetchSQLData());
        push(@headerlist, $field);
        $defmailhead{$field} = $mailhead;
        $fielddescription{$field} = $description;
    }
    SendSQL("SELECT " . join(',', @::log_columns) . ", lastdiffed, now() " .
            "FROM bugs WHERE bug_id = $id");
    my @row = FetchSQLData();
    foreach my $i (@::log_columns) {
        $values{$i} = shift(@row);
    }
    $values{product} = get_product_name($values{product_id});
    $values{component} = get_component_name($values{component_id});

    my ($start, $end) = (@row);

    # User IDs of people in various roles. More than one person can 'have' a 
    # role, if the person in that role has changed, or people are watching.
    my $reporter = $values{'reporter'};
    my @assignees = ($values{'assigned_to'});
    my @qa_contacts = ($values{'qa_contact'});

    my $cc_users = $dbh->selectall_arrayref(
           "SELECT cc.who, profiles.login_name
              FROM cc
        INNER JOIN profiles
                ON cc.who = profiles.userid
             WHERE bug_id = ?",
           undef, $id);

    my (@ccs, @cc_login_names);
    foreach my $cc_user (@$cc_users) {
        my ($user_id, $user_login) = @$cc_user;
        push (@ccs, $user_id);
        push (@cc_login_names, $user_login);
    }

    # Include the people passed in as being in particular roles.
    # This can include people who used to hold those roles.
    # At this point, we don't care if there are duplicates in these arrays.
    my $changer = $forced->{'changer'};
    if ($forced->{'owner'}) {
        push (@assignees, DBNameToIdAndCheck($forced->{'owner'}));
    }
    
    if ($forced->{'qacontact'}) {
        push (@qa_contacts, DBNameToIdAndCheck($forced->{'qacontact'}));
    }
    
    if ($forced->{'cc'}) {
        foreach my $cc (@{$forced->{'cc'}}) {
            push(@ccs, DBNameToIdAndCheck($cc));
        }
    }
    
    # Convert to names, for later display
    $values{'assigned_to'} = DBID_to_name($values{'assigned_to'});
    $values{'reporter'} = DBID_to_name($values{'reporter'});
    if ($values{'qa_contact'}) {
        $values{'qa_contact'} = DBID_to_name($values{'qa_contact'});
    }
    $values{'cc'} = join(', ', @cc_login_names);
    $values{'estimated_time'} = format_time_decimal($values{'estimated_time'});

    if ($values{'deadline'}) {
        $values{'deadline'} = time2str("%Y-%m-%d", str2time($values{'deadline'}));
    }

    my @dependslist;
    SendSQL("SELECT dependson FROM dependencies WHERE 
             blocked = $id ORDER BY dependson");
    while (MoreSQLData()) {
        push(@dependslist, FetchOneColumn());
    }
    $values{'dependson'} = join(",", @dependslist);

    my @blockedlist;
    SendSQL("SELECT blocked FROM dependencies WHERE 
             dependson = $id ORDER BY blocked");
    while (MoreSQLData()) {
        push(@blockedlist, FetchOneColumn());
    }
    $values{'blocked'} = join(",", @blockedlist);

    my @diffs;

    # If lastdiffed is NULL, then we don't limit the search on time.
    my $when_restriction = $start ? 
        " AND bug_when > '$start' AND bug_when <= '$end'" : '';
    SendSQL("SELECT profiles.login_name, fielddefs.description, " .
            "       bug_when, removed, added, attach_id, fielddefs.name " .
            "FROM bugs_activity, fielddefs, profiles " .
            "WHERE bug_id = $id " .
            "  AND fielddefs.fieldid = bugs_activity.fieldid " .
            "  AND profiles.userid = who " .
            $when_restriction .
            "ORDER BY bug_when"
            );

    while (MoreSQLData()) {
        my @row = FetchSQLData();
        push(@diffs, \@row);
    }

    my $difftext = "";
    my $diffheader = "";
    my @diffparts;
    my $lastwho = "";
    foreach my $ref (@diffs) {
        my ($who, $what, $when, $old, $new, $attachid, $fieldname) = (@$ref);
        my $diffpart = {};
        if ($who ne $lastwho) {
            $lastwho = $who;
            $diffheader = "\n$who" . Param('emailsuffix') . " changed:\n\n";
            $diffheader .= FormatTriple("What    ", "Removed", "Added");
            $diffheader .= ('-' x 76) . "\n";
        }
        $what =~ s/^(Attachment )?/Attachment #$attachid / if $attachid;
        if( $fieldname eq 'estimated_time' ||
            $fieldname eq 'remaining_time' ) {
            $old = format_time_decimal($old);
            $new = format_time_decimal($new);
        }
        if ($attachid) {
            SendSQL("SELECT isprivate FROM attachments 
                     WHERE attach_id = $attachid");
            $diffpart->{'isprivate'} = FetchOneColumn();
        }
        $difftext = FormatTriple($what, $old, $new);
        $diffpart->{'header'} = $diffheader;
        $diffpart->{'fieldname'} = $fieldname;
        $diffpart->{'text'} = $difftext;
        push(@diffparts, $diffpart);
    }

    my $deptext = "";

    SendSQL("SELECT bugs_activity.bug_id, bugs.short_desc, fielddefs.name, " .
            "       removed, added " .
            "FROM bugs_activity, bugs, dependencies, fielddefs ".
            "WHERE bugs_activity.bug_id = dependencies.dependson " .
            "  AND bugs.bug_id = bugs_activity.bug_id ".
            "  AND dependencies.blocked = $id " .
            "  AND fielddefs.fieldid = bugs_activity.fieldid" .
            "  AND (fielddefs.name = 'bug_status' " .
            "    OR fielddefs.name = 'resolution') " .
            $when_restriction .
            "ORDER BY bug_when, bug_id");
    
    my $thisdiff = "";
    my $lastbug = "";
    my $interestingchange = 0;
    my $depbug = 0;
    my @depbugs;
    while (MoreSQLData()) {
        my ($summary, $what, $old, $new);
        ($depbug, $summary, $what, $old, $new) = (FetchSQLData());
        if ($depbug ne $lastbug) {
            if ($interestingchange) {
                $deptext .= $thisdiff;
            }
            $lastbug = $depbug;
            my $urlbase = Param("urlbase");
            $thisdiff =
              "\nBug $id depends on bug $depbug, which changed state.\n\n" . 
              "Bug $depbug Summary: $summary\n" . 
              "${urlbase}show_bug.cgi?id=$depbug\n\n"; 
            $thisdiff .= FormatTriple("What    ", "Old Value", "New Value");
            $thisdiff .= ('-' x 76) . "\n";
            $interestingchange = 0;
        }
        $thisdiff .= FormatTriple($fielddescription{$what}, $old, $new);
        if ($what eq 'bug_status' && IsOpenedState($old) ne IsOpenedState($new)) {
            $interestingchange = 1;
        }
        
        push(@depbugs, $depbug);
    }
    
    if ($interestingchange) {
        $deptext .= $thisdiff;
    }

    $deptext = trim($deptext);

    if ($deptext) {
        my $diffpart = {};
        $diffpart->{'text'} = "\n" . trim("\n\n" . $deptext);
        push(@diffparts, $diffpart);
    }


    my ($newcomments, $anyprivate) = GetLongDescriptionAsText($id, $start, $end);

    ###########################################################################
    # Start of email filtering code
    ###########################################################################
    
    # A user_id => roles hash to keep track of people.
    my %recipients;
    
    # Now we work out all the people involved with this bug, and note all of
    # the relationships in a hash. The keys are userids, the values are an
    # array of role constants.
    
    # Voters
    my $voters = 
          $dbh->selectcol_arrayref("SELECT who FROM votes WHERE bug_id = $id");
        
    push(@{$recipients{$_}}, REL_VOTER) foreach (@$voters);

    # CCs
    push(@{$recipients{$_}}, REL_CC) foreach (@ccs);
    
    # Reporter (there's only ever one)
    push(@{$recipients{$reporter}}, REL_REPORTER);
    
    # QA Contact
    if (Param('useqacontact')) {
        foreach (@qa_contacts) {
            # QA Contact can be blank; ignore it if so.
            push(@{$recipients{$_}}, REL_QA) if $_;
        }
    }

    # Assignee
    push(@{$recipients{$_}}, REL_ASSIGNEE) foreach (@assignees);

    # The last relevant set of people are those who are being removed from 
    # their roles in this change. We get their names out of the diffs.
    foreach my $ref (@diffs) {
        my ($who, $what, $when, $old, $new) = (@$ref);
        if ($old) {
            # You can't stop being the reporter, and mail isn't sent if you
            # remove your vote.
            # Ignore people whose user account has been deleted or renamed.
            if ($what eq "CC") {
                foreach my $cc_user (split(/[\s,]+/, $old)) {
                    my $uid = login_to_id($cc_user);
                    push(@{$recipients{$uid}}, REL_CC) if $uid;
                }
            }
            elsif ($what eq "QAContact") {
                my $uid = login_to_id($old);
                push(@{$recipients{$uid}}, REL_QA) if $uid;
            }
            elsif ($what eq "AssignedTo") {
                my $uid = login_to_id($old);
                push(@{$recipients{$uid}}, REL_ASSIGNEE) if $uid;
            }
        }
    }
    
    if (Param("supportwatchers")) {
        # Find all those user-watching anyone on the current list, who is not 
        # on it already themselves.
        my $involved = join(",", keys %recipients);

        my $userwatchers = 
            $dbh->selectall_arrayref("SELECT watcher, watched FROM watch 
                                      WHERE watched IN ($involved)
                                      AND watcher NOT IN ($involved)");

        # Mark these people as having the role of the person they are watching
        foreach my $watch (@$userwatchers) {
            $recipients{$watch->[0]} = $recipients{$watch->[1]};
        }
    }
        
    # We now have a complete set of all the users, and their relationships to
    # the bug in question. However, we are not necessarily going to mail them
    # all - there are preferences, permissions checks and all sorts to do yet.
    my @sent;
    my @excluded;

    foreach my $user_id (keys %recipients) {
        my @rels_which_want;
        my $sent_mail = 0;

        my $user = new Bugzilla::User($user_id);
        # Deleted users must be excluded.
        next unless $user;

        if ($user->can_see_bug($id))
        {
            # Go through each role the user has and see if they want mail in
            # that role.
            foreach my $relationship (@{$recipients{$user_id}}) {
                if ($user->wants_bug_mail($id,
                                          $relationship, 
                                          \@diffs, 
                                          $newcomments, 
                                          $changer))
                {
                    push(@rels_which_want, $relationship);
                }
            }
        }
        
        if (scalar(@rels_which_want)) {
            # So the user exists, can see the bug, and wants mail in at least
            # one role. But do we want to send it to them?

            # If we are using insiders, and the comment is private, only send 
            # to insiders
            my $insider_ok = 1;
            $insider_ok = 0 if (Param("insidergroup") && 
                                ($anyprivate != 0) && 
                                (!$user->groups->{Param("insidergroup")}));

            # We shouldn't send mail if this is a dependency mail (i.e. there 
            # is something in @depbugs), and any of the depending bugs are not 
            # visible to the user. This is to avoid leaking the summaries of 
            # confidential bugs.
            my $dep_ok = 1;
            foreach my $dep_id (@depbugs) {
                if (!$user->can_see_bug($dep_id)) {
                   $dep_ok = 0;
                   last;
                }
            }

            # Make sure the user isn't in the nomail list, and the insider and 
            # dep checks passed.
            if ((!$nomail{$user->login}) &&
                $insider_ok &&
                $dep_ok)
            {
                # OK, OK, if we must. Email the user.
                $sent_mail = sendMail($user, 
                                      \@headerlist,
                                      \@rels_which_want, 
                                      \%values,
                                      \%defmailhead, 
                                      \%fielddescription, 
                                      \@diffparts,
                                      $newcomments, 
                                      $anyprivate, 
                                      $start, 
                                      $id);
            }
        }
       
        if ($sent_mail) {
            push(@sent, $user->login); 
        } 
        else {
            push(@excluded, $user->login); 
        } 
    }
    
    $dbh->do("UPDATE bugs SET lastdiffed = '$end' WHERE bug_id = $id");

    return {'sent' => \@sent, 'excluded' => \@excluded};
}

sub sendMail($$$$$$$$$$$$) {
    my ($user, $hlRef, $relRef, $valueRef, $dmhRef, $fdRef,  
        $diffRef, $newcomments, $anyprivate, $start, 
        $id) = @_;

    my %values = %$valueRef;
    my @headerlist = @$hlRef;
    my %mailhead = %$dmhRef;
    my %fielddescription = %$fdRef;
    my @diffparts = @$diffRef;    
    my $head = "";
    
    foreach my $f (@headerlist) {
      if ($mailhead{$f}) {
        my $value = $values{$f};
        # If there isn't anything to show, don't include this header
        if (! $value) {
          next;
        }
        # Only send estimated_time if it is enabled and the user is in the group
        if (($f ne 'estimated_time' && $f ne 'deadline') ||
             $user->groups->{Param('timetrackinggroup')}) {

            my $desc = $fielddescription{$f};
            $head .= FormatDouble($desc, $value);
        }
      }
    }

    # Build difftext (the actions) by verifying the user should see them
    my $difftext = "";
    my $diffheader = "";
    my $add_diff;

    foreach my $diff (@diffparts) {
        $add_diff = 0;
        
        if (exists($diff->{'fieldname'}) && 
            ($diff->{'fieldname'} eq 'estimated_time' ||
             $diff->{'fieldname'} eq 'remaining_time' ||
             $diff->{'fieldname'} eq 'work_time' ||
             $diff->{'fieldname'} eq 'deadline')){
            if ($user->groups->{Param("timetrackinggroup")}) {
                $add_diff = 1;
            }
        } elsif (($diff->{'isprivate'}) 
                 && Param('insidergroup')
                 && !($user->groups->{Param('insidergroup')})
                ) {
            $add_diff = 0;
        } else {
            $add_diff = 1;
        }

        if ($add_diff) {
            if (exists($diff->{'header'}) && 
             ($diffheader ne $diff->{'header'})) {
                $diffheader = $diff->{'header'};
                $difftext .= $diffheader;
            }
            $difftext .= $diff->{'text'};
        }
    }
 
    if ($difftext eq "" && $newcomments eq "") {
      # Whoops, no differences!
      return 0;
    }
    
    # XXX: This needs making localisable, probably by passing the role to
    # the email template and letting it choose the text.
    my $reasonsbody = "------- You are receiving this mail because: -------\n";

    foreach my $relationship (@$relRef) {
        if ($relationship == REL_ASSIGNEE) {
            $reasonsbody .= "You are the assignee for the bug, or are watching the assignee.\n";
        } elsif ($relationship == REL_REPORTER) {
            $reasonsbody .= "You reported the bug, or are watching the reporter.\n";
        } elsif ($relationship == REL_QA) {
            $reasonsbody .= "You are the QA contact for the bug, or are watching the QA contact.\n";
        } elsif ($relationship == REL_CC) {
            $reasonsbody .= "You are on the CC list for the bug, or are watching someone who is.\n";
        } elsif ($relationship == REL_VOTER) {
            $reasonsbody .= "You are a voter for the bug, or are watching someone who is.\n";
        }
    }

    my $isnew = !$start;
    
    my %substs;

    # If an attachment was created, then add an URL. (Note: the 'g'lobal
    # replace should work with comments with multiple attachments.)

    if ( $newcomments =~ /Created an attachment \(/ ) {

        my $showattachurlbase =
            Param('urlbase') . "attachment.cgi?id=";

        $newcomments =~ s/(Created an attachment \(id=([0-9]+)\))/$1\n --> \(${showattachurlbase}$2&action=view\)/g;
    }

    $substs{"neworchanged"} = $isnew ? 'New: ' : '';
    $substs{"to"} = $user->email;
    $substs{"cc"} = '';
    $substs{"bugid"} = $id;
    if ($isnew) {
      $substs{"diffs"} = $head . "\n\n" . $newcomments;
    } else {
      $substs{"diffs"} = $difftext . "\n\n" . $newcomments;
    }
    $substs{"product"} = $values{'product'};
    $substs{"component"} = $values{'component'};
    $substs{"summary"} = $values{'short_desc'};
    $substs{"reasonsheader"} = join(" ", map { $rel_names{$_} } @$relRef);
    $substs{"reasonsbody"} = $reasonsbody;
    $substs{"space"} = " ";
    if ($isnew) {
        $substs{'threadingmarker'} = "Message-ID: <bug-$id-" . 
                                     $user->id . "$sitespec>";
    } else {
        $substs{'threadingmarker'} = "In-Reply-To: <bug-$id-" . 
                                     $user->id . "$sitespec>";
    }
    
    my $template = Param("newchangedmail");
    
    my $msg = PerformSubsts($template, \%substs);

    MessageToMTA($msg);

    return 1;
}

sub MessageToMTA ($) {
    my ($msg) = (@_);
    return if (Param('mail_delivery_method') eq "none");

    if (Param("mail_delivery_method") eq "sendmail" && $^O =~ /MSWin32/i) {
        open(SENDMAIL, '|' . SENDMAIL_EXE . ' -t -i') ||
            die "Failed to execute " . SENDMAIL_EXE . ": $!\n";
        print SENDMAIL $msg;
        close SENDMAIL;
        return;
    }

    my @args;
    if (Param("mail_delivery_method") eq "sendmail") {
        push @args, "-i";
    }
    if (Param("mail_delivery_method") eq "sendmail" && !Param("sendmailnow")) {
        push @args, "-ODeliveryMode=deferred";
    }
    if (Param("mail_delivery_method") eq "smtp") {
        push @args, Server => Param("smtpserver");
    }
    my $mailer = new Mail::Mailer Param("mail_delivery_method"), @args;
    if (Param("mail_delivery_method") eq "testfile") {
        $Mail::Mailer::testfile::config{outfile} = "$datadir/mailer.testfile";
    }
    
    $msg =~ /(.*?)\n\n(.*)/ms;
    my @header_lines = split(/\n/, $1);
    my $body = $2;

    my $headers = new Mail::Header \@header_lines, Modify => 0;
    $mailer->open($headers->header_hashref);
    print $mailer $body;
    $mailer->close;
}

# Performs substitutions for sending out email with variables in it,
# or for inserting a parameter into some other string.
#
# Takes a string and a reference to a hash containing substitution 
# variables and their values.
#
# If the hash is not specified, or if we need to substitute something
# that's not in the hash, then we will use parameters to do the 
# substitution instead.
#
# Substitutions are always enclosed with '%' symbols. So they look like:
# %some_variable_name%. If "some_variable_name" is a key in the hash, then
# its value will be placed into the string. If it's not a key in the hash,
# then the value of the parameter called "some_variable_name" will be placed
# into the string.
sub PerformSubsts ($;$) {
    my ($str, $substs) = (@_);
    $str =~ s/%([a-z]*)%/(defined $substs->{$1} ? $substs->{$1} : Param($1))/eg;
    return $str;
}

1;
