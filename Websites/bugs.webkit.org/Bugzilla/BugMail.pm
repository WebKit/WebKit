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
#                 Byron Jones <bugzilla@glob.com.au>
#                 Reed Loden <reed@reedloden.com>
#                 Frédéric Buclin <LpSolit@gmail.com>
#                 Guy Pyrzak <guy.pyrzak@gmail.com>

use strict;

package Bugzilla::BugMail;

use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Bug;
use Bugzilla::Comment;
use Bugzilla::Mailer;
use Bugzilla::Hook;

use Date::Parse;
use Date::Format;
use Scalar::Util qw(blessed);
use List::MoreUtils qw(uniq);

use constant BIT_DIRECT    => 1;
use constant BIT_WATCHING  => 2;

sub relationships {
    my $ref = RELATIONSHIPS;
    # Clone it so that we don't modify the constant;
    my %relationships = %$ref;
    Bugzilla::Hook::process('bugmail_relationships', 
                            { relationships => \%relationships });
    return %relationships;
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
sub Send {
    my ($id, $forced, $params) = @_;
    $params ||= {};

    my $dbh = Bugzilla->dbh;
    my $bug = new Bugzilla::Bug($id);

    my $start = $bug->lastdiffed;
    my $end   = $dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');

    # Bugzilla::User objects of people in various roles. More than one person
    # can 'have' a role, if the person in that role has changed, or people are
    # watching.
    my @assignees = ($bug->assigned_to);
    my @qa_contacts = $bug->qa_contact || ();

    my @ccs = @{ $bug->cc_users };
    # Include the people passed in as being in particular roles.
    # This can include people who used to hold those roles.
    # At this point, we don't care if there are duplicates in these arrays.
    my $changer = $forced->{'changer'};
    if ($forced->{'owner'}) {
        push (@assignees, Bugzilla::User->check($forced->{'owner'}));
    }
    
    if ($forced->{'qacontact'}) {
        push (@qa_contacts, Bugzilla::User->check($forced->{'qacontact'}));
    }
    
    if ($forced->{'cc'}) {
        foreach my $cc (@{$forced->{'cc'}}) {
            push(@ccs, Bugzilla::User->check($cc));
        }
    }
    my %user_cache = map { $_->id => $_ } (@assignees, @qa_contacts, @ccs);

    my @diffs;
    if (!$start) {
        @diffs = _get_new_bugmail_fields($bug);
    }

    if ($params->{dep_only}) {
        push(@diffs, { field_name => 'bug_status',
                       old => $params->{changes}->{bug_status}->[0],
                       new => $params->{changes}->{bug_status}->[1],
                       login_name => $changer->login,
                       blocker => $params->{blocker} },
                     { field_name => 'resolution',
                       old => $params->{changes}->{resolution}->[0],
                       new => $params->{changes}->{resolution}->[1],
                       login_name => $changer->login,
                       blocker => $params->{blocker} });
    }
    else {
        push(@diffs, _get_diffs($bug, $end, \%user_cache));
    }

    my $comments = $bug->comments({ after => $start, to => $end });
    # Skip empty comments.
    @$comments = grep { $_->type || $_->body =~ /\S/ } @$comments;

    ###########################################################################
    # Start of email filtering code
    ###########################################################################
    
    # A user_id => roles hash to keep track of people.
    my %recipients;
    my %watching;
    
    # Now we work out all the people involved with this bug, and note all of
    # the relationships in a hash. The keys are userids, the values are an
    # array of role constants.
    
    # CCs
    $recipients{$_->id}->{+REL_CC} = BIT_DIRECT foreach (@ccs);
    
    # Reporter (there's only ever one)
    $recipients{$bug->reporter->id}->{+REL_REPORTER} = BIT_DIRECT;
    
    # QA Contact
    if (Bugzilla->params->{'useqacontact'}) {
        foreach (@qa_contacts) {
            # QA Contact can be blank; ignore it if so.
            $recipients{$_->id}->{+REL_QA} = BIT_DIRECT if $_;
        }
    }

    # Assignee
    $recipients{$_->id}->{+REL_ASSIGNEE} = BIT_DIRECT foreach (@assignees);

    # The last relevant set of people are those who are being removed from 
    # their roles in this change. We get their names out of the diffs.
    foreach my $change (@diffs) {
        if ($change->{old}) {
            # You can't stop being the reporter, so we don't check that
            # relationship here.
            # Ignore people whose user account has been deleted or renamed.
            if ($change->{field_name} eq 'cc') {
                foreach my $cc_user (split(/[\s,]+/, $change->{old})) {
                    my $uid = login_to_id($cc_user);
                    $recipients{$uid}->{+REL_CC} = BIT_DIRECT if $uid;
                }
            }
            elsif ($change->{field_name} eq 'qa_contact') {
                my $uid = login_to_id($change->{old});
                $recipients{$uid}->{+REL_QA} = BIT_DIRECT if $uid;
            }
            elsif ($change->{field_name} eq 'assigned_to') {
                my $uid = login_to_id($change->{old});
                $recipients{$uid}->{+REL_ASSIGNEE} = BIT_DIRECT if $uid;
            }
        }
    }

    # Make sure %user_cache has every user in it so far referenced
    foreach my $user_id (keys %recipients) {
        $user_cache{$user_id} ||= new Bugzilla::User($user_id);
    }
    
    Bugzilla::Hook::process('bugmail_recipients',
                            { bug => $bug, recipients => \%recipients,
                              users => \%user_cache, diffs => \@diffs });

    # Find all those user-watching anyone on the current list, who is not
    # on it already themselves.
    my $involved = join(",", keys %recipients);

    my $userwatchers =
        $dbh->selectall_arrayref("SELECT watcher, watched FROM watch
                                  WHERE watched IN ($involved)");

    # Mark these people as having the role of the person they are watching
    foreach my $watch (@$userwatchers) {
        while (my ($role, $bits) = each %{$recipients{$watch->[1]}}) {
            $recipients{$watch->[0]}->{$role} |= BIT_WATCHING
                if $bits & BIT_DIRECT;
        }
        push(@{$watching{$watch->[0]}}, $watch->[1]);
    }

    # Global watcher
    my @watchers = split(/[,\s]+/, Bugzilla->params->{'globalwatchers'});
    foreach (@watchers) {
        my $watcher_id = login_to_id($_);
        next unless $watcher_id;
        $recipients{$watcher_id}->{+REL_GLOBAL_WATCHER} = BIT_DIRECT;
    }

    # We now have a complete set of all the users, and their relationships to
    # the bug in question. However, we are not necessarily going to mail them
    # all - there are preferences, permissions checks and all sorts to do yet.
    my @sent;
    my @excluded;

    # The email client will display the Date: header in the desired timezone,
    # so we can always use UTC here.
    my $date = $params->{dep_only} ? $end : $bug->delta_ts;
    $date = format_time($date, '%a, %d %b %Y %T %z', 'UTC');

    foreach my $user_id (keys %recipients) {
        my %rels_which_want;
        my $sent_mail = 0;
        $user_cache{$user_id} ||= new Bugzilla::User($user_id);
        my $user = $user_cache{$user_id};
        # Deleted users must be excluded.
        next unless $user;

        if ($user->can_see_bug($id)) {
            # Go through each role the user has and see if they want mail in
            # that role.
            foreach my $relationship (keys %{$recipients{$user_id}}) {
                if ($user->wants_bug_mail($bug,
                                          $relationship, 
                                          $start ? \@diffs : [],
                                          $comments,
                                          $params->{dep_only},
                                          $changer))
                {
                    $rels_which_want{$relationship} = 
                        $recipients{$user_id}->{$relationship};
                }
            }
        }
        
        if (scalar(%rels_which_want)) {
            # So the user exists, can see the bug, and wants mail in at least
            # one role. But do we want to send it to them?

            # We shouldn't send mail if this is a dependency mail and the
            # depending bug is not visible to the user.
            # This is to avoid leaking the summary of a confidential bug.
            my $dep_ok = 1;
            if ($params->{dep_only}) {
                $dep_ok = $user->can_see_bug($params->{blocker}->id) ? 1 : 0;
            }

            # Make sure the user isn't in the nomail list, and the dep check passed.
            if ($user->email_enabled && $dep_ok) {
                # OK, OK, if we must. Email the user.
                $sent_mail = sendMail(
                    { to       => $user, 
                      bug      => $bug,
                      comments => $comments,
                      date     => $date,
                      changer  => $changer,
                      watchers => exists $watching{$user_id} ?
                                  $watching{$user_id} : undef,
                      diffs    => \@diffs,
                      rels_which_want => \%rels_which_want,
                    });
            }
        }

        if ($sent_mail) {
            push(@sent, $user->login); 
        } 
        else {
            push(@excluded, $user->login); 
        } 
    }

    # When sending bugmail about a blocker being reopened or resolved,
    # we say nothing about changes in the bug being blocked, so we must
    # not update lastdiffed in this case.
    if (!$params->{dep_only}) {
        $dbh->do('UPDATE bugs SET lastdiffed = ? WHERE bug_id = ?',
                 undef, ($end, $id));
        $bug->{lastdiffed} = $end;
    }

    return {'sent' => \@sent, 'excluded' => \@excluded};
}

sub sendMail {
    my $params = shift;
    
    my $user   = $params->{to};
    my $bug    = $params->{bug};
    my @send_comments = @{ $params->{comments} };
    my $date = $params->{date};
    my $changer = $params->{changer};
    my $watchingRef = $params->{watchers};
    my @diffs = @{ $params->{diffs} };
    my $relRef      = $params->{rels_which_want};

    # Only display changes the user is allowed see.
    my @display_diffs;

    foreach my $diff (@diffs) {
        my $add_diff = 0;
        
        if (grep { $_ eq $diff->{field_name} } TIMETRACKING_FIELDS) {
            $add_diff = 1 if $user->is_timetracker;
        }
        elsif (!$diff->{isprivate} || $user->is_insider) {
            $add_diff = 1;
        }
        push(@display_diffs, $diff) if $add_diff;
    }

    if (!$user->is_insider) {
        @send_comments = grep { !$_->is_private } @send_comments;
    }

    if (!scalar(@display_diffs) && !scalar(@send_comments)) {
      # Whoops, no differences!
      return 0;
    }

    my (@reasons, @reasons_watch);
    while (my ($relationship, $bits) = each %{$relRef}) {
        push(@reasons, $relationship) if ($bits & BIT_DIRECT);
        push(@reasons_watch, $relationship) if ($bits & BIT_WATCHING);
    }

    my %relationships = relationships();
    my @headerrel   = map { $relationships{$_} } @reasons;
    my @watchingrel = map { $relationships{$_} } @reasons_watch;
    push(@headerrel,   'None') unless @headerrel;
    push(@watchingrel, 'None') unless @watchingrel;
    push @watchingrel, map { user_id_to_login($_) } @$watchingRef;

    my $vars = {
        date => $date,
        to_user => $user,
        bug => $bug,
        reasons => \@reasons,
        reasons_watch => \@reasons_watch,
        reasonsheader => join(" ", @headerrel),
        reasonswatchheader => join(" ", @watchingrel),
        changer => $changer,
        diffs => \@display_diffs,
        changedfields => [uniq map { $_->{field_name} } @display_diffs],
        new_comments => \@send_comments,
        threadingmarker => build_thread_marker($bug->id, $user->id, !$bug->lastdiffed),
    };
    my $msg =  _generate_bugmail($user, $vars);
    MessageToMTA($msg);

    return 1;
}

sub _generate_bugmail {
    my ($user, $vars) = @_;
    my $template = Bugzilla->template_inner($user->setting('lang'));
    my ($msg_text, $msg_html, $msg_header);
  
    $template->process("email/bugmail-header.txt.tmpl", $vars, \$msg_header)
        || ThrowTemplateError($template->error());
    $template->process("email/bugmail.txt.tmpl", $vars, \$msg_text)
        || ThrowTemplateError($template->error());

    my @parts = (
        Email::MIME->create(
            attributes => {
                content_type => "text/plain",
            },
            body => $msg_text,
        )
    );
    if ($user->setting('email_format') eq 'html') {
        $template->process("email/bugmail.html.tmpl", $vars, \$msg_html)
            || ThrowTemplateError($template->error());
        push @parts, Email::MIME->create(
            attributes => {
                content_type => "text/html",         
            },
            body => $msg_html,
        );
    }

    # TT trims the trailing newline, and threadingmarker may be ignored.
    my $email = new Email::MIME("$msg_header\n");
    if (scalar(@parts) == 1) {
        $email->content_type_set($parts[0]->content_type);
    } else {
        $email->content_type_set('multipart/alternative');
    }
    $email->parts_set(\@parts);
    return $email;
}

sub _get_diffs {
    my ($bug, $end, $user_cache) = @_;
    my $dbh = Bugzilla->dbh;

    my @args = ($bug->id);
    # If lastdiffed is NULL, then we don't limit the search on time.
    my $when_restriction = '';
    if ($bug->lastdiffed) {
        $when_restriction = ' AND bug_when > ? AND bug_when <= ?';
        push @args, ($bug->lastdiffed, $end);
    }

    my $diffs = $dbh->selectall_arrayref(
           "SELECT fielddefs.name AS field_name,
                   bugs_activity.bug_when, bugs_activity.removed AS old,
                   bugs_activity.added AS new, bugs_activity.attach_id,
                   bugs_activity.comment_id, bugs_activity.who
              FROM bugs_activity
        INNER JOIN fielddefs
                ON fielddefs.id = bugs_activity.fieldid
             WHERE bugs_activity.bug_id = ?
                   $when_restriction
          ORDER BY bugs_activity.bug_when", {Slice=>{}}, @args);

    foreach my $diff (@$diffs) {
        $user_cache->{$diff->{who}} ||= new Bugzilla::User($diff->{who}); 
        $diff->{who} =  $user_cache->{$diff->{who}};
        if ($diff->{attach_id}) {
            $diff->{isprivate} = $dbh->selectrow_array(
                'SELECT isprivate FROM attachments WHERE attach_id = ?',
                undef, $diff->{attach_id});
         }
         if ($diff->{field_name} eq 'longdescs.isprivate') {
             my $comment = Bugzilla::Comment->new($diff->{comment_id});
             $diff->{num} = $comment->count;
             $diff->{isprivate} = $diff->{new};
         }
    }

    return @$diffs;
}

sub _get_new_bugmail_fields {
    my $bug = shift;
    my @fields = @{ Bugzilla->fields({obsolete => 0, in_new_bugmail => 1}) };
    my @diffs;

    foreach my $field (@fields) {
        my $name = $field->name;
        my $value = $bug->$name;

        if (ref $value eq 'ARRAY') {
            $value = join(', ', @$value);
        }
        elsif (blessed($value) && $value->isa('Bugzilla::User')) {
            $value = $value->login;
        }
        elsif (blessed($value) && $value->isa('Bugzilla::Object')) {
            $value = $value->name;
        }
        elsif ($name eq 'estimated_time') {
            # "0.00" (which is what we get from the DB) is true,
            # so we explicitly do a numerical comparison with 0.
            $value = 0 if $value == 0;
        }
        elsif ($name eq 'deadline') {
            $value = time2str("%Y-%m-%d", str2time($value)) if $value;
        }

        # If there isn't anything to show, don't include this header.
        next unless $value;

        push(@diffs, {field_name => $name, new => $value});
    }

    return @diffs;
}

1;
