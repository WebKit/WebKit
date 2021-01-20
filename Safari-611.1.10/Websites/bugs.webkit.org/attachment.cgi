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
use Bugzilla::BugMail;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Flag; 
use Bugzilla::FlagType; 
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Bug;
use Bugzilla::Attachment;
use Bugzilla::Attachment::PatchReader;
use Bugzilla::Token;

use Encode qw(encode find_encoding);
use Encode::MIME::Header; # Required to alter Encode::Encoding{'MIME-Q'}.
#if WEBKIT_CHANGES
use Apache2::SubProcess ();
use Apache2::RequestUtil ();
#endif // WEBKIT_CHANGES

# For most scripts we don't make $cgi and $template global variables. But
# when preparing Bugzilla for mod_perl, this script used these
# variables in so many subroutines that it was easier to just
# make them globals.
local our $cgi = Bugzilla->cgi;
local our $template = Bugzilla->template;
local our $vars = {};
local $Bugzilla::CGI::ALLOW_UNSAFE_RESPONSE = 1;

# All calls to this script should contain an "action" variable whose
# value determines what the user wants to do.  The code below checks
# the value of that variable and runs the appropriate code. If none is
# supplied, we default to 'view'.

# Determine whether to use the action specified by the user or the default.
my $action = $cgi->param('action') || 'view';
my $format = $cgi->param('format') || '';

# You must use the appropriate urlbase/sslbase param when doing anything
# but viewing an attachment, or a raw diff.
if ($action ne 'view'
    && (($action !~ /^(?:interdiff|diff)$/) || $format ne 'raw'))
{
    do_ssl_redirect_if_required();
    if ($cgi->url_is_attachment_base) {
        $cgi->redirect_to_urlbase;
    }
    Bugzilla->login();
}

# When viewing an attachment, do not request credentials if we are on
# the alternate host. Let view() decide when to call Bugzilla->login.
if ($action eq "view")
{
    view();
}
elsif ($action eq "interdiff")
{
    interdiff();
}
elsif ($action eq "diff")
{
    diff();
}
elsif ($action eq "viewall") 
{ 
    viewall(); 
}
elsif ($action eq "enter") 
{ 
    Bugzilla->login(LOGIN_REQUIRED);
    enter(); 
}
elsif ($action eq "insert")
{
    Bugzilla->login(LOGIN_REQUIRED);
    insert();
}
elsif ($action eq "edit") 
{ 
    edit(); 
}
#if WEBKIT_CHANGES
elsif ($action eq "review")
{
    prettyPatch();
}
elsif ($action eq "reviewform")
{
    edit("reviewform");
}
#endif // WEBKIT_CHANGES
elsif ($action eq "update") 
{ 
    Bugzilla->login(LOGIN_REQUIRED);
    update();
}
#if WEBKIT_CHANGES
elsif ($action eq "prettypatch")
{
    prettyPatch();
}
#endif // WEBKIT_CHANGES
elsif ($action eq "delete") {
    delete_attachment();
}
else 
{ 
  ThrowUserError('unknown_action', {action => $action});
}

exit;

################################################################################
# Data Validation / Security Authorization
################################################################################

# Validates an attachment ID. Optionally takes a parameter of a form
# variable name that contains the ID to be validated. If not specified,
# uses 'id'.
# If the second parameter is true, the attachment ID will be validated,
# however the current user's access to the attachment will not be checked.
# Will throw an error if 1) attachment ID is not a valid number,
# 2) attachment does not exist, or 3) user isn't allowed to access the
# attachment.
#
# Returns an attachment object.

sub validateID {
    my($param, $dont_validate_access) = @_;
    $param ||= 'id';

    # If we're not doing interdiffs, check if id wasn't specified and
    # prompt them with a page that allows them to choose an attachment.
    # Happens when calling plain attachment.cgi from the urlbar directly
    if ($param eq 'id' && !$cgi->param('id')) {
        print $cgi->header();
        $template->process("attachment/choose.html.tmpl", $vars) ||
            ThrowTemplateError($template->error());
        exit;
    }
    
    my $attach_id = $cgi->param($param);

    # Validate the specified attachment id. detaint kills $attach_id if
    # non-natural, so use the original value from $cgi in our exception
    # message here.
    detaint_natural($attach_id)
        || ThrowUserError("invalid_attach_id",
                          { attach_id => scalar $cgi->param($param) });
  
    # Make sure the attachment exists in the database.
    my $attachment = new Bugzilla::Attachment({ id => $attach_id, cache => 1 })
        || ThrowUserError("invalid_attach_id", { attach_id => $attach_id });

    return $attachment if ($dont_validate_access || check_can_access($attachment));
}

