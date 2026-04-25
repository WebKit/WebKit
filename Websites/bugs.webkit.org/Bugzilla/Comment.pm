# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Comment;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::Attachment;
use Bugzilla::Comment::TagWeights;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Util;

use List::Util qw(first);
use Scalar::Util qw(blessed);

###############################
####    Initialization     ####
###############################

# Creation and updating of comments are audited in longdescs
# and bugs_activity respectively instead of audit_log.
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;

use constant DB_COLUMNS => qw(
    comment_id
    bug_id
    who
    bug_when
    work_time
    thetext
    isprivate
    already_wrapped
    type
    extra_data
);

use constant UPDATE_COLUMNS => qw(
    isprivate
    type
    extra_data
);

use constant DB_TABLE => 'longdescs';
use constant ID_FIELD => 'comment_id';
# In some rare cases, two comments can have identical timestamps. If
# this happens, we want to be sure that the comment added later shows up
# later in the sequence.
use constant LIST_ORDER => 'bug_when, comment_id';

use constant VALIDATORS => {
    bug_id      => \&_check_bug_id,
    who         => \&_check_who,
    bug_when    => \&_check_bug_when,
    work_time   => \&_check_work_time,
    thetext     => \&_check_thetext,
    isprivate   => \&_check_isprivate,
    extra_data  => \&_check_extra_data,
    type        => \&_check_type,
};

use constant VALIDATOR_DEPENDENCIES => {
    extra_data => ['type'],
    bug_id     => ['who'],
    work_time  => ['who', 'bug_id'],
    isprivate  => ['who'],
};

#########################
# Database Manipulation #
#########################

sub update {
    my $self = shift;
    my ($changes, $old_comment) = $self->SUPER::update(@_);

    if (exists $changes->{'thetext'} || exists $changes->{'isprivate'}) {
        $self->bug->_sync_fulltext( update_comments => 1);
    }

    my @old_tags = @{ $old_comment->tags };
    my @new_tags = @{ $self->tags };
    my ($removed_tags, $added_tags) = diff_arrays(\@old_tags, \@new_tags);

    if (@$removed_tags || @$added_tags) {
        my $dbh = Bugzilla->dbh;
        my $when = $dbh->selectrow_array("SELECT LOCALTIMESTAMP(0)");
        my $sth_delete = $dbh->prepare(
            "DELETE FROM longdescs_tags WHERE comment_id = ? AND tag = ?"
        );
        my $sth_insert = $dbh->prepare(
            "INSERT INTO longdescs_tags(comment_id, tag) VALUES (?, ?)"
        );
        my $sth_activity = $dbh->prepare(
            "INSERT INTO longdescs_tags_activity
            (bug_id, comment_id, who, bug_when, added, removed)
            VALUES (?, ?, ?, ?, ?, ?)"
        );

        foreach my $tag (@$removed_tags) {
            my $weighted = Bugzilla::Comment::TagWeights->new({ name => $tag });
            if ($weighted) {
                if ($weighted->weight == 1) {
                    $weighted->remove_from_db();
                } else {
                    $weighted->set_weight($weighted->weight - 1);
                    $weighted->update();
                }
            }
            trick_taint($tag);
            $sth_delete->execute($self->id, $tag);
            $sth_activity->execute(
                $self->bug_id, $self->id, Bugzilla->user->id, $when, '', $tag);
        }

        foreach my $tag (@$added_tags) {
            my $weighted = Bugzilla::Comment::TagWeights->new({ name => $tag });
            if ($weighted) {
                $weighted->set_weight($weighted->weight + 1);
                $weighted->update();
            } else {
                Bugzilla::Comment::TagWeights->create({ tag => $tag, weight => 1 });
            }
            trick_taint($tag);
            $sth_insert->execute($self->id, $tag);
            $sth_activity->execute(
                $self->bug_id, $self->id, Bugzilla->user->id, $when, $tag, '');
        }
    }

    return $changes;
}

