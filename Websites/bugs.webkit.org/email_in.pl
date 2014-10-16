#!/usr/bin/env perl -wT
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
# The Original Code is the Bugzilla Inbound Email System.
#
# The Initial Developer of the Original Code is Akamai Technologies, Inc.
# Portions created by Akamai are Copyright (C) 2006 Akamai Technologies, 
# Inc. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use warnings;

# MTAs may call this script from any directory, but it should always
# run from this one so that it can find its modules.
use Cwd qw(abs_path);
use File::Basename qw(dirname);
BEGIN {
    # Untaint the abs_path.
    my ($a) = abs_path($0) =~ /^(.*)$/;
    chdir dirname($a);
}

use lib qw(. lib);

use Data::Dumper;
use Email::Address;
use Email::Reply qw(reply);
use Email::MIME;
use Email::MIME::Attachment::Stripper;
use Getopt::Long qw(:config bundling);
use Pod::Usage;
use Encode;
use Scalar::Util qw(blessed);

use Bugzilla;
use Bugzilla::Attachment;
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Mailer;
use Bugzilla::Token;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Hook;

#############
# Constants #
#############

# This is the USENET standard line for beginning a signature block
# in a message. RFC-compliant mailers use this.
use constant SIGNATURE_DELIMITER => '-- ';

# $input_email is a global so that it can be used in die_handler.
our ($input_email, %switch);

####################
# Main Subroutines #
####################

sub parse_mail {
    my ($mail_text) = @_;
    debug_print('Parsing Email');
    $input_email = Email::MIME->new($mail_text);
    
    my %fields = %{ $switch{'default'} || {} };
    Bugzilla::Hook::process('email_in_before_parse', { mail => $input_email,
                                                       fields => \%fields });

    my $summary = $input_email->header('Subject');
    if ($summary =~ /\[\S+ (\d+)\](.*)/i) {
        $fields{'bug_id'} = $1;
        $summary = trim($2);
    }

    # Ignore automatic replies.
    # XXX - Improve the way to detect such subjects in different languages.
    my $auto_submitted = $input_email->header('Auto-Submitted') || '';
    if ($summary =~ /out of( the)? office/i || $auto_submitted eq 'auto-replied') {
        debug_print("Automatic reply detected: $summary");
        exit;
    }

    my ($body, $attachments) = get_body_and_attachments($input_email);
    if (@$attachments) {
        $fields{'attachments'} = $attachments;
    }

    debug_print("Body:\n" . $body, 3);

    $body = remove_leading_blank_lines($body);
    my @body_lines = split(/\r?\n/s, $body);

    # If there are fields specified.
    if ($body =~ /^\s*@/s) {
        my $current_field;
        while (my $line = shift @body_lines) {
            # If the sig is starting, we want to keep this in the 
            # @body_lines so that we don't keep the sig as part of the 
            # comment down below.
            if ($line eq SIGNATURE_DELIMITER) {
                unshift(@body_lines, $line);
                last;
            }
            # Otherwise, we stop parsing fields on the first blank line.
            $line = trim($line);
            last if !$line;
            if ($line =~ /^\@(\w+)\s*(?:=|\s|$)\s*(.*)\s*/) {
                $current_field = lc($1);
                $fields{$current_field} = $2;
            }
            else {
                $fields{$current_field} .= " $line";
            }
        }
    }

    %fields = %{ Bugzilla::Bug::map_fields(\%fields) };

    my ($reporter) = Email::Address->parse($input_email->header('From'));
    $fields{'reporter'} = $reporter->address;

    # The summary line only affects us if we're doing a post_bug.
    # We have to check it down here because there might have been
    # a bug_id specified in the body of the email.
    if (!$fields{'bug_id'} && !$fields{'short_desc'}) {
        $fields{'short_desc'} = $summary;
    }

    my $comment = '';
    # Get the description, except the signature.
    foreach my $line (@body_lines) {
        last if $line eq SIGNATURE_DELIMITER;
        $comment .= "$line\n";
    }
    $fields{'comment'} = $comment;

    my %override = %{ $switch{'override'} || {} };
    foreach my $key (keys %override) {
        $fields{$key} = $override{$key};
    }

    debug_print("Parsed Fields:\n" . Dumper(\%fields), 2);

    return \%fields;
}

