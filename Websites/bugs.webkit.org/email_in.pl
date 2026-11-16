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
use Getopt::Long qw(:config bundling);
use HTML::FormatText::WithLinks;
use Pod::Usage;
use Encode;
use Scalar::Util qw(blessed);
use List::MoreUtils qw(firstidx);

use Bugzilla;
use Bugzilla::Attachment;
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field;
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

# These MIME types represent a "body" of an email if they have an
# "inline" Content-Disposition (or no content disposition).
use constant BODY_TYPES => qw(
    text/plain
    text/html
    application/xhtml+xml
    multipart/alternative
);

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

    # The Importance/X-Priority headers are only used when creating a new bug.
    # 1) If somebody specifies a priority, use it.
    # 2) If there is an Importance or X-Priority header, use it as
    #    something that is relative to the default priority.
    #    If the value is High or 1, increase the priority by 1.
    #    If the value is Low or 5, decrease the priority by 1.
    # 3) Otherwise, use the default priority.
    # Note: this will only work if the 'letsubmitterchoosepriority'
    # parameter is enabled.
    my $importance = $input_email->header('Importance')
                     || $input_email->header('X-Priority');
    if (!$fields{'bug_id'} && !$fields{'priority'} && $importance) {
        my @legal_priorities = @{get_legal_field_values('priority')};
        my $i = firstidx { $_ eq Bugzilla->params->{'defaultpriority'} } @legal_priorities;
        if ($importance =~ /(high|[12])/i) {
            $i-- unless $i == 0;
        }
        elsif ($importance =~ /(low|[45])/i) {
            $i++ unless $i == $#legal_priorities;
        }
        $fields{'priority'} = $legal_priorities[$i];
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

    debug_print("Attachments:\n" . Dumper($attachments), 3);
    if (@$attachments) {
        $fields{'attachments'} = $attachments;
    }

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
    $cgi->param('token', issue_hash_token([$bug->id, $bug->delta_ts]));

    require 'process_bug.cgi';
    debug_print("Bug processed.");

    my $added_comment;
    if (trim($fields{'comment'})) {
        # The "old" bug object doesn't contain the comment we just added.
        $added_comment = Bugzilla::Bug->check($bug_id)->comments->[-1];
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
        debug_print("Inserting Attachment: " . Dumper($attachment), 3);
        my $type = $attachment->content_type || 'application/octet-stream';
        # MUAs add stuff like "name=" to content-type that we really don't
        # want.
        $type =~ s/;.*//;
        my $obj = Bugzilla::Attachment->create({
            bug         => $bug,
            description => $attachment->filename(1),
            filename    => $attachment->filename(1),
            mimetype    => $type,
            data        => $attachment->body,
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
    debug_print("Splitting Body and Attachments [Type: $ct]...", 2);

    my ($bodies, $attachments) = split_body_and_attachments($email);
    debug_print(scalar(@$bodies) . " body part(s) and " . scalar(@$attachments)
                . " attachment part(s).");
    debug_print('Bodies: ' . Dumper($bodies), 3);

    # Get the first part of the email that contains a text body,
    # and make all the other pieces into attachments. (This handles
    # people or MUAs who accidentally attach text files as an "inline"
    # attachment.)
    my $body;
    while (@$bodies) {
        my $possible = shift @$bodies;
        $body = get_text_alternative($possible);
        if (defined $body) {
            unshift(@$attachments, @$bodies);
            last;
        }
    }

    if (!defined $body) {
        # Note that this only happens if the email does not contain any
        # text/plain parts. If the email has an empty text/plain part,
        # you're fine, and this message does NOT get thrown.
        ThrowUserError('email_no_body');
    }

    debug_print("Picked Body:\n$body", 2);
    
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
        debug_print("Alternative Part Content-Type: $ct", 2);
        debug_print("Alternative Part Character Encoding: $charset", 2);
        # If we find a text/plain body here, return it immediately.
        if (!$ct || $ct =~ m{^text/plain}i) {
            return _decode_body($charset, $part->body);
        }
        # If we find a text/html body, decode it, but don't return
        # it immediately, because there might be a text/plain alternative
        # later. This could be any HTML type.
        if ($ct =~ m{^application/xhtml\+xml}i or $ct =~ m{text/html}i) {
            my $parser = HTML::FormatText::WithLinks->new(
                # Put footnnote indicators after the text, not before it.
                before_link => '',
                after_link  => '[%n]',
                # Convert bold and italics, use "*" for bold instead of "_".
                with_emphasis => 1,
                bold_marker => '*',
                # If the same link appears multiple times, only create
                # one footnote.
                unique_links => 1,
                # If the link text is the URL, don't create a footnote.
                skip_linked_urls => 1,
            );
            $body = _decode_body($charset, $part->body);
            $body = $parser->parse($body);
        }
    }

    return $body;
}

sub _decode_body {
    my ($charset, $body) = @_;
    if (Bugzilla->params->{'utf8'} && !utf8::is_utf8($body)) {
        return Encode::decode($charset, $body);
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

sub split_body_and_attachments {
    my ($email) = @_;

    my (@body, @attachments);
    foreach my $part ($email->parts) {
        my $ct = lc($part->content_type || 'text/plain');
        my $disposition = lc($part->header('Content-Disposition') || 'inline');
        # Remove the charset, etc. from the content-type, we don't care here.
        $ct =~ s/;.*//;
        debug_print("Part Content-Type: [$ct]", 2);
        debug_print("Part Disposition: [$disposition]", 2);

        if ($disposition eq 'inline' and grep($_ eq $ct, BODY_TYPES)) {
            push(@body, $part);
            next;
        }
        
        if (scalar($part->parts) == 1) {
            push(@attachments, $part);
            next;
        }

        # If this part has sub-parts, analyze them similarly to how we
        # did above and return the relevant pieces.
        my ($add_body, $add_attachments) = split_body_and_attachments($part);
        push(@body, @$add_body);
        push(@attachments, @$add_attachments);
    }

    return (\@body, \@attachments);
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
    die @_ if ($^S // Bugzilla::Error::_in_eval());

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

You cannot modify Flags through the email interface.
