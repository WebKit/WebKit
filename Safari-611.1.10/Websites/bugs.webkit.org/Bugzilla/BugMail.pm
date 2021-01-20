# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugMail;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Bug;
use Bugzilla::Comment;
use Bugzilla::Mailer;
use Bugzilla::Hook;
use Bugzilla::MIME;

use Date::Parse;
use Date::Format;
use Scalar::Util qw(blessed);
use List::MoreUtils qw(uniq);
use Storable qw(dclone);

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

    my $comments = [];

    if ($params->{dep_only}) {
        push(@diffs, { field_name => 'bug_status',
                       old        => $params->{changes}->{bug_status}->[0],
                       new        => $params->{changes}->{bug_status}->[1],
                       login_name => $changer->login,
                       who        => $changer,
                       blocker    => $params->{blocker} },
                     { field_name => 'resolution',
                       old        => $params->{changes}->{resolution}->[0],
                       new        => $params->{changes}->{resolution}->[1],
                       login_name => $changer->login,
                       who        => $changer,
                       blocker    => $params->{blocker} });
    }
    else {
        push(@diffs, _get_diffs($bug, $end, \%user_cache));

        $comments = $bug->comments({ after => $start, to => $end });
        # Skip empty comments.
        @$comments = grep { $_->type || $_->body =~ /\S/ } @$comments;

        # If no changes have been made, there is no need to process further.
        return {'sent' => []} unless scalar(@diffs) || scalar(@$comments);
    }

    ###########################################################################
    # Start of email filtering code
    ###########################################################################
    
    # A user_id => roles hash to keep track of people.
    my %recipients;
    my %watching;

    # We also record bugs that are referenced
    my @referenced_bug_ids = ();

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

        if ($change->{field_name} eq 'dependson' || $change->{field_name} eq 'blocked') {
            push @referenced_bug_ids, split(/[\s,]+/, $change->{old} // '');
            push @referenced_bug_ids, split(/[\s,]+/, $change->{new} // '');
        }
    }

    my $referenced_bugs = scalar(@referenced_bug_ids)
        ? Bugzilla::Bug->new_from_list([uniq @referenced_bug_ids])
        : [];

    # Make sure %user_cache has every user in it so far referenced
    foreach my $user_id (keys %recipients) {
        $user_cache{$user_id} ||= new Bugzilla::User($user_id);
    }
    
    Bugzilla::Hook::process('bugmail_recipients',
                            { bug => $bug, recipients => \%recipients,
                              users => \%user_cache, diffs => \@diffs });

    # We should not assume %recipients to have any entries.
    if (scalar keys %recipients) {
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

    # The email client will display the Date: header in the desired timezone,
    # so we can always use UTC here.
    my $date = $params->{dep_only} ? $end : $bug->delta_ts;
    $date = format_time($date, '%a, %d %b %Y %T %z', 'UTC');

    foreach my $user_id (keys %recipients) {
        my %rels_which_want;
        my $user = $user_cache{$user_id} ||= new Bugzilla::User($user_id);
        # Deleted users must be excluded.
        next unless $user;

        # If email notifications are disabled for this account, or the bug
        # is ignored, there is no need to do additional checks.
        next if ($user->email_disabled || $user->is_bug_ignored($id));

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

            # Email the user if the dep check passed.
            if ($dep_ok) {
                my $sent_mail = sendMail(
                    { to              => $user, 
                      bug             => $bug,
                      comments        => $comments,
                      date            => $date,
                      changer         => $changer,
                      watchers        => exists $watching{$user_id} ?
                                         $watching{$user_id} : undef,
                      diffs           => \@diffs,
                      rels_which_want => \%rels_which_want,
                      dep_only        => $params->{dep_only},
                      referenced_bugs => $referenced_bugs,
                    });
                push(@sent, $user->login) if $sent_mail;
            }
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

    return {'sent' => \@sent};
}

sub sendMail {
    my $params = shift;

    my $user            = $params->{to};
    my $bug             = $params->{bug};
    my @send_comments   = @{ $params->{comments} };
    my $date            = $params->{date};
    my $changer         = $params->{changer};
    my $watchingRef     = $params->{watchers};
    my @diffs           = @{ $params->{diffs} };
    my $relRef          = $params->{rels_which_want};
    my $dep_only        = $params->{dep_only};
    my $referenced_bugs = $params->{referenced_bugs};

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
    push @watchingrel, map { Bugzilla::User->new($_)->login } @$watchingRef;

    my @changedfields = uniq map { $_->{field_name} } @display_diffs;

    # Add attachments.created to changedfields if one or more
    # comments contain information about a new attachment
    if (grep($_->type == CMT_ATTACHMENT_CREATED, @send_comments)) {
        push(@changedfields, 'attachments.created');
    }

    my $bugmailtype = "changed";
    $bugmailtype = "new" if !$bug->lastdiffed;
    $bugmailtype = "dep_changed" if $dep_only;

    my $vars = {
        date               => $date,
        to_user            => $user,
        bug                => $bug,
        reasons            => \@reasons,
        reasons_watch      => \@reasons_watch,
        reasonsheader      => join(" ", @headerrel),
        reasonswatchheader => join(" ", @watchingrel),
        changer            => $changer,
        diffs              => \@display_diffs,
        changedfields      => \@changedfields,
        referenced_bugs    => $user->visible_bugs($referenced_bugs),
        new_comments       => \@send_comments,
        threadingmarker    => build_thread_marker($bug->id, $user->id, !$bug->lastdiffed),
        bugmailtype        => $bugmailtype,
    };
    if (Bugzilla->params->{'use_mailer_queue'}) {
        enqueue($vars);
    } else {
        MessageToMTA(_generate_bugmail($vars));
    }

    return 1;
}

sub enqueue {
    my ($vars) = @_;
    # we need to flatten all objects to a hash before pushing to the job queue.
    # the hashes need to be inflated in the dequeue method.
    $vars->{bug}          = _flatten_object($vars->{bug});
    $vars->{to_user}      = _flatten_object($vars->{to_user});
    $vars->{changer}      = _flatten_object($vars->{changer});
    $vars->{new_comments} = [ map { _flatten_object($_) } @{ $vars->{new_comments} } ];
    foreach my $diff (@{ $vars->{diffs} }) {
        $diff->{who} = _flatten_object($diff->{who});
        if (exists $diff->{blocker}) {
            $diff->{blocker} = _flatten_object($diff->{blocker});
        }
    }
    Bugzilla->job_queue->insert('bug_mail', { vars => $vars });
}

sub dequeue {
    my ($payload) = @_;
    # clone the payload so we can modify it without impacting TheSchwartz's
    # ability to process the job when we've finished
    my $vars = dclone($payload);
    # inflate objects
    $vars->{bug}          = Bugzilla::Bug->new_from_hash($vars->{bug});
    $vars->{to_user}      = Bugzilla::User->new_from_hash($vars->{to_user});
    $vars->{changer}      = Bugzilla::User->new_from_hash($vars->{changer});
    $vars->{new_comments} = [ map { Bugzilla::Comment->new_from_hash($_) } @{ $vars->{new_comments} } ];
    foreach my $diff (@{ $vars->{diffs} }) {
        $diff->{who} = Bugzilla::User->new_from_hash($diff->{who});
        if (exists $diff->{blocker}) {
            $diff->{blocker} = Bugzilla::Bug->new_from_hash($diff->{blocker});
        }
    }
    # generate bugmail and send
    MessageToMTA(_generate_bugmail($vars), 1);
}

sub _flatten_object {
    my ($object) = @_;
    # nothing to do if it's already flattened
    return $object unless blessed($object);
    # the same objects are used for each recipient, so cache the flattened hash
    my $cache = Bugzilla->request_cache->{bugmail_flat_objects} ||= {};
    my $key = blessed($object) . '-' . $object->id;
    return $cache->{$key} ||= $object->flatten_to_hash;
}

sub _generate_bugmail {
    my ($vars) = @_;
    my $user = $vars->{to_user};
    my $template = Bugzilla->template_inner($user->setting('lang'));
    my ($msg_text, $msg_html, $msg_header);
    state $use_utf8 = Bugzilla->params->{'utf8'};

    $template->process("email/bugmail-header.txt.tmpl", $vars, \$msg_header)
        || ThrowTemplateError($template->error());
    $template->process("email/bugmail.txt.tmpl", $vars, \$msg_text)
        || ThrowTemplateError($template->error());

    my @parts = (
        Bugzilla::MIME->create(
            attributes => {
                content_type => 'text/plain',
                charset      => $use_utf8 ? 'UTF-8' : 'iso-8859-1',
                encoding     => 'quoted-printable',
            },
            body_str => $msg_text,
        )
    );
    if ($user->setting('email_format') eq 'html') {
        $template->process("email/bugmail.html.tmpl", $vars, \$msg_html)
            || ThrowTemplateError($template->error());
        push @parts, Bugzilla::MIME->create(
            attributes => {
                content_type => 'text/html',
                charset      => $use_utf8 ? 'UTF-8' : 'iso-8859-1',
                encoding     => 'quoted-printable',
            },
            body_str => $msg_html,
        );
    }

    my $email = Bugzilla::MIME->new($msg_header);
    if (scalar(@parts) == 1) {
        $email->content_type_set($parts[0]->content_type);
    } else {
        $email->content_type_set('multipart/alternative');
        # Some mail clients need same encoding for each part, even empty ones.
        $email->charset_set('UTF-8') if $use_utf8;
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
          ORDER BY bugs_activity.bug_when, bugs_activity.id",
        {Slice=>{}}, @args);

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

    my @changes = ();
    foreach my $diff (@$diffs) {
        # If this is the same field as the previous item, then concatenate
        # the data into the same change.
        if (scalar(@changes)
            && $diff->{field_name}        eq $changes[-1]->{field_name}
            && $diff->{bug_when}          eq $changes[-1]->{bug_when}
            && $diff->{who}               eq $changes[-1]->{who}
            && ($diff->{attach_id} // 0)  == ($changes[-1]->{attach_id} // 0)
            && ($diff->{comment_id} // 0) == ($changes[-1]->{comment_id} // 0)
        ) {
            my $old_change = pop @changes;
            $diff->{old} = join_activity_entries($diff->{field_name}, $old_change->{old}, $diff->{old});
            $diff->{new} = join_activity_entries($diff->{field_name}, $old_change->{new}, $diff->{new});
        }
        push @changes, $diff;
    }

    return @changes;
}

sub _get_new_bugmail_fields {
    my $bug = shift;
    my @fields = @{ Bugzilla->fields({obsolete => 0, in_new_bugmail => 1}) };
    my @diffs;
    my $params = Bugzilla->params;

    foreach my $field (@fields) {
        my $name = $field->name;
        my $value = $bug->$name;

        next if !$field->is_visible_on_bug($bug)
            || ($name eq 'classification' && !$params->{'useclassification'})
            || ($name eq 'status_whiteboard' && !$params->{'usestatuswhiteboard'})
            || ($name eq 'qa_contact' && !$params->{'useqacontact'})
            || ($name eq 'target_milestone' && !$params->{'usetargetmilestone'});

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

        push(@diffs, {
            field_name => $name,
            who        => $bug->reporter,
            new        => $value});
    }

    return @diffs;
}

1;

=head1 NAME

BugMail - Routines to generate email notifications when a bug is created or
modified.

=head1 METHODS

=over 4

=item C<enqueue>

Serialises the variables required to generate bugmail and pushes the result to
the job-queue for processing by TheSchwartz.

=item C<dequeue>

When given serialised variables from the job-queue, recreates the objects from
the flattened hashes, generates the bugmail, and sends it.

=back

=head1 B<Methods in need of POD>

=over

=item relationships

=item sendMail

=item Send

=back