# Make sure the current user has access to the specified attachment.
sub check_can_access {
    my $attachment = shift;
    my $user = Bugzilla->user;

    # Make sure the user is authorized to access this attachment's bug.
    Bugzilla::Bug->check({ id => $attachment->bug_id, cache => 1 });
    if ($attachment->isprivate && $user->id != $attachment->attacher->id 
        && !$user->is_insider) 
    {
        ThrowUserError('auth_failure', {action => 'access',
                                        object => 'attachment',
                                        attach_id => $attachment->id});
    }
    return 1;
}

# Determines if the attachment is public -- that is, if users who are
# not logged in have access to the attachment
sub attachmentIsPublic {
    my $attachment = shift;

    return 0 if Bugzilla->params->{'requirelogin'};
    return 0 if $attachment->isprivate;

    my $anon_user = new Bugzilla::User;
    return $anon_user->can_see_bug($attachment->bug_id);
}

# Validates format of a diff/interdiff. Takes a list as an parameter, which
# defines the valid format values. Will throw an error if the format is not
# in the list. Returns either the user selected or default format.
sub validateFormat {
  # receives a list of legal formats; first item is a default
  my $format = $cgi->param('format') || $_[0];
  if (not grep($_ eq $format, @_)) {
     ThrowUserError("invalid_format", { format  => $format, formats => \@_ });
  }

  return $format;
}

# Gets the attachment object(s) generated by validateID, while ensuring
# attachbase and token authentication is used when required.
sub get_attachment {
    my @field_names = @_ ? @_ : qw(id);

    my %attachments;

    if (use_attachbase()) {
        # Load each attachment, and ensure they are all from the same bug
        my $bug_id = 0;
        foreach my $field_name (@field_names) {
            my $attachment = validateID($field_name, 1);
            if (!$bug_id) {
                $bug_id = $attachment->bug_id;
            } elsif ($attachment->bug_id != $bug_id) {
                ThrowUserError('attachment_bug_id_mismatch');
            }
            $attachments{$field_name} = $attachment;
        }
        my @args = map { $_ . '=' . $attachments{$_}->id } @field_names;
        my $cgi_params = $cgi->canonicalise_query(@field_names, 't',
            'Bugzilla_login', 'Bugzilla_password');
        push(@args, $cgi_params) if $cgi_params;
        my $path = 'attachment.cgi?' . join('&', @args);

        # Make sure the attachment is served from the correct server.
        if ($cgi->url_is_attachment_base($bug_id)) {
            # No need to validate the token for public attachments. We cannot request
            # credentials as we are on the alternate host.
            if (!all_attachments_are_public(\%attachments)) {
                my $token = $cgi->param('t');
                my ($userid, undef, $token_data) = Bugzilla::Token::GetTokenData($token);
                my %token_data = unpack_token_data($token_data);
                my $valid_token = 1;
                foreach my $field_name (@field_names) {
                    my $token_id = $token_data{$field_name};
                    if (!$token_id
                        || !detaint_natural($token_id)
                        || $attachments{$field_name}->id != $token_id)
                    {
                        $valid_token = 0;
                        last;
                    }
                }
                unless ($userid && $valid_token) {
                    # Not a valid token.
                    print $cgi->redirect('-location' => correct_urlbase() . $path);
                    exit;
                }
                # Change current user without creating cookies.
                Bugzilla->set_user(new Bugzilla::User($userid));
                # Tokens are single use only, delete it.
                delete_token($token);
            }
        }
        elsif ($cgi->url_is_attachment_base) {
            # If we come here, this means that each bug has its own host
            # for attachments, and that we are trying to view one attachment
            # using another bug's host. That's not desired.
            $cgi->redirect_to_urlbase;
        }
        else {
            # We couldn't call Bugzilla->login earlier as we first had to
            # make sure we were not going to request credentials on the
            # alternate host.
            Bugzilla->login();
            my $attachbase = Bugzilla->params->{'attachment_base'};
            # Replace %bugid% by the ID of the bug the attachment 
            # belongs to, if present.
            $attachbase =~ s/\%bugid\%/$bug_id/;
            if (all_attachments_are_public(\%attachments)) {
                # No need for a token; redirect to attachment base.
                print $cgi->redirect(-location => $attachbase . $path);
                exit;
            } else {
                # Make sure the user can view the attachment.
                foreach my $field_name (@field_names) {
                    check_can_access($attachments{$field_name});
                }
                # Create a token and redirect.
                my $token = url_quote(issue_session_token(pack_token_data(\%attachments)));
                print $cgi->redirect(-location => $attachbase . "$path&t=$token");
                exit;
            }
        }
    } else {
        do_ssl_redirect_if_required();
        # No alternate host is used. Request credentials if required.
        Bugzilla->login();
        foreach my $field_name (@field_names) {
            $attachments{$field_name} = validateID($field_name);
        }
    }

    return wantarray
        ? map { $attachments{$_} } @field_names
        : $attachments{$field_names[0]};
}