sub check_email_fields {
    my ($fields) = @_;

    my ($retval, $non_conclusive_fields) =
      Bugzilla::User::match_field({
        'assigned_to'   => { 'type' => 'single' },
        'qa_contact'    => { 'type' => 'single' },
        'cc'            => { 'type' => 'multi'  },
        'newcc'         => { 'type' => 'multi'  }
      }, $fields, MATCH_SKIP_CONFIRM);

    if ($retval != USER_MATCH_SUCCESS) {
        ThrowUserError('user_match_too_many', {fields => $non_conclusive_fields});
    }
}

sub post_bug {
    my ($fields) = @_;
    debug_print('Posting a new bug...');

    my $user = Bugzilla->user;

    check_email_fields($fields);

    my $bug = Bugzilla::Bug->create($fields);
    debug_print("Created bug " . $bug->id);
    return ($bug, $bug->comments->[0]);
}

sub process_bug {
    my ($fields_in) = @_; 
    my %fields = %$fields_in;

    my $bug_id = $fields{'bug_id'};
    $fields{'id'} = $bug_id;
    delete $fields{'bug_id'};

    debug_print("Updating Bug $fields{id}...");

    my $bug = Bugzilla::Bug->check($bug_id);

    if ($fields{'bug_status'}) {
        $fields{'knob'} = $fields{'bug_status'};
    }
    # If no status is given, then we only want to change the resolution.
    elsif ($fields{'resolution'}) {
        $fields{'knob'} = 'change_resolution';
        $fields{'resolution_knob_change_resolution'} = $fields{'resolution'};
    }
    if ($fields{'dup_id'}) {
        $fields{'knob'} = 'duplicate';
    }

    # Move @cc to @newcc as @cc is used by process_bug.cgi to remove
    # users from the CC list when @removecc is set.
    $fields{'newcc'} = delete $fields{'cc'} if $fields{'cc'};

    # Make it possible to remove CCs.
    if ($fields{'removecc'}) {
        $fields{'cc'} = [split(',', $fields{'removecc'})];
        $fields{'removecc'} = 1;
    }

    check_email_fields(\%fields);

    my $cgi = Bugzilla->cgi;
    foreach my $field (keys %fields) {
        $cgi->param(-name => $field, -value => $fields{$field});
    }
    $cgi->param('longdesclength', scalar @{ $bug->comments });
    $cgi->param('token', issue_hash_token([$bug->id, $bug->delta_ts]));

    require 'process_bug.cgi';
    debug_print("Bug processed.");

    my $added_comment;
    if (trim($fields{'comment'})) {
        $added_comment = $bug->comments->[-1];
    }
    return ($bug, $added_comment);
}

sub handle_attachments {
    my ($bug, $attachments, $comment) = @_;
    return if !$attachments;
    debug_print("Handling attachments...");
    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    my ($update_comment, $update_bug);
    foreach my $attachment (@$attachments) {
        my $data = delete $attachment->{payload};
        debug_print("Inserting Attachment: " . Dumper($attachment), 2);
        $attachment->{content_type} ||= 'application/octet-stream';
        my $obj = Bugzilla::Attachment->create({
            bug         => $bug,
            description => $attachment->{filename},
            filename    => $attachment->{filename},
            mimetype    => $attachment->{content_type},
            data        => $data,
        });
        # If we added a comment, and our comment does not already have a type,
        # and this is our first attachment, then we make the comment an 
        # "attachment created" comment.
        if ($comment and !$comment->type and !$update_comment) {
            $comment->set_all({ type       => CMT_ATTACHMENT_CREATED, 
                                extra_data => $obj->id });
            $update_comment = 1;
        }
        else {
            $bug->add_comment('', { type => CMT_ATTACHMENT_CREATED,
                                    extra_data => $obj->id });
            $update_bug = 1;
        }
    }
    # We only update the comments and bugs at the end of the transaction,
    # because doing so modifies bugs_fulltext, which is a non-transactional
    # table.
    $bug->update() if $update_bug;
    $comment->update() if $update_comment;
    $dbh->bz_commit_transaction();
}

######################
# Helper Subroutines #
######################

sub debug_print {
    my ($str, $level) = @_;
    $level ||= 1;
    print STDERR "$str\n" if $level <= $switch{'verbose'};
}