# Speeds up displays of comment lists by loading all author objects and tags at
# once for a whole list.
sub preload {
    my ($class, $comments) = @_;
    # Author
    my %user_ids = map { $_->{who} => 1 } @$comments;
    my $users = Bugzilla::User->new_from_list([keys %user_ids]);
    my %user_map = map { $_->id => $_ } @$users;
    foreach my $comment (@$comments) {
        $comment->{author} = $user_map{$comment->{who}};
    }
    # Tags
    if (Bugzilla->params->{'comment_taggers_group'}) {
        my $dbh = Bugzilla->dbh;
        my @comment_ids = map { $_->id } @$comments;
        my %comment_map = map { $_->id => $_ } @$comments;
        my $rows = $dbh->selectall_arrayref(
            "SELECT comment_id, " . $dbh->sql_group_concat('tag', "','") . "
               FROM longdescs_tags
              WHERE " . $dbh->sql_in('comment_id', \@comment_ids) . ' ' .
              $dbh->sql_group_by('comment_id'));
        foreach my $row (@$rows) {
            $comment_map{$row->[0]}->{tags} = [ split(/,/, $row->[1]) ];
        }
        # Also sets the 'tags' attribute for comments which have no entry
        # in the longdescs_tags table, else calling $comment->tags will
        # trigger another SQL query again.
        $comment_map{$_}->{tags} ||= [] foreach @comment_ids;
    }
}

###############################
####      Accessors      ######
###############################

sub already_wrapped { return $_[0]->{'already_wrapped'}; }
sub body        { return $_[0]->{'thetext'};   }
sub bug_id      { return $_[0]->{'bug_id'};    }
sub creation_ts { return $_[0]->{'bug_when'};  }
sub is_private  { return $_[0]->{'isprivate'}; }
sub work_time   {
    # Work time is returned as a string (see bug 607909)
    return 0 if $_[0]->{'work_time'} + 0 == 0;
    return $_[0]->{'work_time'};
}
sub type        { return $_[0]->{'type'};      }
sub extra_data  { return $_[0]->{'extra_data'} }

sub tags {
    my ($self) = @_;
    state $comment_taggers_group = Bugzilla->params->{'comment_taggers_group'};
    return [] unless $comment_taggers_group;
    $self->{'tags'} ||= Bugzilla->dbh->selectcol_arrayref(
        "SELECT tag
           FROM longdescs_tags
          WHERE comment_id = ?
          ORDER BY tag",
        undef, $self->id);
    return $self->{'tags'};
}

sub collapsed {
    my ($self) = @_;
    state $comment_taggers_group = Bugzilla->params->{'comment_taggers_group'};
    return 0 unless $comment_taggers_group;
    return $self->{collapsed} if exists $self->{collapsed};

    state $collapsed_comment_tags = Bugzilla->params->{'collapsed_comment_tags'};
    $self->{collapsed} = 0;
    Bugzilla->request_cache->{comment_tags_collapsed}
            ||= [ split(/\s*,\s*/, $collapsed_comment_tags) ];
    my @collapsed_tags = @{ Bugzilla->request_cache->{comment_tags_collapsed} };
    foreach my $my_tag (@{ $self->tags }) {
        $my_tag = lc($my_tag);
        foreach my $collapsed_tag (@collapsed_tags) {
            if ($my_tag eq lc($collapsed_tag)) {
                $self->{collapsed} = 1;
                last;
            }
        }
        last if $self->{collapsed};
    }
    return $self->{collapsed};
}

sub bug {
    my $self = shift;
    require Bugzilla::Bug;
    $self->{bug} ||= new Bugzilla::Bug($self->bug_id);
    return $self->{bug};
}

sub is_about_attachment {
    my ($self) = @_;
    return 1 if ($self->type == CMT_ATTACHMENT_CREATED
                 or $self->type == CMT_ATTACHMENT_UPDATED);
    return 0;
}

sub attachment {
    my ($self) = @_;
    return undef if not $self->is_about_attachment;
    $self->{attachment} ||=
        new Bugzilla::Attachment({ id => $self->extra_data, cache => 1 });
    return $self->{attachment};
}

sub author { 
    my $self = shift;
    $self->{'author'}
      ||= new Bugzilla::User({ id => $self->{'who'}, cache => 1 });
    return $self->{'author'};
}

sub body_full {
    my ($self, $params) = @_;
    $params ||= {};
    my $template = Bugzilla->template_inner;
    my $body;
    if ($self->type) {
        $template->process("bug/format_comment.txt.tmpl", 
                           { comment => $self, %$params }, \$body)
            || ThrowTemplateError($template->error());
        $body =~ s/^X//;
    }
    else {
        $body = $self->body;
    }
    if ($params->{wrap} and !$self->already_wrapped) {
        $body = wrap_comment($body);
    }
    return $body;
}

############
# Mutators #
############

sub set_is_private  { $_[0]->set('isprivate',  $_[1]); }
sub set_type        { $_[0]->set('type',       $_[1]); }
sub set_extra_data  { $_[0]->set('extra_data', $_[1]); }