sub all_attachments_are_public {
    my $attachments = shift;
    foreach my $field_name (keys %$attachments) {
        if (!attachmentIsPublic($attachments->{$field_name})) {
            return 0;
        }
    }
    return 1;
}

sub pack_token_data {
    my $attachments = shift;
    return join(' ', map { $_ . '=' . $attachments->{$_}->id } keys %$attachments);
}

sub unpack_token_data {
    my @token_data = split(/ /, shift || '');
    my %data;
    foreach my $token (@token_data) {
        my ($field_name, $attach_id) = split('=', $token);
        $data{$field_name} = $attach_id;
    }
    return %data;
}

################################################################################
# Functions
################################################################################

# Display an attachment.
sub view {
    my $attachment = get_attachment();

    # At this point, Bugzilla->login has been called if it had to.
    my $contenttype = $attachment->contenttype;
    my $filename = $attachment->filename;

    # Bug 111522: allow overriding content-type manually in the posted form
    # params.
    if (defined $cgi->param('content_type')) {
        $contenttype = $attachment->_check_content_type($cgi->param('content_type'));
    }

    # Return the appropriate HTTP response headers.
    $attachment->datasize || ThrowUserError("attachment_removed");

    $filename =~ s/^.*[\/\\]//;
    # escape quotes and backslashes in the filename, per RFCs 2045/822
    $filename =~ s/\\/\\\\/g; # escape backslashes
    $filename =~ s/"/\\"/g; # escape quotes

    # Avoid line wrapping done by Encode, which we don't need for HTTP
    # headers. See discussion in bug 328628 for details.
    local $Encode::Encoding{'MIME-Q'}->{'bpl'} = 10000;
    $filename = encode('MIME-Q', $filename);

    my $disposition = Bugzilla->params->{'allow_attachment_display'} ? 'inline' : 'attachment';

    # Don't send a charset header with attachments--they might not be UTF-8.
    # However, we do allow people to explicitly specify a charset if they
    # want.
    if ($contenttype !~ /\bcharset=/i) {
        # In order to prevent Apache from adding a charset, we have to send a
        # charset that's a single space.
        $cgi->charset(' ');
        if (Bugzilla->feature('detect_charset') && $contenttype =~ /^text\//) {
            my $encoding = detect_encoding($attachment->data);
            if ($encoding) {
                $cgi->charset(find_encoding($encoding)->mime_name);
            }
        }
    }
    print $cgi->header(-type=>"$contenttype; name=\"$filename\"",
                       -content_disposition=> "$disposition; filename=\"$filename\"",
                       -content_length => $attachment->datasize);
    disable_utf8();
    print $attachment->data;
}