sub get_body_and_attachments {
    my ($email) = @_;

    my $ct = $email->content_type || 'text/plain';
    debug_print("Splitting Body and Attachments [Type: $ct]...");

    my $body;
    my $attachments = [];
    if ($ct =~ /^multipart\/(alternative|signed)/i) {
        $body = get_text_alternative($email);
    }
    else {
        my $stripper = new Email::MIME::Attachment::Stripper(
            $email, force_filename => 1);
        my $message = $stripper->message;
        $body = get_text_alternative($message);
        $attachments = [$stripper->attachments];
    }

    return ($body, $attachments);
}

sub get_text_alternative {
    my ($email) = @_;

    my @parts = $email->parts;
    my $body;
    foreach my $part (@parts) {
        my $ct = $part->content_type || 'text/plain';
        my $charset = 'iso-8859-1';
        # The charset may be quoted.
        if ($ct =~ /charset="?([^;"]+)/) {
            $charset= $1;
        }
        debug_print("Part Content-Type: $ct", 2);
        debug_print("Part Character Encoding: $charset", 2);
        if (!$ct || $ct =~ /^text\/plain/i) {
            $body = $part->body;
            if (Bugzilla->params->{'utf8'} && !utf8::is_utf8($body)) {
                $body = Encode::decode($charset, $body);
            }
            last;
        }
    }

    if (!defined $body) {
        # Note that this only happens if the email does not contain any
        # text/plain parts. If the email has an empty text/plain part,
        # you're fine, and this message does NOT get thrown.
        ThrowUserError('email_no_text_plain');
    }

    return $body;
}

sub remove_leading_blank_lines {
    my ($text) = @_;
    $text =~ s/^(\s*\n)+//s;
    return $text;
}

sub html_strip {
    my ($var) = @_;
    # Trivial HTML tag remover (this is just for error messages, really.)
    $var =~ s/<[^>]*>//g;
    # And this basically reverses the Template-Toolkit html filter.
    $var =~ s/\&amp;/\&/g;
    $var =~ s/\&lt;/</g;
    $var =~ s/\&gt;/>/g;
    $var =~ s/\&quot;/\"/g;
    $var =~ s/&#64;/@/g;
    # Also remove undesired newlines and consecutive spaces.
    $var =~ s/[\n\s]+/ /gms;
    return $var;
}


sub die_handler {
    my ($msg) = @_;

    # In Template-Toolkit, [% RETURN %] is implemented as a call to "die".
    # But of course, we really don't want to actually *die* just because
    # the user-error or code-error template ended. So we don't really die.
    return if blessed($msg) && $msg->isa('Template::Exception')
              && $msg->type eq 'return';

    # If this is inside an eval, then we should just act like...we're
    # in an eval (instead of printing the error and exiting).
    die(@_) if $^S;

    # We can't depend on the MTA to send an error message, so we have
    # to generate one properly.
    if ($input_email) {
       $msg =~ s/at .+ line.*$//ms;
       $msg =~ s/^Compilation failed in require.+$//ms;
       $msg = html_strip($msg);
       my $from = Bugzilla->params->{'mailfrom'};
       my $reply = reply(to => $input_email, from => $from, top_post => 1, 
                         body => "$msg\n");
       MessageToMTA($reply->as_string);
    }
    print STDERR "$msg\n";
    # We exit with a successful value, because we don't want the MTA
    # to *also* send a failure notice.
    exit;
}

###############
# Main Script #
###############

$SIG{__DIE__} = \&die_handler;

GetOptions(\%switch, 'help|h', 'verbose|v+', 'default=s%', 'override=s%');
$switch{'verbose'} ||= 0;

# Print the help message if that switch was selected.
pod2usage({-verbose => 0, -exitval => 1}) if $switch{'help'};

Bugzilla->usage_mode(USAGE_MODE_EMAIL);

my @mail_lines = <STDIN>;
my $mail_text = join("", @mail_lines);
my $mail_fields = parse_mail($mail_text);

Bugzilla::Hook::process('email_in_after_parse', { fields => $mail_fields });

my $attachments = delete $mail_fields->{'attachments'};

my $username = $mail_fields->{'reporter'};
# If emailsuffix is in use, we have to remove it from the email address.
if (my $suffix = Bugzilla->params->{'emailsuffix'}) {
    $username =~ s/\Q$suffix\E$//i;
}

my $user = Bugzilla::User->check($username);
Bugzilla->set_user($user);

