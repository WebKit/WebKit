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
#                 Myk Melez <myk@mozilla.org>
#                 Marc Schumann <wurblzap@gmail.com>
#                 Frédéric Buclin <LpSolit@gmail.com>

use strict;

package Bugzilla::Attachment;

=head1 NAME

Bugzilla::Attachment - Bugzilla attachment class.

=head1 SYNOPSIS

  use Bugzilla::Attachment;

  # Get the attachment with the given ID.
  my $attachment = new Bugzilla::Attachment($attach_id);

  # Get the attachments with the given IDs.
  my $attachments = Bugzilla::Attachment->new_from_list($attach_ids);

=head1 DESCRIPTION

Attachment.pm represents an attachment object. It is an implementation
of L<Bugzilla::Object>, and thus provides all methods that
L<Bugzilla::Object> provides.

The methods that are specific to C<Bugzilla::Attachment> are listed
below.

=cut

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Flag;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Field;
use Bugzilla::Hook;

use File::Copy;
use List::Util qw(max);

use base qw(Bugzilla::Object);

###############################
####    Initialization     ####
###############################

use constant DB_TABLE   => 'attachments';
use constant ID_FIELD   => 'attach_id';
use constant LIST_ORDER => ID_FIELD;
# Attachments are tracked in bugs_activity.
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;

sub DB_COLUMNS {
    my $dbh = Bugzilla->dbh;

    return qw(
        attach_id
        bug_id
        description
        filename
        isobsolete
        ispatch
        isprivate
        mimetype
        modification_time
        submitter_id),
        $dbh->sql_date_format('attachments.creation_ts', '%Y.%m.%d %H:%i') . ' AS creation_ts';
}

use constant REQUIRED_FIELD_MAP => {
    bug_id => 'bug',
};
use constant EXTRA_REQUIRED_FIELDS => qw(data);

use constant UPDATE_COLUMNS => qw(
    description
    filename
    isobsolete
    ispatch
    isprivate
    mimetype
);

use constant VALIDATORS => {
    bug           => \&_check_bug,
    description   => \&_check_description,
    filename      => \&_check_filename,
    ispatch       => \&Bugzilla::Object::check_boolean,
    isprivate     => \&_check_is_private,
    mimetype      => \&_check_content_type,
};

use constant VALIDATOR_DEPENDENCIES => {
    mimetype => ['ispatch'],
};

use constant UPDATE_VALIDATORS => {
    isobsolete => \&Bugzilla::Object::check_boolean,
};

###############################
####      Accessors      ######
###############################

=pod

=head2 Instance Properties

=over

=item C<bug_id>

the ID of the bug to which the attachment is attached

=back

=cut

sub bug_id {
    my $self = shift;
    return $self->{bug_id};
}

=over

=item C<bug>

the bug object to which the attachment is attached

=back

=cut

sub bug {
    my $self = shift;

    require Bugzilla::Bug;
    $self->{bug} ||= Bugzilla::Bug->new($self->bug_id);
    return $self->{bug};
}

=over

=item C<description>

user-provided text describing the attachment

=back

=cut

sub description {
    my $self = shift;
    return $self->{description};
}

=over

=item C<contenttype>

the attachment's MIME media type

=back

=cut

sub contenttype {
    my $self = shift;
    return $self->{mimetype};
}

=over

=item C<attacher>

the user who attached the attachment

=back

=cut

sub attacher {
    my $self = shift;
    return $self->{attacher} if exists $self->{attacher};
    $self->{attacher} = new Bugzilla::User($self->{submitter_id});
    return $self->{attacher};
}

=over

=item C<attached>

the date and time on which the attacher attached the attachment

=back

=cut

sub attached {
    my $self = shift;
    return $self->{creation_ts};
}

=over

=item C<modification_time>

the date and time on which the attachment was last modified.

=back

=cut

sub modification_time {
    my $self = shift;
    return $self->{modification_time};
}

=over

=item C<filename>

the name of the file the attacher attached

=back

=cut

sub filename {
    my $self = shift;
    return $self->{filename};
}

=over

=item C<ispatch>