sub interdiff {
    # Retrieve and validate parameters
    my $format = validateFormat('html', 'raw');
    my($old_attachment, $new_attachment);
    if ($format eq 'raw') {
        ($old_attachment, $new_attachment) = get_attachment('oldid', 'newid');
    } else {
        $old_attachment = validateID('oldid');
        $new_attachment = validateID('newid');
    }

    Bugzilla::Attachment::PatchReader::process_interdiff(
        $old_attachment, $new_attachment, $format);
}

#if WEBKIT_CHANGES
sub prettyPatch
{
    # Retrieve and validate parameters
    my $attachment = validateID();
    my $format = validateFormat('html', 'raw');

    # If it is not a patch, view normally.
    if (!$attachment->ispatch) {
      view();
      return;
    }

    use vars qw($cgi);
    print $cgi->header(-type => 'text/html',
                       -expires => '+3M');

    my $orig_path = $ENV{'PATH'};
    $ENV{'PATH'} = "/usr/bin:" . $ENV{'PATH'};
    my @prettyargs = ("-I", "/var/www/html/PrettyPatch", "/var/www/html/PrettyPatch/prettify.rb", "--html-exceptions");
    my $r = Apache2::RequestUtil->request;
    my ($in, $out, $err) = $r->spawn_proc_prog("/usr/bin/ruby", \@prettyargs);
    $ENV{'PATH'} = $orig_path;
    print $in $attachment->data;
    close($in);
    while (<$out>) {
        print;
    }
    close($out);
    close($err);
}
#endif // WEBKIT_CHANGES

sub diff {
    # Retrieve and validate parameters
    my $format = validateFormat('html', 'raw');
    my $attachment = $format eq 'raw' ? get_attachment() : validateID();

    # If it is not a patch, view normally.
    if (!$attachment->ispatch) {
        view();
        return;
    }

    Bugzilla::Attachment::PatchReader::process_diff($attachment, $format);
}