my ($bug, $comment);
if ($mail_fields->{'bug_id'}) {
    ($bug, $comment) = process_bug($mail_fields);
}
else {
    ($bug, $comment) = post_bug($mail_fields);
}

handle_attachments($bug, $attachments, $comment);

# This is here for post_bug and handle_attachments, so that when posting a bug
# with an attachment, any comment goes out as an attachment comment.
#
# Eventually this should be sending the mail for process_bug, too, but we have
# to wait for $bug->update() to be fully used in email_in.pl first. So
# currently, process_bug.cgi does the mail sending for bugs, and this does
# any mail sending for attachments after the first one.
Bugzilla::BugMail::Send($bug->id, { changer => Bugzilla->user });
debug_print("Sent bugmail");


__END__

=head1 NAME

email_in.pl - The Bugzilla Inbound Email Interface

=head1 SYNOPSIS

./email_in.pl [-vvv] [--default name=value] [--override name=value] < email.txt

Reads an email on STDIN (the standard input).

Options:

   --verbose (-v)        - Make the script print more to STDERR.
                           Specify multiple times to print even more.

   --default name=value  - Specify defaults for field values, like
                           product=TestProduct. Can be specified multiple
                           times to specify defaults for multiple fields.

   --override name=value - Override field values specified in the email,
                           like product=TestProduct. Can be specified
                           multiple times to override multiple fields.

=head1 DESCRIPTION

This script processes inbound email and creates a bug, or appends data
to an existing bug.

=head2 Creating a New Bug

The script expects to read an email with the following format:

 From: account@domain.com
 Subject: Bug Summary

 @product ProductName
 @component ComponentName
 @version 1.0

 This is a bug description. It will be entered into the bug exactly as
 written here.

 It can be multiple paragraphs.

 -- 
 This is a signature line, and will be removed automatically, It will not
 be included in the bug description.

For the list of valid field names for the C<@> fields, including
a list of which ones are required, see L<Bugzilla::WebService::Bug/create>.
(Note, however, that you cannot specify C<@description> as a field--
you just add a comment by adding text after the C<@> fields.)

The values for the fields can be split across multiple lines, but
note that a newline will be parsed as a single space, for the value.
So, for example:

 @summary This is a very long
 description

Will be parsed as "This is a very long description".

If you specify C<@summary>, it will override the summary you specify
in the Subject header.

C<account@domain.com> (the value of the C<From> header) must be a valid
Bugzilla account.

Note that signatures must start with '-- ', the standard signature
border.

=head2 Modifying an Existing Bug

Bugzilla determines what bug you want to modify in one of two ways:

=over

=item *

Your subject starts with [Bug 123456] -- then it modifies bug 123456.

=item *

You include C<@id 123456> in the first lines of the email.

=back

If you do both, C<@id> takes precedence.

You send your email in the same format as for creating a bug, except
that you only specify the fields you want to change. If the very
first non-blank line of the email doesn't begin with C<@>, then it
will be assumed that you are only adding a comment to the bug.

Note that when updating a bug, the C<Subject> header is ignored,
except for getting the bug ID. If you want to change the bug's summary,
you have to specify C<@summary> as one of the fields to change.

Please remember not to include any extra text in your emails, as that
text will also be added as a comment. This includes any text that your
email client automatically quoted and included, if this is a reply to
another email.

=head3 Adding/Removing CCs

To add CCs, you can specify them in a comma-separated list in C<@cc>.

To remove CCs, specify them as a comma-separated list in C<@removecc>.

=head2 Errors

If your request cannot be completed for any reason, Bugzilla will
send an email back to you. If your request succeeds, Bugzilla will
not send you anything.

If any part of your request fails, all of it will fail. No partial
changes will happen.

=head1 CAUTION

The script does not do any validation that the user is who they say
they are. That is, it accepts I<any> 'From' address, as long as it's
a valid Bugzilla account. So make sure that your MTA validates that
the message is actually coming from who it says it's coming from,
and only allow access to the inbound email system from people you trust.

=head1 LIMITATIONS

The email interface only accepts emails that are correctly formatted
per RFC2822. If you send it an incorrectly formatted message, it
may behave in an unpredictable fashion.

You cannot send an HTML mail along with attachments. If you do, Bugzilla
will reject your email, saying that it doesn't contain any text. This
is a bug in L<Email::MIME::Attachment::Stripper> that we can't work
around.

You cannot modify Flags through the email interface.