sub add_tag {
    my ($self, $tag) = @_;
    $tag = $self->_check_tag($tag);

    my $tags = $self->tags;
    return if grep { lc($tag) eq lc($_) } @$tags;
    push @$tags, $tag;
    $self->{'tags'} = [ sort @$tags ];
}

sub remove_tag {
    my ($self, $tag) = @_;
    $tag = $self->_check_tag($tag);

    my $tags = $self->tags;
    my $index = first { lc($tags->[$_]) eq lc($tag) } 0..scalar(@$tags) - 1;
    return unless defined $index;
    splice(@$tags, $index, 1);
}

##############
# Validators #
##############

sub run_create_validators {
    my $self = shift;
    my $params = $self->SUPER::run_create_validators(@_);
    # Sometimes this run_create_validators is called with parameters that
    # skip bug_id validation, so it might not exist in the resulting hash.
    if (defined $params->{bug_id}) {
        $params->{bug_id} = $params->{bug_id}->id;
    }
    return $params;
}

sub _check_extra_data {
    my ($invocant, $extra_data, undef, $params) = @_;
    my $type = blessed($invocant) ? $invocant->type : $params->{type};

    if ($type == CMT_NORMAL) {
        if (defined $extra_data) {
            ThrowCodeError('comment_extra_data_not_allowed',
                           { type => $type, extra_data => $extra_data });
        }
    }
    else {
        if (!defined $extra_data) {
            ThrowCodeError('comment_extra_data_required', { type => $type });
        }
        elsif ($type == CMT_ATTACHMENT_CREATED 
               or $type == CMT_ATTACHMENT_UPDATED) 
        {
             my $attachment = Bugzilla::Attachment->check({ 
                 id => $extra_data });
             $extra_data = $attachment->id;
        }
        else {
            my $original = $extra_data;
            detaint_natural($extra_data) 
              or ThrowCodeError('comment_extra_data_not_numeric',
                                { type => $type, extra_data => $original });
        }
    }

    return $extra_data;
}

sub _check_type {
    my ($invocant, $type) = @_;
    $type ||= CMT_NORMAL;
    my $original = $type;
    detaint_natural($type)
        or ThrowCodeError('comment_type_invalid', { type => $original });
    return $type;
}

sub _check_bug_id {
    my ($invocant, $bug_id) = @_;

    ThrowCodeError('param_required', {function => 'Bugzilla::Comment->create',
                                      param => 'bug_id'}) unless $bug_id;

    my $bug;
    if (blessed $bug_id) {
        # We got a bug object passed in, use it
        $bug = $bug_id;
        $bug->check_is_visible;
    }
    else {
        # We got a bug id passed in, check it and get the bug object
        $bug = Bugzilla::Bug->check({ id => $bug_id });
    }

    # Make sure the user can edit the product
    Bugzilla->user->can_edit_product($bug->{product_id});

    # Make sure the user can comment
    my $privs;
    $bug->check_can_change_field('longdesc', 0, 1, \$privs)
        || ThrowUserError('illegal_change', 
                          { field => 'longdesc', privs => $privs });
    return $bug;
}

sub _check_who {
    my ($invocant, $who) = @_;
    Bugzilla->login(LOGIN_REQUIRED);
    return Bugzilla->user->id;
}

sub _check_bug_when {
    my ($invocant, $when) = @_;

    # Make sure the timestamp is defined, default to a timestamp from the db
    if (!defined $when) {
        $when = Bugzilla->dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');
    }

    # Make sure the timestamp parses
    if (!datetime_from($when)) {
        ThrowCodeError('invalid_timestamp', { timestamp => $when });
    }

    return $when;
}

sub _check_work_time {
    my ($invocant, $value_in, $field, $params) = @_;

    # Call down to Bugzilla::Object, letting it know negative
    # values are ok
    my $time = $invocant->check_time($value_in, $field, $params, 1);
    my $privs;
    $params->{bug_id}->check_can_change_field('work_time', 0, $time, \$privs)
        || ThrowUserError('illegal_change',
                          { field => 'work_time', privs => $privs });
    return $time;
}