whether or not the attachment is a patch

=back

=cut

sub ispatch {
    my $self = shift;
    return $self->{ispatch};
}

=over

=item C<isobsolete>

whether or not the attachment is obsolete

=back

=cut

sub isobsolete {
    my $self = shift;
    return $self->{isobsolete};
}

=over

=item C<isprivate>

whether or not the attachment is private

=back

=cut

sub isprivate {
    my $self = shift;
    return $self->{isprivate};
}

=over

=item C<is_viewable>

Returns 1 if the attachment has a content-type viewable in this browser.
Note that we don't use $cgi->Accept()'s ability to check if a content-type
matches, because this will return a value even if it's matched by the generic
*/* which most browsers add to the end of their Accept: headers.

=back

=cut

sub is_viewable {
    my $self = shift;
    my $contenttype = $self->contenttype;
    my $cgi = Bugzilla->cgi;

    # We assume we can view all text and image types.
    return 1 if ($contenttype =~ /^(text|image)\//);

    # Mozilla can view XUL. Note the trailing slash on the Gecko detection to
    # avoid sending XUL to Safari.
    return 1 if (($contenttype =~ /^application\/vnd\.mozilla\./)
                 && ($cgi->user_agent() =~ /Gecko\//));

    # If it's not one of the above types, we check the Accept: header for any
    # types mentioned explicitly.
    my $accept = join(",", $cgi->Accept());
    return 1 if ($accept =~ /^(.*,)?\Q$contenttype\E(,.*)?$/);

    return 0;
}

=over

=item C<data>

the content of the attachment

=back

=cut

sub data {
    my $self = shift;
    return $self->{data} if exists $self->{data};

    # First try to get the attachment data from the database.
    ($self->{data}) = Bugzilla->dbh->selectrow_array("SELECT thedata
                                                      FROM attach_data
                                                      WHERE id = ?",
                                                     undef,
                                                     $self->id);

    # If there's no attachment data in the database, the attachment is stored
    # in a local file, so retrieve it from there.
    if (length($self->{data}) == 0) {
        if (open(AH, $self->_get_local_filename())) {
            local $/;
            binmode AH;
            $self->{data} = <AH>;
            close(AH);
        }
    }

    return $self->{data};
}

=over

=item C<datasize>

the length (in characters) of the attachment content

=back

=cut

# datasize is a property of the data itself, and it's unclear whether we should
# expose it at all, since you can easily derive it from the data itself: in TT,
# attachment.data.size; in Perl, length($attachment->{data}).  But perhaps
# it makes sense for performance reasons, since accessing the data forces it
# to get retrieved from the database/filesystem and loaded into memory,
# while datasize avoids loading the attachment into memory, calling SQL's
# LENGTH() function or stat()ing the file instead.  I've left it in for now.

sub datasize {
    my $self = shift;
    return $self->{datasize} if exists $self->{datasize};

    # If we have already retrieved the data, return its size.
    return length($self->{data}) if exists $self->{data};

    $self->{datasize} =
        Bugzilla->dbh->selectrow_array("SELECT LENGTH(thedata)
                                        FROM attach_data
                                        WHERE id = ?",
                                       undef, $self->id) || 0;

    # If there's no attachment data in the database, either the attachment
    # is stored in a local file, and so retrieve its size from the file,
    # or the attachment has been deleted.
    unless ($self->{datasize}) {
        if (open(AH, $self->_get_local_filename())) {
            binmode AH;
            $self->{datasize} = (stat(AH))[7];
            close(AH);
        }
    }

    return $self->{datasize};
}

sub _get_local_filename {
    my $self = shift;
    my $hash = ($self->id % 100) + 100;
    $hash =~ s/.*(\d\d)$/group.$1/;
    return bz_locations()->{'attachdir'} . "/$hash/attachment." . $self->id;
}

=over

=item C<flags>

flags that have been set on the attachment

=back

=cut

sub flags {
    my $self = shift;

    # Don't cache it as it must be in sync with ->flag_types.
    $self->{flags} = [map { @{$_->{flags}} } @{$self->flag_types}];
    return $self->{flags};
}

=over

=item C<flag_types>

Return all flag types available for this attachment as well as flags
already set, grouped by flag type.

=back

=cut

sub flag_types {
    my $self = shift;
    return $self->{flag_types} if exists $self->{flag_types};

    my $vars = { target_type  => 'attachment',
                 product_id   => $self->bug->product_id,
                 component_id => $self->bug->component_id,
                 attach_id    => $self->id };

    $self->{flag_types} = Bugzilla::Flag->_flag_types($vars);
    return $self->{flag_types};
}

###############################
####      Validators     ######
###############################

sub set_content_type { $_[0]->set('mimetype', $_[1]); }
sub set_description  { $_[0]->set('description', $_[1]); }
sub set_filename     { $_[0]->set('filename', $_[1]); }
sub set_is_patch     { $_[0]->set('ispatch', $_[1]); }
sub set_is_private   { $_[0]->set('isprivate', $_[1]); }

sub set_is_obsolete  {
    my ($self, $obsolete) = @_;

    my $old = $self->isobsolete;
    $self->set('isobsolete', $obsolete);
    my $new = $self->isobsolete;

    # If the attachment is being marked as obsolete, cancel pending requests.
    if ($new && $old != $new) {
        my @requests = grep { $_->status eq '?' } @{$self->flags};
        return unless scalar @requests;

        my %flag_ids = map { $_->id => 1 } @requests;
        foreach my $flagtype (@{$self->flag_types}) {
            @{$flagtype->{flags}} = grep { !$flag_ids{$_->id} } @{$flagtype->{flags}};
        }
    }
}

sub set_flags {
    my ($self, $flags, $new_flags) = @_;

    Bugzilla::Flag->set_flag($self, $_) foreach (@$flags, @$new_flags);
}

sub _check_bug {
    my ($invocant, $bug) = @_;
    my $user = Bugzilla->user;

    $bug = ref $invocant ? $invocant->bug : $bug;

    $bug || ThrowCodeError('param_required', 
                           { function => "$invocant->create", param => 'bug' });

    ($user->can_see_bug($bug->id) && $user->can_edit_product($bug->product_id))
      || ThrowUserError("illegal_attachment_edit_bug", { bug_id => $bug->id });

    return $bug;
}

sub _check_content_type {
    my ($invocant, $content_type, undef, $params) = @_;
 
    my $is_patch = ref($invocant) ? $invocant->ispatch : $params->{ispatch};
    $content_type = 'text/plain' if $is_patch;
    $content_type = clean_text($content_type);
    # The subsets below cover all existing MIME types and charsets registered by IANA.
    # (MIME type: RFC 2045 section 5.1; charset: RFC 2278 section 3.3)
    my $legal_types = join('|', LEGAL_CONTENT_TYPES);
    if (!$content_type
        || $content_type !~ /^($legal_types)\/[a-z0-9_\-\+\.]+(;\s*charset=[a-z0-9_\-\+]+)?$/i)
    {
        ThrowUserError("invalid_content_type", { contenttype => $content_type });
    }
    trick_taint($content_type);

    return $content_type;
}

sub _check_data {
    my ($invocant, $params) = @_;

    my $data = $params->{data};
    $params->{filesize} = ref $data ? -s $data : length($data);

    Bugzilla::Hook::process('attachment_process_data', { data       => \$data,
                                                         attributes => $params });

    $params->{filesize} || ThrowUserError('zero_length_file');
    # Make sure the attachment does not exceed the maximum permitted size.
    my $max_size = max(Bugzilla->params->{'maxlocalattachment'} * 1048576,
                       Bugzilla->params->{'maxattachmentsize'} * 1024);

    if ($params->{filesize} > $max_size) {
        my $vars = { filesize => sprintf("%.0f", $params->{filesize}/1024) };
        ThrowUserError('file_too_large', $vars);
    }
    return $data;
}

sub _check_description {
    my ($invocant, $description) = @_;

    $description = trim($description);
    $description || ThrowUserError('missing_attachment_description');
    return $description;
}

sub _check_filename {
    my ($invocant, $filename) = @_;

    $filename = clean_text($filename);
    if (!$filename) {
        if (ref $invocant) {
            ThrowUserError('filename_not_specified');
        }
        else {
            ThrowUserError('file_not_specified');
        }
    }

    # Remove path info (if any) from the file name.  The browser should do this
    # for us, but some are buggy.  This may not work on Mac file names and could
    # mess up file names with slashes in them, but them's the breaks.  We only
    # use this as a hint to users downloading attachments anyway, so it's not
    # a big deal if it munges incorrectly occasionally.
    $filename =~ s/^.*[\/\\]//;

    # Truncate the filename to 100 characters, counting from the end of the
    # string to make sure we keep the filename extension.
    $filename = substr($filename, -100, 100);
    trick_taint($filename);

    return $filename;
}

sub _check_is_private {
    my ($invocant, $is_private) = @_;

    $is_private = $is_private ? 1 : 0;
    if (((!ref $invocant && $is_private)
         || (ref $invocant && $invocant->isprivate != $is_private))
        && !Bugzilla->user->is_insider) {
        ThrowUserError('user_not_insider');
    }
    return $is_private;
}

=pod

=head2 Class Methods

=over

=item C<get_attachments_by_bug($bug_id)>

Description: retrieves and returns the attachments the currently logged in
             user can view for the given bug.

Params:     C<$bug_id> - integer - the ID of the bug for which
            to retrieve and return attachments.

Returns:    a reference to an array of attachment objects.

=cut

sub get_attachments_by_bug {
    my ($class, $bug_id, $vars) = @_;
    my $user = Bugzilla->user;
    my $dbh = Bugzilla->dbh;

    # By default, private attachments are not accessible, unless the user
    # is in the insider group or submitted the attachment.
    my $and_restriction = '';
    my @values = ($bug_id);

    unless ($user->is_insider) {
        $and_restriction = 'AND (isprivate = 0 OR submitter_id = ?)';
        push(@values, $user->id);
    }

    my $attach_ids = $dbh->selectcol_arrayref("SELECT attach_id FROM attachments
                                               WHERE bug_id = ? $and_restriction",
                                               undef, @values);

    my $attachments = Bugzilla::Attachment->new_from_list($attach_ids);

    # To avoid $attachment->flags to run SQL queries itself for each
    # attachment listed here, we collect all the data at once and
    # populate $attachment->{flags} ourselves.
    if ($vars->{preload}) {
        $_->{flags} = [] foreach @$attachments;
        my %att = map { $_->id => $_ } @$attachments;

        my $flags = Bugzilla::Flag->match({ bug_id      => $bug_id,
                                            target_type => 'attachment' });

        # Exclude flags for private attachments you cannot see.
        @$flags = grep {exists $att{$_->attach_id}} @$flags;

        push(@{$att{$_->attach_id}->{flags}}, $_) foreach @$flags;
        $attachments = [sort {$a->id <=> $b->id} values %att];
    }
    return $attachments;
}

=pod

=item C<validate_can_edit($attachment, $product_id)>

Description: validates if the user is allowed to view and edit the attachment.
             Only the submitter or someone with editbugs privs can edit it.
             Only the submitter and users in the insider group can view
             private attachments.

Params:      $attachment - the attachment object being edited.
             $product_id - the product ID the attachment belongs to.

Returns:     1 on success, 0 otherwise.

=cut

sub validate_can_edit {
    my ($attachment, $product_id) = @_;
    my $user = Bugzilla->user;

    # The submitter can edit their attachments.
    return ($attachment->attacher->id == $user->id
            || ((!$attachment->isprivate || $user->is_insider)
                 && $user->in_group('editbugs', $product_id))) ? 1 : 0;
}

=item C<validate_obsolete($bug, $attach_ids)>

Description: validates if attachments the user wants to mark as obsolete
             really belong to the given bug and are not already obsolete.
             Moreover, a user cannot mark an attachment as obsolete if
             he cannot view it (due to restrictions on it).

Params:      $bug - The bug object obsolete attachments should belong to.
             $attach_ids - The list of attachments to mark as obsolete.

Returns:     The list of attachment objects to mark as obsolete.
             Else an error is thrown.

=cut

sub validate_obsolete {
    my ($class, $bug, $list) = @_;

    # Make sure the attachment id is valid and the user has permissions to view
    # the bug to which it is attached. Make sure also that the user can view
    # the attachment itself.
    my @obsolete_attachments;
    foreach my $attachid (@$list) {
        my $vars = {};
        $vars->{'attach_id'} = $attachid;

        detaint_natural($attachid)
          || ThrowCodeError('invalid_attach_id_to_obsolete', $vars);

        # Make sure the attachment exists in the database.
        my $attachment = new Bugzilla::Attachment($attachid)
          || ThrowUserError('invalid_attach_id', $vars);

        # Check that the user can view and edit this attachment.
        $attachment->validate_can_edit($bug->product_id)
          || ThrowUserError('illegal_attachment_edit', { attach_id => $attachment->id });

        $vars->{'description'} = $attachment->description;

        if ($attachment->bug_id != $bug->bug_id) {
            $vars->{'my_bug_id'} = $bug->bug_id;
            $vars->{'attach_bug_id'} = $attachment->bug_id;
            ThrowCodeError('mismatched_bug_ids_on_obsolete', $vars);
        }

        next if $attachment->isobsolete;

        push(@obsolete_attachments, $attachment);
    }
    return @obsolete_attachments;
}

###############################
####     Constructors     #####
###############################

=pod

=item C<create>

Description: inserts an attachment into the given bug.

Params:     takes a hashref with the following keys:
            C<bug> - Bugzilla::Bug object - the bug for which to insert
            the attachment.
            C<data> - Either a filehandle pointing to the content of the
            attachment, or the content of the attachment itself.
            C<description> - string - describe what the attachment is about.
            C<filename> - string - the name of the attachment (used by the
            browser when downloading it). If the attachment is a URL, this
            parameter has no effect.
            C<mimetype> - string - a valid MIME type.
            C<creation_ts> - string (optional) - timestamp of the insert
            as returned by SELECT LOCALTIMESTAMP(0).
            C<ispatch> - boolean (optional, default false) - true if the
            attachment is a patch.
            C<isprivate> - boolean (optional, default false) - true if
            the attachment is private.

Returns:    The new attachment object.

=cut

sub create {
    my $class = shift;
    my $dbh = Bugzilla->dbh;

    $class->check_required_create_fields(@_);
    my $params = $class->run_create_validators(@_);

    # Extract everything which is not a valid column name.
    my $bug = delete $params->{bug};
    $params->{bug_id} = $bug->id;
    my $data = delete $params->{data};
    my $size = delete $params->{filesize};

    my $attachment = $class->insert_create_data($params);
    my $attachid = $attachment->id;

    # The file is too large to be stored in the DB, so we store it locally.
    if ($size > Bugzilla->params->{'maxattachmentsize'} * 1024) {
        my $attachdir = bz_locations()->{'attachdir'};
        my $hash = ($attachid % 100) + 100;
        $hash =~ s/.*(\d\d)$/group.$1/;
        mkdir "$attachdir/$hash", 0770;
        chmod 0770, "$attachdir/$hash";
        if (ref $data) {
            copy($data, "$attachdir/$hash/attachment.$attachid");
            close $data;
        }
        else {
            open(AH, '>', "$attachdir/$hash/attachment.$attachid");
            binmode AH;
            print AH $data;
            close AH;
        }
        $data = ''; # Will be stored in the DB.
    }
    # If we have a filehandle, we need its content to store it in the DB.
    elsif (ref $data) {
        local $/;
        # Store the content in a temp variable while we close the FH.
        my $tmp = <$data>;
        close $data;
        $data = $tmp;
    }

    my $sth = $dbh->prepare("INSERT INTO attach_data
                             (id, thedata) VALUES ($attachid, ?)");

    trick_taint($data);
    $sth->bind_param(1, $data, $dbh->BLOB_TYPE);
    $sth->execute();

    $attachment->{bug} = $bug;

    # Return the new attachment object.
    return $attachment;
}

sub run_create_validators {
    my ($class, $params) = @_;

    # Let's validate the attachment content first as it may
    # alter some other attachment attributes.
    $params->{data} = $class->_check_data($params);
    $params = $class->SUPER::run_create_validators($params);

    $params->{creation_ts} ||= Bugzilla->dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');
    $params->{modification_time} = $params->{creation_ts};
    $params->{submitter_id} = Bugzilla->user->id || ThrowCodeError('invalid_user');

    return $params;
}

sub update {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $user = Bugzilla->user;
    my $timestamp = shift || $dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');

    my ($changes, $old_self) = $self->SUPER::update(@_);

    my ($removed, $added) = Bugzilla::Flag->update_flags($self, $old_self, $timestamp);
    if ($removed || $added) {
        $changes->{'flagtypes.name'} = [$removed, $added];
    }

    # Record changes in the activity table.
    my $sth = $dbh->prepare('INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                                        fieldid, removed, added)
                             VALUES (?, ?, ?, ?, ?, ?, ?)');

    foreach my $field (keys %$changes) {
        my $change = $changes->{$field};
        $field = "attachments.$field" unless $field eq "flagtypes.name";
        my $fieldid = get_field_id($field);
        $sth->execute($self->bug_id, $self->id, $user->id, $timestamp,
                      $fieldid, $change->[0], $change->[1]);
    }

    if (scalar(keys %$changes)) {
      $dbh->do('UPDATE attachments SET modification_time = ? WHERE attach_id = ?',
               undef, ($timestamp, $self->id));
      $dbh->do('UPDATE bugs SET delta_ts = ? WHERE bug_id = ?',
               undef, ($timestamp, $self->bug_id));
    }

    return $changes;
}

=pod

=item C<remove_from_db()>

Description: removes an attachment from the DB.

Params:     none

Returns:    nothing

=back

=cut

sub remove_from_db {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();
    $dbh->do('DELETE FROM flags WHERE attach_id = ?', undef, $self->id);
    $dbh->do('DELETE FROM attach_data WHERE id = ?', undef, $self->id);
    $dbh->do('UPDATE attachments SET mimetype = ?, ispatch = ?, isobsolete = ?
              WHERE attach_id = ?', undef, ('text/plain', 0, 1, $self->id));
    $dbh->bz_commit_transaction();
}

###############################
####       Helpers        #####
###############################

# Extract the content type from the attachment form.
sub get_content_type {
    my $cgi = Bugzilla->cgi;

    return 'text/plain' if ($cgi->param('ispatch') || $cgi->param('attach_text'));

    my $content_type;
    if (!defined $cgi->param('contenttypemethod')) {
        ThrowUserError("missing_content_type_method");
    }
    elsif ($cgi->param('contenttypemethod') eq 'autodetect') {
        defined $cgi->upload('data') || ThrowUserError('file_not_specified');
        # The user asked us to auto-detect the content type, so use the type
        # specified in the HTTP request headers.
        $content_type =
            $cgi->uploadInfo($cgi->param('data'))->{'Content-Type'};
        $content_type || ThrowUserError("missing_content_type");

        # Set the ispatch flag to 1 if the content type
        # is text/x-diff or text/x-patch
        if ($content_type =~ m{text/x-(?:diff|patch)}) {
            $cgi->param('ispatch', 1);
            $content_type = 'text/plain';
        }

        # Internet Explorer sends image/x-png for PNG images,
        # so convert that to image/png to match other browsers.
        if ($content_type eq 'image/x-png') {
            $content_type = 'image/png';
        }
    }
    elsif ($cgi->param('contenttypemethod') eq 'list') {
        # The user selected a content type from the list, so use their
        # selection.
        $content_type = $cgi->param('contenttypeselection');
    }
    elsif ($cgi->param('contenttypemethod') eq 'manual') {
        # The user entered a content type manually, so use their entry.
        $content_type = $cgi->param('contenttypeentry');
    }
    else {
        ThrowCodeError("illegal_content_type_method",
                       { contenttypemethod => $cgi->param('contenttypemethod') });
    }
    return $content_type;
}


1;