# Display all attachments for a given bug in a series of IFRAMEs within one
# HTML page.
sub viewall {
    # Retrieve and validate parameters
    my $bug = Bugzilla::Bug->check({ id => scalar $cgi->param('bugid'), cache => 1 });

    my $attachments = Bugzilla::Attachment->get_attachments_by_bug($bug);
    # Ignore deleted attachments.
    @$attachments = grep { $_->datasize } @$attachments;

    if ($cgi->param('hide_obsolete')) {
        @$attachments = grep { !$_->isobsolete } @$attachments;
        $vars->{'hide_obsolete'} = 1;
    }

    # Define the variables and functions that will be passed to the UI template.
    $vars->{'bug'} = $bug;
    $vars->{'attachments'} = $attachments;

    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("attachment/show-multiple.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Display a form for entering a new attachment.
sub enter {
    # Retrieve and validate parameters
    my $bug = Bugzilla::Bug->check(scalar $cgi->param('bugid'));
    my $bugid = $bug->id;
    Bugzilla::Attachment->_check_bug($bug);
    my $dbh = Bugzilla->dbh;
    my $user = Bugzilla->user;

    # Retrieve the attachments the user can edit from the database and write
    # them into an array of hashes where each hash represents one attachment.
  
    my ($can_edit, $not_private) = ('', '');
    if (!$user->in_group('editbugs', $bug->product_id)) {
        $can_edit = "AND submitter_id = " . $user->id;
    }
    if (!$user->is_insider) {
        $not_private = "AND isprivate = 0";
    }
    my $attach_ids = $dbh->selectcol_arrayref(
        "SELECT attach_id
           FROM attachments
          WHERE bug_id = ?
                AND isobsolete = 0
                $can_edit $not_private
       ORDER BY attach_id",
         undef, $bugid);

    # Define the variables and functions that will be passed to the UI template.
    $vars->{'bug'} = $bug;
    $vars->{'attachments'} = Bugzilla::Attachment->new_from_list($attach_ids);

    my $flag_types = Bugzilla::FlagType::match({
        'target_type'  => 'attachment',
        'product_id'   => $bug->product_id,
        'component_id' => $bug->component_id
    });
    $vars->{'flag_types'} = $flag_types;
    $vars->{'any_flags_requesteeble'} =
        grep { $_->is_requestable && $_->is_requesteeble } @$flag_types;
    $vars->{'token'} = issue_session_token('create_attachment');

    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("attachment/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Insert a new attachment into the database.
sub insert {
    my $dbh = Bugzilla->dbh;
    my $user = Bugzilla->user;

    $dbh->bz_start_transaction;

    # Retrieve and validate parameters
    my $bug = Bugzilla::Bug->check(scalar $cgi->param('bugid'));
    my $bugid = $bug->id;
    my ($timestamp) = $dbh->selectrow_array("SELECT NOW()");

    # Detect if the user already used the same form to submit an attachment
    my $token = trim($cgi->param('token'));
    check_token_data($token, 'create_attachment', 'index.cgi');

    # Check attachments the user tries to mark as obsolete.
    my @obsolete_attachments;
    if ($cgi->param('obsolete')) {
        my @obsolete = $cgi->param('obsolete');
        @obsolete_attachments = Bugzilla::Attachment->validate_obsolete($bug, \@obsolete);
    }

    # Must be called before create() as it may alter $cgi->param('ispatch').
    my $content_type = Bugzilla::Attachment::get_content_type();

    # Get the filehandle of the attachment.
    my $data_fh = $cgi->upload('data');
    my $attach_text = $cgi->param('attach_text');

    my $attachment = Bugzilla::Attachment->create(
        {bug           => $bug,
         creation_ts   => $timestamp,
         data          => $attach_text || $data_fh,
         description   => scalar $cgi->param('description'),
         filename      => $attach_text ? "file_$bugid.txt" : $data_fh,
         ispatch       => scalar $cgi->param('ispatch'),
         isprivate     => scalar $cgi->param('isprivate'),
         mimetype      => $content_type,
         });

    # Delete the token used to create this attachment.
    delete_token($token);

    foreach my $obsolete_attachment (@obsolete_attachments) {
        $obsolete_attachment->set_is_obsolete(1);
        $obsolete_attachment->update($timestamp);
    }

    my ($flags, $new_flags) = Bugzilla::Flag->extract_flags_from_cgi(
                                  $bug, $attachment, $vars, SKIP_REQUESTEE_ON_ERROR);
    $attachment->set_flags($flags, $new_flags);

    # Insert a comment about the new attachment into the database.
    my $comment = $cgi->param('comment');
    $comment = '' unless defined $comment;
    $bug->add_comment($comment, { isprivate => $attachment->isprivate,
                                  type => CMT_ATTACHMENT_CREATED,
                                  extra_data => $attachment->id });

    # Assign the bug to the user, if they are allowed to take it
    my $owner = "";
    if ($cgi->param('takebug') && $user->in_group('editbugs', $bug->product_id)) {
        # When taking a bug, we have to follow the workflow.
        my $bug_status = $cgi->param('bug_status') || '';
        ($bug_status) = grep { $_->name eq $bug_status }
                        @{ $bug->status->can_change_to };

        if ($bug_status && $bug_status->is_open
            && ($bug_status->name ne 'UNCONFIRMED'
                || $bug->product_obj->allows_unconfirmed))
        {
            $bug->set_bug_status($bug_status->name);
            $bug->clear_resolution();
        }
        # Make sure the person we are taking the bug from gets mail.
        $owner = $bug->assigned_to->login;
        $bug->set_assigned_to($user);
    }

    $bug->add_cc($user) if $cgi->param('addselfcc');
    $bug->update($timestamp);

    # We have to update the attachment after updating the bug, to ensure new
    # comments are available.
    $attachment->update($timestamp);

    $dbh->bz_commit_transaction;

    # Define the variables and functions that will be passed to the UI template.
    $vars->{'attachment'} = $attachment;
    # We cannot reuse the $bug object as delta_ts has eventually been updated
    # since the object was created.
    $vars->{'bugs'} = [new Bugzilla::Bug($bugid)];
    $vars->{'header_done'} = 1;
    $vars->{'contenttypemethod'} = $cgi->param('contenttypemethod');

    my $recipients = { 'changer' => $user, 'owner' => $owner };
    $vars->{'sent_bugmail'} = Bugzilla::BugMail::Send($bugid, $recipients);

    print $cgi->header();
    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("attachment/created.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
}

# Displays a form for editing attachment properties.
# Any user is allowed to access this page, unless the attachment
# is private and the user does not belong to the insider group.
# Validations are done later when the user submits changes.
sub edit {
#if WEBKIT_CHANGES
    my ($template_name) = @_;
    $template_name = $template_name || "edit";
#endif // WEBKIT_CHANGES

    my $attachment = validateID();

    my $bugattachments =
        Bugzilla::Attachment->get_attachments_by_bug($attachment->bug);

    my $any_flags_requesteeble = grep { $_->is_requestable && $_->is_requesteeble }
                                 @{ $attachment->flag_types };
    # Useful in case a flagtype is no longer requestable but a requestee
    # has been set before we turned off that bit.
    $any_flags_requesteeble ||= grep { $_->requestee_id } @{ $attachment->flags };
    $vars->{'any_flags_requesteeble'} = $any_flags_requesteeble;
    $vars->{'attachment'} = $attachment;
    $vars->{'attachments'} = $bugattachments;

#if WEBKIT_CHANGES
    if ($attachment->ispatch) {
        my $quotedpatch = $attachment->data;
        $quotedpatch =~ s/^/> /mg;
        $vars->{'quotedpatch'} = $quotedpatch;
    }
#endif // WEBKIT_CHANGES

    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("attachment/${template_name}.html.tmpl", $vars) # WEBKIT_CHANGES
        || ThrowTemplateError($template->error());
}

# Updates an attachment record. Only users with "editbugs" privileges,
# (or the original attachment's submitter) can edit the attachment.
# Users cannot edit the content of the attachment itself.
sub update {
    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    # Start a transaction in preparation for updating the attachment.
    $dbh->bz_start_transaction();

    # Retrieve and validate parameters
    my $attachment = validateID();
    my $bug = $attachment->bug;
    $attachment->_check_bug;
    my $can_edit = $attachment->validate_can_edit;

    if ($can_edit) {
        $attachment->set_description(scalar $cgi->param('description'));
        $attachment->set_is_patch(scalar $cgi->param('ispatch'));
        $attachment->set_content_type(scalar $cgi->param('contenttypeentry'));
        $attachment->set_is_obsolete(scalar $cgi->param('isobsolete'));
        $attachment->set_is_private(scalar $cgi->param('isprivate'));
        $attachment->set_filename(scalar $cgi->param('filename'));

        # Now make sure the attachment has not been edited since we loaded the page.
        my $delta_ts = $cgi->param('delta_ts');
        my $modification_time = $attachment->modification_time;

        if ($delta_ts && $delta_ts ne $modification_time) {
            datetime_from($delta_ts)
              or ThrowCodeError('invalid_timestamp', { timestamp => $delta_ts });
            ($vars->{'operations'}) = $bug->get_activity($attachment->id, $delta_ts);

            # If the modification date changed but there is no entry in
            # the activity table, this means someone commented only.
            # In this case, there is no reason to midair.
            if (scalar(@{$vars->{'operations'}})) {
                $cgi->param('delta_ts', $modification_time);
                # The token contains the old modification_time. We need a new one.
                $cgi->param('token', issue_hash_token([$attachment->id, $modification_time]));

                $vars->{'attachment'} = $attachment;

                print $cgi->header();
                # Warn the user about the mid-air collision and ask them what to do.
                $template->process("attachment/midair.html.tmpl", $vars)
                  || ThrowTemplateError($template->error());
                exit;
            }
        }
    }

    # We couldn't do this check earlier as we first had to validate attachment ID
    # and display the mid-air collision page if modification_time changed.
    my $token = $cgi->param('token');
    check_hash_token($token, [$attachment->id, $attachment->modification_time]);

    # If the user submitted a comment while editing the attachment,
    # add the comment to the bug. Do this after having validated isprivate!
    my $comment = $cgi->param('comment');
    if (defined $comment && trim($comment) ne '') {
        $bug->add_comment($comment, { isprivate => $attachment->isprivate,
                                      type => CMT_ATTACHMENT_UPDATED,
                                      extra_data => $attachment->id });
    }

    $bug->add_cc($user) if $cgi->param('addselfcc');

    my ($flags, $new_flags) =
      Bugzilla::Flag->extract_flags_from_cgi($bug, $attachment, $vars);

    if ($can_edit) {
        $attachment->set_flags($flags, $new_flags);
    }
    # Requestees can set flags targetted to them, even if they cannot
    # edit the attachment. Flag setters can edit their own flags too.
    elsif (scalar @$flags) {
        my %flag_list = map { $_->{id} => $_ } @$flags;
        my $flag_objs = Bugzilla::Flag->new_from_list([keys %flag_list]);

        my @editable_flags;
        foreach my $flag_obj (@$flag_objs) {
            if ($flag_obj->setter_id == $user->id
                || ($flag_obj->requestee_id && $flag_obj->requestee_id == $user->id))
            {
                push(@editable_flags, $flag_list{$flag_obj->id});
            }
        }

        if (scalar @editable_flags) {
            $attachment->set_flags(\@editable_flags, []);
            # Flag changes must be committed.
            $can_edit = 1;
        }
    }

    # Figure out when the changes were made.
    my $timestamp = $dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');

    # Commit the comment, if any.
    # This has to happen before updating the attachment, to ensure new comments
    # are available to $attachment->update.
    $bug->update($timestamp);

    if ($can_edit) {
        my $changes = $attachment->update($timestamp);
        # If there are changes, we updated delta_ts in the DB. We have to
        # reflect this change in the bug object.
        $bug->{delta_ts} = $timestamp if scalar(keys %$changes);
    }

    # Commit the transaction now that we are finished updating the database.
    $dbh->bz_commit_transaction();

    # Define the variables and functions that will be passed to the UI template.
    $vars->{'attachment'} = $attachment;
    $vars->{'bugs'} = [$bug];
    $vars->{'header_done'} = 1;
    $vars->{'sent_bugmail'} = 
        Bugzilla::BugMail::Send($bug->id, { 'changer' => $user });

    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("attachment/updated.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Only administrators can delete attachments.
sub delete_attachment {
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    my $dbh = Bugzilla->dbh;

    print $cgi->header();

    $user->in_group('admin')
      || ThrowUserError('auth_failure', {group  => 'admin',
                                         action => 'delete',
                                         object => 'attachment'});

    Bugzilla->params->{'allow_attachment_deletion'}
      || ThrowUserError('attachment_deletion_disabled');

    # Make sure the administrator is allowed to edit this attachment.
    my $attachment = validateID();
    Bugzilla::Attachment->_check_bug($attachment->bug);

    $attachment->datasize || ThrowUserError('attachment_removed');

    # We don't want to let a malicious URL accidentally delete an attachment.
    my $token = trim($cgi->param('token'));
    if ($token) {
        my ($creator_id, $date, $event) = Bugzilla::Token::GetTokenData($token);
        unless ($creator_id
                  && ($creator_id == $user->id)
                  && ($event eq 'delete_attachment' . $attachment->id))
        {
            # The token is invalid.
            ThrowUserError('token_does_not_exist');
        }

        my $bug = new Bugzilla::Bug($attachment->bug_id);

        # The token is valid. Delete the content of the attachment.
        my $msg;
        $vars->{'attachment'} = $attachment;
        $vars->{'reason'} = clean_text($cgi->param('reason') || '');

        $template->process("attachment/delete_reason.txt.tmpl", $vars, \$msg)
          || ThrowTemplateError($template->error());

        # Paste the reason provided by the admin into a comment.
        $bug->add_comment($msg);

        $attachment->remove_from_db();

        # Now delete the token.
        delete_token($token);

        # Insert the comment.
        $bug->update();

        # Required to display the bug the deleted attachment belongs to.
        $vars->{'bugs'} = [$bug];
        $vars->{'header_done'} = 1;

        $vars->{'sent_bugmail'} =
            Bugzilla::BugMail::Send($bug->id, { 'changer' => $user });

        $template->process("attachment/updated.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }
    else {
        # Create a token.
        $token = issue_session_token('delete_attachment' . $attachment->id);

        $vars->{'a'} = $attachment;
        $vars->{'token'} = $token;

        $template->process("attachment/confirm-delete.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }
}