sub _check_thetext {
    my ($invocant, $thetext) = @_;

    ThrowCodeError('param_required',{function => 'Bugzilla::Comment->create',
                                     param => 'thetext'}) unless defined $thetext;

    # Remove any trailing whitespace. Leading whitespace could be
    # a valid part of the comment.
    $thetext =~ s/\s*$//s;
    $thetext =~ s/\r\n?/\n/g; # Get rid of \r.

    # Characters above U+FFFF cannot be stored by MySQL older than 5.5.3 as they
    # require the new utf8mb4 character set. Other DB servers are handling them
    # without any problem. So we need to replace these characters if we use MySQL,
    # else the comment is truncated.
    # XXX - Once we use utf8mb4 for comments, this hack for MySQL can go away.
    state $is_mysql = Bugzilla->dbh->isa('Bugzilla::DB::Mysql') ? 1 : 0;
    if ($is_mysql) {
        # Perl 5.13.8 and older complain about non-characters.
        no warnings 'utf8';
        $thetext =~ s/([\x{10000}-\x{10FFFF}])/"\x{FDD0}[" . uc(sprintf('U+%04x', ord($1))) . "]\x{FDD1}"/eg;
    }

    ThrowUserError('comment_too_long') if length($thetext) > MAX_COMMENT_LENGTH;
    return $thetext;
}

sub _check_isprivate {
    my ($invocant, $isprivate) = @_;
    if ($isprivate && !Bugzilla->user->is_insider) {
        ThrowUserError('user_not_insider');
    }
    return $isprivate ? 1 : 0;
}

sub _check_tag {
    my ($invocant, $tag) = @_;
    length($tag) < MIN_COMMENT_TAG_LENGTH
        and ThrowUserError('comment_tag_too_short', { tag => $tag });
    length($tag) > MAX_COMMENT_TAG_LENGTH
        and ThrowUserError('comment_tag_too_long', { tag => $tag });
    $tag =~ /^[\w\d\._-]+$/
        or ThrowUserError('comment_tag_invalid', { tag => $tag });
    return $tag;
}

sub count {
    my ($self) = @_;

    return $self->{'count'} if defined $self->{'count'};

    my $dbh = Bugzilla->dbh;
    ($self->{'count'}) = $dbh->selectrow_array(
        "SELECT COUNT(*)
           FROM longdescs 
          WHERE bug_id = ? 
                AND bug_when <= ?",
        undef, $self->bug_id, $self->creation_ts);

    return --$self->{'count'};
}

1;

__END__

=head1 NAME

Bugzilla::Comment - A Comment for a given bug 

=head1 SYNOPSIS

 use Bugzilla::Comment;

 my $comment = Bugzilla::Comment->new($comment_id);
 my $comments = Bugzilla::Comment->new_from_list($comment_ids);

=head1 DESCRIPTION

Bugzilla::Comment represents a comment attached to a bug.

This implements all standard C<Bugzilla::Object> methods. See 
L<Bugzilla::Object> for more details.

=head2 Accessors

=over

=item C<bug_id>

C<int> The ID of the bug to which the comment belongs.

=item C<creation_ts>

C<string> The comment creation timestamp.

=item C<body>

C<string> The body without any special additional text.

=item C<work_time>

C<string> Time spent as related to this comment.

=item C<is_private>

C<boolean> Comment is marked as private.

=item C<already_wrapped>

If this comment is stored in the database word-wrapped, this will be C<1>.
C<0> otherwise.

=item C<author>

L<Bugzilla::User> who created the comment.

=item C<count>

C<int> The position this comment is located in the full list of comments for a bug starting from 0.

=item C<collapsed>

C<boolean> Comment should be displayed as collapsed by default.

=item C<tags>

C<array of strings> The tags attached to the comment.

=item C<add_tag>

=over

=item B<Description>

Attaches the specified tag to the comment.

=item B<Params>

=over

=item C<tag>

C<string> The tag to attach.

=back

=back

=item C<remove_tag>

=over

=item B<Description>

Detaches the specified tag from the comment.

=item B<Params>

=over

=item C<tag>

C<string> The tag to detach.

=back

=back

=item C<body_full>

=over

=item B<Description>

C<string> Body of the comment, including any special text (such as
"this bug was marked as a duplicate of...").

=item B<Params>

=over

=item C<is_bugmail>

C<boolean>. C<1> if this comment should be formatted specifically for
bugmail.

=item C<wrap>

C<boolean>. C<1> if the comment should be returned word-wrapped.

=back

=item B<Returns>

A string, the full text of the comment as it would be displayed to an end-user.

=back

=back

=cut

=head1 B<Methods in need of POD>

=over

=item set_type

=item bug

=item set_extra_data

=item set_is_private

=item attachment

=item is_about_attachment

=item extra_data

=item preload

=item type

=item update

=back
