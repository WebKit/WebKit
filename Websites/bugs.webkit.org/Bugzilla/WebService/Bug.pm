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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Mads Bondo Dydensborg <mbd@dbc.dk>
#                 Tsahi Asher <tsahi_75@yahoo.com>
#                 Noura Elhawary <nelhawar@redhat.com>
#                 Frank Becker <Frank@Frank-Becker.de>
#                 Dave Lawrence <dkl@redhat.com>

package Bugzilla::WebService::Bug;

use strict;
use base qw(Bugzilla::WebService);

use Bugzilla::Comment;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field;
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(filter filter_wants validate);
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Util qw(trick_taint trim);
use Bugzilla::Version;
use Bugzilla::Milestone;
use Bugzilla::Status;
use Bugzilla::Token qw(issue_hash_token);

#############
# Constants #
#############

use constant PRODUCT_SPECIFIC_FIELDS => qw(version target_milestone component);

use constant DATE_FIELDS => {
    comments => ['new_since'],
    search   => ['last_change_time', 'creation_time'],
};

use constant BASE64_FIELDS => {
    add_attachment => ['data'],
};

use constant READ_ONLY => qw(
    attachments
    comments
    fields
    get
    history
    legal_values
    search
);

######################################################
# Add aliases here for old method name compatibility #
######################################################

BEGIN { 
  # In 3.0, get was called get_bugs
  *get_bugs = \&get;
  # Before 3.4rc1, "history" was get_history.
  *get_history = \&history;
}

###########
# Methods #
###########

sub fields {
    my ($self, $params) = validate(@_, 'ids', 'names');

    my @fields;
    if (defined $params->{ids}) {
        my $ids = $params->{ids};
        foreach my $id (@$ids) {
            my $loop_field = Bugzilla::Field->check({ id => $id });
            push(@fields, $loop_field);
        }
    }

    if (defined $params->{names}) {
        my $names = $params->{names};
        foreach my $field_name (@$names) {
            my $loop_field = Bugzilla::Field->check($field_name);
            # Don't push in duplicate fields if we also asked for this field
            # in "ids".
            if (!grep($_->id == $loop_field->id, @fields)) {
                push(@fields, $loop_field);
            }
        }
    }

    if (!defined $params->{ids} and !defined $params->{names}) {
        @fields = @{ Bugzilla->fields({ obsolete => 0 }) };
    }

    my @fields_out;
    foreach my $field (@fields) {
        my $visibility_field = $field->visibility_field
                               ? $field->visibility_field->name : undef;
        my $vis_values = $field->visibility_values;
        my $value_field = $field->value_field
                          ? $field->value_field->name : undef;

        my (@values, $has_values);
        if ( ($field->is_select and $field->name ne 'product')
             or grep($_ eq $field->name, PRODUCT_SPECIFIC_FIELDS))
        {
             $has_values = 1;
             @values = @{ $self->_legal_field_values({ field => $field }) };
        }

        if (grep($_ eq $field->name, PRODUCT_SPECIFIC_FIELDS)) {
             $value_field = 'product';
        }

        my %field_data = (
           id                => $self->type('int', $field->id),
           type              => $self->type('int', $field->type),
           is_custom         => $self->type('boolean', $field->custom),
           name              => $self->type('string', $field->name),
           display_name      => $self->type('string', $field->description),
           is_mandatory      => $self->type('boolean', $field->is_mandatory),
           is_on_bug_entry   => $self->type('boolean', $field->enter_bug),
           visibility_field  => $self->type('string', $visibility_field),
           visibility_values =>
              [ map { $self->type('string', $_->name) } @$vis_values ],
        );
        if ($has_values) {
           $field_data{value_field} = $self->type('string', $value_field);
           $field_data{values}      = \@values;
        };
        push(@fields_out, filter $params, \%field_data);
    }

    return { fields => \@fields_out };
}

sub _legal_field_values {
    my ($self, $params) = @_;
    my $field = $params->{field};
    my $field_name = $field->name;
    my $user = Bugzilla->user;

    my @result;
    if (grep($_ eq $field_name, PRODUCT_SPECIFIC_FIELDS)) {
        my @list;
        if ($field_name eq 'version') {
            @list = Bugzilla::Version->get_all;
        }
        elsif ($field_name eq 'component') {
            @list = Bugzilla::Component->get_all;
        }
        else {
            @list = Bugzilla::Milestone->get_all;
        }

        foreach my $value (@list) {
            my $sortkey = $field_name eq 'target_milestone'
                          ? $value->sortkey : 0;
            # XXX This is very slow for large numbers of values.
            my $product_name = $value->product->name;
            if ($user->can_see_product($product_name)) {
                push(@result, {
                    name     => $self->type('string', $value->name),
                    sort_key => $self->type('int', $sortkey),
                    sortkey  => $self->type('int', $sortkey), # deprecated
                    visibility_values => [$self->type('string', $product_name)],
                });
            }
        }
    }

    elsif ($field_name eq 'bug_status') {
        my @status_all = Bugzilla::Status->get_all;
        foreach my $status (@status_all) {
            my @can_change_to;
            foreach my $change_to (@{ $status->can_change_to }) {
                # There's no need to note that a status can transition
                # to itself.
                next if $change_to->id == $status->id;
                my %change_to_hash = (
                    name => $self->type('string', $change_to->name),
                    comment_required => $self->type('boolean', 
                        $change_to->comment_required_on_change_from($status)),
                );
                push(@can_change_to, \%change_to_hash);
            }

            push (@result, {
               name     => $self->type('string', $status->name),
               is_open  => $self->type('boolean', $status->is_open),
               sort_key => $self->type('int', $status->sortkey),
               sortkey  => $self->type('int', $status->sortkey), # deprecated
               can_change_to => \@can_change_to,
               visibility_values => [],
            });
        }
    }

    else {
        my @values = Bugzilla::Field::Choice->type($field)->get_all();
        foreach my $value (@values) {
            my $vis_val = $value->visibility_value;
            push(@result, {
                name     => $self->type('string', $value->name),
                sort_key => $self->type('int'   , $value->sortkey),
                sortkey  => $self->type('int'   , $value->sortkey), # deprecated
                visibility_values => [
                    defined $vis_val ? $self->type('string', $vis_val->name) 
                                     : ()
                ],
            });
        }
    }

    return \@result;
}

sub comments {
    my ($self, $params) = validate(@_, 'ids', 'comment_ids');

    if (!(defined $params->{ids} || defined $params->{comment_ids})) {
        ThrowCodeError('params_required',
                       { function => 'Bug.comments',
                         params   => ['ids', 'comment_ids'] });
    }

    my $bug_ids = $params->{ids} || [];
    my $comment_ids = $params->{comment_ids} || [];

    my $dbh  = Bugzilla->dbh;
    my $user = Bugzilla->user;

    my %bugs;
    foreach my $bug_id (@$bug_ids) {
        my $bug = Bugzilla::Bug->check($bug_id);
        # We want the API to always return comments in the same order.
   
        my $comments = $bug->comments({ order => 'oldest_to_newest',
                                        after => $params->{new_since} });
        my @result;
        foreach my $comment (@$comments) {
            next if $comment->is_private && !$user->is_insider;
            push(@result, $self->_translate_comment($comment, $params));
        }
        $bugs{$bug->id}{'comments'} = \@result;
    }

    my %comments;
    if (scalar @$comment_ids) {
        my @ids = map { trim($_) } @$comment_ids;
        my $comment_data = Bugzilla::Comment->new_from_list(\@ids);

        # See if we were passed any invalid comment ids.
        my %got_ids = map { $_->id => 1 } @$comment_data;
        foreach my $comment_id (@ids) {
            if (!$got_ids{$comment_id}) {
                ThrowUserError('comment_id_invalid', { id => $comment_id });
            }
        }
 
        # Now make sure that we can see all the associated bugs.
        my %got_bug_ids = map { $_->bug_id => 1 } @$comment_data;
        Bugzilla::Bug->check($_) foreach (keys %got_bug_ids);

        foreach my $comment (@$comment_data) {
            if ($comment->is_private && !$user->is_insider) {
                ThrowUserError('comment_is_private', { id => $comment->id });
            }
            $comments{$comment->id} =
                $self->_translate_comment($comment, $params);
        }
    }

    return { bugs => \%bugs, comments => \%comments };
}

# Helper for Bug.comments
sub _translate_comment {
    my ($self, $comment, $filters) = @_;
    my $attach_id = $comment->is_about_attachment ? $comment->extra_data
                                                  : undef;
    return filter $filters, {
        id         => $self->type('int', $comment->id),
        bug_id     => $self->type('int', $comment->bug_id),
        creator    => $self->type('string', $comment->author->login),
        author     => $self->type('string', $comment->author->login),
        time       => $self->type('dateTime', $comment->creation_ts),
        is_private => $self->type('boolean', $comment->is_private),
        text       => $self->type('string', $comment->body_full),
        attachment_id => $self->type('int', $attach_id),
    };
}

sub get {
    my ($self, $params) = validate(@_, 'ids');

    my $ids = $params->{ids};
    defined $ids || ThrowCodeError('param_required', { param => 'ids' });

    my @bugs;
    my @faults;
    foreach my $bug_id (@$ids) {
        my $bug;
        if ($params->{permissive}) {
            eval { $bug = Bugzilla::Bug->check($bug_id); };
            if ($@) {
                push(@faults, {id => $bug_id,
                               faultString => $@->faultstring,
                               faultCode => $@->faultcode,
                              }
                    );
                undef $@;
                next;
            }
        }
        else {
            $bug = Bugzilla::Bug->check($bug_id);
        }
        push(@bugs, $self->_bug_to_hash($bug, $params));
    }

    return { bugs => \@bugs, faults => \@faults };
}

# this is a function that gets bug activity for list of bug ids 
# it can be called as the following:
# $call = $rpc->call( 'Bug.history', { ids => [1,2] });
sub history {
    my ($self, $params) = validate(@_, 'ids');

    my $ids = $params->{ids};
    defined $ids || ThrowCodeError('param_required', { param => 'ids' });

    my @return;

    foreach my $bug_id (@$ids) {
        my %item;
        my $bug = Bugzilla::Bug->check($bug_id);
        $bug_id = $bug->id;
        $item{id} = $self->type('int', $bug_id);

        my ($activity) = Bugzilla::Bug::GetBugActivity($bug_id);

        my @history;
        foreach my $changeset (@$activity) {
            my %bug_history;
            $bug_history{when} = $self->type('dateTime', $changeset->{when});
            $bug_history{who}  = $self->type('string', $changeset->{who});
            $bug_history{changes} = [];
            foreach my $change (@{ $changeset->{changes} }) {
                my $attach_id = delete $change->{attachid};
                if ($attach_id) {
                    $change->{attachment_id} = $self->type('int', $attach_id);
                }
                $change->{removed} = $self->type('string', $change->{removed});
                $change->{added}   = $self->type('string', $change->{added});
                $change->{field_name} = $self->type('string',
                    delete $change->{fieldname});
                push (@{$bug_history{changes}}, $change);
            }
            
            push (@history, \%bug_history);
        }

        $item{history} = \@history;

        # alias is returned in case users passes a mixture of ids and aliases
        # then they get to know which bug activity relates to which value  
        # they passed
        if (Bugzilla->params->{'usebugaliases'}) {
            $item{alias} = $self->type('string', $bug->alias);
        }
        else {
            # For API reasons, we always want the value to appear, we just
            # don't want it to have a value if aliases are turned off.
            $item{alias} = undef;
        }

        push(@return, \%item);
    }

    return { bugs => \@return };
}

sub search {
    my ($self, $params) = @_;

    if ( defined($params->{offset}) and !defined($params->{limit}) ) {
        ThrowCodeError('param_required', 
                       { param => 'limit', function => 'Bug.search()' });
    }

    my $max_results = Bugzilla->params->{max_search_results};
    unless (defined $params->{limit} && $params->{limit} == 0) {
        if (!defined $params->{limit} || $params->{limit} > $max_results) {
            $params->{limit} = $max_results;
        }
    }
    else {
        delete $params->{limit};
        delete $params->{offset};
    }

    $params = Bugzilla::Bug::map_fields($params);
    delete $params->{WHERE};

    unless (Bugzilla->user->is_timetracker) {
        delete $params->{$_} foreach qw(estimated_time remaining_time deadline);
    }

    # Do special search types for certain fields.
    if ( my $bug_when = delete $params->{delta_ts} ) {
        $params->{WHERE}->{'delta_ts >= ?'} = $bug_when;
    }
    if (my $when = delete $params->{creation_ts}) {
        $params->{WHERE}->{'creation_ts >= ?'} = $when;
    }
    if (my $summary = delete $params->{short_desc}) {
        my @strings = ref $summary ? @$summary : ($summary);
        my @likes = ("short_desc LIKE ?") x @strings;
        my $clause = join(' OR ', @likes);
        $params->{WHERE}->{"($clause)"} = [map { "\%$_\%" } @strings];
    }
    if (my $whiteboard = delete $params->{status_whiteboard}) {
        my @strings = ref $whiteboard ? @$whiteboard : ($whiteboard);
        my @likes = ("status_whiteboard LIKE ?") x @strings;
        my $clause = join(' OR ', @likes);
        $params->{WHERE}->{"($clause)"} = [map { "\%$_\%" } @strings];
    }

    # If no other parameters have been passed other than limit and offset
    # and a WHERE parameter was not created earlier, then we throw error
    # if system is configured to do so.
    if (!$params->{WHERE}
        && !grep(!/(limit|offset)/i, keys %$params)
        && !Bugzilla->params->{search_allow_no_criteria})
    {
        ThrowUserError('buglist_parameters_required');
    }

    # We want include_fields and exclude_fields to be passed to
    # _bug_to_hash but not to Bugzilla::Bug->match so we copy the 
    # params and delete those before passing to Bugzilla::Bug->match.
    my %match_params = %{ $params };
    delete $match_params{'include_fields'};
    delete $match_params{'exclude_fields'};

    my $bugs = Bugzilla::Bug->match(\%match_params);
    my $visible = Bugzilla->user->visible_bugs($bugs);
    my @hashes = map { $self->_bug_to_hash($_, $params) } @$visible;
    return { bugs => \@hashes };
}

sub possible_duplicates {
    my ($self, $params) = validate(@_, 'product');
    my $user = Bugzilla->user;

    # Undo the array-ification that validate() does, for "summary".
    $params->{summary} || ThrowCodeError('param_required',
        { function => 'Bug.possible_duplicates', param => 'summary' });

    my @products;
    foreach my $name (@{ $params->{'product'} || [] }) {
        my $object = $user->can_enter_product($name, THROW_ERROR);
        push(@products, $object);
    }

    my $possible_dupes = Bugzilla::Bug->possible_duplicates(
        { summary => $params->{summary}, products => \@products,
          limit   => $params->{limit} });
    my @hashes = map { $self->_bug_to_hash($_, $params) } @$possible_dupes;
    return { bugs => \@hashes };
}

sub update {
    my ($self, $params) = validate(@_, 'ids');

    my $user = Bugzilla->login(LOGIN_REQUIRED);
    my $dbh = Bugzilla->dbh;

    # We skip certain fields because their set_ methods actually use
    # the external names instead of the internal names.
    $params = Bugzilla::Bug::map_fields($params, 
        { summary => 1, platform => 1, severity => 1, url => 1 });

    my $ids = delete $params->{ids};
    defined $ids || ThrowCodeError('param_required', { param => 'ids' });

    my @bugs = map { Bugzilla::Bug->check($_) } @$ids;

    my %values = %$params;
    $values{other_bugs} = \@bugs;

    if (exists $values{comment} and exists $values{comment}{comment}) {
        $values{comment}{body} = delete $values{comment}{comment};
    }

    # Prevent bugs that could be triggered by specifying fields that
    # have valid "set_" functions in Bugzilla::Bug, but shouldn't be
    # called using those field names.
    delete $values{dependencies};
    delete $values{flags};

    foreach my $bug (@bugs) {
        if (!$user->can_edit_product($bug->product_obj->id) ) {
            ThrowUserError("product_edit_denied",
                          { product => $bug->product });
        }

        $bug->set_all(\%values);
    }

    my %all_changes;
    $dbh->bz_start_transaction();
    foreach my $bug (@bugs) {
        $all_changes{$bug->id} = $bug->update();
    }
    $dbh->bz_commit_transaction();
    
    foreach my $bug (@bugs) {
        $bug->send_changes($all_changes{$bug->id});
    }

    my %api_name = reverse %{ Bugzilla::Bug::FIELD_MAP() };
    # This doesn't normally belong in FIELD_MAP, but we do want to translate
    # "bug_group" back into "groups".
    $api_name{'bug_group'} = 'groups';

    my @result;
    foreach my $bug (@bugs) {
        my %hash = (
            id               => $self->type('int', $bug->id),
            last_change_time => $self->type('dateTime', $bug->delta_ts),
            changes          => {},
        );

        # alias is returned in case users pass a mixture of ids and aliases,
        # so that they can know which set of changes relates to which value
        # they passed.
        if (Bugzilla->params->{'usebugaliases'}) {
            $hash{alias} = $self->type('string', $bug->alias);
        }
        else {
            # For API reasons, we always want the alias field to appear, we
            # just don't want it to have a value if aliases are turned off.
            $hash{alias} = $self->type('string', '');
        }

        my %changes = %{ $all_changes{$bug->id} };
        foreach my $field (keys %changes) {
            my $change = $changes{$field};
            my $api_field = $api_name{$field} || $field;
            # We normalize undef to an empty string, so that the API
            # stays consistent for things like Deadline that can become
            # empty.
            $change->[0] = '' if !defined $change->[0];
            $change->[1] = '' if !defined $change->[1];
            $hash{changes}->{$api_field} = {
                removed => $self->type('string', $change->[0]),
                added   => $self->type('string', $change->[1]) 
            };
        }

        push(@result, \%hash);
    }

    return { bugs => \@result };
}

sub create {
    my ($self, $params) = @_;
    Bugzilla->login(LOGIN_REQUIRED);
    $params = Bugzilla::Bug::map_fields($params);
    my $bug = Bugzilla::Bug->create($params);
    Bugzilla::BugMail::Send($bug->bug_id, { changer => $bug->reporter });
    return { id => $self->type('int', $bug->bug_id) };
}

sub legal_values {
    my ($self, $params) = @_;

    defined $params->{field} 
        or ThrowCodeError('param_required', { param => 'field' });

    my $field = Bugzilla::Bug::FIELD_MAP->{$params->{field}} 
                || $params->{field};

    my @global_selects =
        @{ Bugzilla->fields({ is_select => 1, is_abnormal => 0 }) };

    my $values;
    if (grep($_->name eq $field, @global_selects)) {
        # The field is a valid one.
        trick_taint($field);
        $values = get_legal_field_values($field);
    }
    elsif (grep($_ eq $field, PRODUCT_SPECIFIC_FIELDS)) {
        my $id = $params->{product_id};
        defined $id || ThrowCodeError('param_required',
            { function => 'Bug.legal_values', param => 'product_id' });
        grep($_->id eq $id, @{Bugzilla->user->get_accessible_products})
            || ThrowUserError('product_access_denied', { id => $id });

        my $product = new Bugzilla::Product($id);
        my @objects;
        if ($field eq 'version') {
            @objects = @{$product->versions};
        }
        elsif ($field eq 'target_milestone') {
            @objects = @{$product->milestones};
        }
        elsif ($field eq 'component') {
            @objects = @{$product->components};
        }

        $values = [map { $_->name } @objects];
    }
    else {
        ThrowCodeError('invalid_field_name', { field => $params->{field} });
    }

    my @result;
    foreach my $val (@$values) {
        push(@result, $self->type('string', $val));
    }

    return { values => \@result };
}

sub add_attachment {
    my ($self, $params) = validate(@_, 'ids');
    my $dbh = Bugzilla->dbh;

    Bugzilla->login(LOGIN_REQUIRED);
    defined $params->{ids}
        || ThrowCodeError('param_required', { param => 'ids' });
    defined $params->{data}
        || ThrowCodeError('param_required', { param => 'data' });

    my @bugs = map { Bugzilla::Bug->check($_) } @{ $params->{ids} };
    foreach my $bug (@bugs) {
        Bugzilla->user->can_edit_product($bug->product_id)
          || ThrowUserError("product_edit_denied", {product => $bug->product});
    }

    my @created;
    $dbh->bz_start_transaction();
    my $timestamp = $dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');

    foreach my $bug (@bugs) {
        my $attachment = Bugzilla::Attachment->create({
            bug         => $bug,
            creation_ts => $timestamp,
            data        => $params->{data},
            description => $params->{summary},
            filename    => $params->{file_name},
            mimetype    => $params->{content_type},
            ispatch     => $params->{is_patch},
            isprivate   => $params->{is_private},
        });
        my $comment = $params->{comment} || '';
        $attachment->bug->add_comment($comment, 
            { isprivate  => $attachment->isprivate,
              type       => CMT_ATTACHMENT_CREATED,
              extra_data => $attachment->id });
        push(@created, $attachment);
    }
    $_->bug->update($timestamp) foreach @created;
    $dbh->bz_commit_transaction();

    $_->send_changes() foreach @bugs;

    my %attachments = map { $_->id => $self->_attachment_to_hash($_, $params) }
                          @created;

    return { attachments => \%attachments };
}

sub add_comment {
    my ($self, $params) = @_;
    
    #The user must login in order add a comment
    Bugzilla->login(LOGIN_REQUIRED);
    
    # Check parameters
    defined $params->{id} 
        || ThrowCodeError('param_required', { param => 'id' }); 
    my $comment = $params->{comment}; 
    (defined $comment && trim($comment) ne '')
        || ThrowCodeError('param_required', { param => 'comment' });
    
    my $bug = Bugzilla::Bug->check($params->{id});
    
    Bugzilla->user->can_edit_product($bug->product_id)
        || ThrowUserError("product_edit_denied", {product => $bug->product});
    
    # Backwards-compatibility for versions before 3.6    
    if (defined $params->{private}) {
        $params->{is_private} = delete $params->{private};
    }
    # Append comment
    $bug->add_comment($comment, { isprivate => $params->{is_private},
                                  work_time => $params->{work_time} });
    
    # Capture the call to bug->update (which creates the new comment) in 
    # a transaction so we're sure to get the correct comment_id.
    
    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    
    $bug->update();
    
    my $new_comment_id = $dbh->bz_last_key('longdescs', 'comment_id');
    
    $dbh->bz_commit_transaction();
    
    # Send mail.
    Bugzilla::BugMail::Send($bug->bug_id, { changer => Bugzilla->user });
    
    return { id => $self->type('int', $new_comment_id) };
}

sub update_see_also {
    my ($self, $params) = @_;

    my $user = Bugzilla->login(LOGIN_REQUIRED);

    # Check parameters
    $params->{ids}
        || ThrowCodeError('param_required', { param => 'id' });
    my ($add, $remove) = @$params{qw(add remove)};
    ($add || $remove)
        or ThrowCodeError('params_required', { params => ['add', 'remove'] });

    my @bugs;
    foreach my $id (@{ $params->{ids} }) {
        my $bug = Bugzilla::Bug->check($id);
        $user->can_edit_product($bug->product_id)
            || ThrowUserError("product_edit_denied", 
                              { product => $bug->product });
        push(@bugs, $bug);
        if ($remove) {
            $bug->remove_see_also($_) foreach @$remove;
        }
        if ($add) {
            $bug->add_see_also($_) foreach @$add;
        }
    }
    
    my %changes;
    foreach my $bug (@bugs) {
        my $change = $bug->update();
        if (my $see_also = $change->{see_also}) {
            $changes{$bug->id}->{see_also} = {
                removed => [split(', ', $see_also->[0])],
                added   => [split(', ', $see_also->[1])],
            };
        }
        else {
            # We still want a changes entry, for API consistency.
            $changes{$bug->id}->{see_also} = { added => [], removed => [] };
        }

        Bugzilla::BugMail::Send($bug->id, { changer => $user });
    }

    return { changes => \%changes };
}

sub attachments {
    my ($self, $params) = validate(@_, 'ids', 'attachment_ids');

    if (!(defined $params->{ids}
          or defined $params->{attachment_ids}))
    {
        ThrowCodeError('param_required',
                       { function => 'Bug.attachments', 
                         params   => ['ids', 'attachment_ids'] });
    }

    my $ids = $params->{ids} || [];
    my $attach_ids = $params->{attachment_ids} || [];

    my %bugs;
    foreach my $bug_id (@$ids) {
        my $bug = Bugzilla::Bug->check($bug_id);
        $bugs{$bug->id} = [];
        foreach my $attach (@{$bug->attachments}) {
            push @{$bugs{$bug->id}},
                $self->_attachment_to_hash($attach, $params);
        }
    }

    my %attachments;
    foreach my $attach (@{Bugzilla::Attachment->new_from_list($attach_ids)}) {
        Bugzilla::Bug->check($attach->bug_id);
        if ($attach->isprivate && !Bugzilla->user->is_insider) {
            ThrowUserError('auth_failure', {action    => 'access',
                                            object    => 'attachment',
                                            attach_id => $attach->id});
        }
        $attachments{$attach->id} =
            $self->_attachment_to_hash($attach, $params);
    }

    return { bugs => \%bugs, attachments => \%attachments };
}

##############################
# Private Helper Subroutines #
##############################

# A helper for get() and search(). This is done in this fashion in order
# to produce a stable API and to explicitly type return values.
# The internals of Bugzilla::Bug are not stable enough to just
# return them directly.

sub _bug_to_hash {
    my ($self, $bug, $params) = @_;

    # All the basic bug attributes are here, in alphabetical order.
    # A bug attribute is "basic" if it doesn't require an additional
    # database call to get the info.
    my %item = (
        alias            => $self->type('string', $bug->alias),
        classification   => $self->type('string', $bug->classification),
        component        => $self->type('string', $bug->component),
        creation_time    => $self->type('dateTime', $bug->creation_ts),
        id               => $self->type('int', $bug->bug_id),
        is_confirmed     => $self->type('boolean', $bug->everconfirmed),
        last_change_time => $self->type('dateTime', $bug->delta_ts),
        op_sys           => $self->type('string', $bug->op_sys),
        platform         => $self->type('string', $bug->rep_platform),
        priority         => $self->type('string', $bug->priority),
        product          => $self->type('string', $bug->product),
        resolution       => $self->type('string', $bug->resolution),
        severity         => $self->type('string', $bug->bug_severity),
        status           => $self->type('string', $bug->bug_status),
        summary          => $self->type('string', $bug->short_desc),
        target_milestone => $self->type('string', $bug->target_milestone),
        url              => $self->type('string', $bug->bug_file_loc),
        version          => $self->type('string', $bug->version),
        whiteboard       => $self->type('string', $bug->status_whiteboard),
    );


    # First we handle any fields that require extra SQL calls.
    # We don't do the SQL calls at all if the filter would just
    # eliminate them anyway.
    if (filter_wants $params, 'assigned_to') {
        $item{'assigned_to'} = $self->type('string', $bug->assigned_to->login);
    }
    if (filter_wants $params, 'blocks') {
        my @blocks = map { $self->type('int', $_) } @{ $bug->blocked };
        $item{'blocks'} = \@blocks;
    }
    if (filter_wants $params, 'cc') {
        my @cc = map { $self->type('string', $_) } @{ $bug->cc || [] };
        $item{'cc'} = \@cc;
    }
    if (filter_wants $params, 'creator') {
        $item{'creator'} = $self->type('string', $bug->reporter->login);
    }
    if (filter_wants $params, 'depends_on') {
        my @depends_on = map { $self->type('int', $_) } @{ $bug->dependson };
        $item{'depends_on'} = \@depends_on;
    }
    if (filter_wants $params, 'dupe_of') {
        $item{'dupe_of'} = $self->type('int', $bug->dup_id);
    }
    if (filter_wants $params, 'groups') {
        my @groups = map { $self->type('string', $_->name) }
                     @{ $bug->groups_in };
        $item{'groups'} = \@groups;
    }
    if (filter_wants $params, 'is_open') {
        $item{'is_open'} = $self->type('boolean', $bug->status->is_open);
    }
    if (filter_wants $params, 'keywords') {
        my @keywords = map { $self->type('string', $_->name) }
                       @{ $bug->keyword_objects };
        $item{'keywords'} = \@keywords;
    }
    if (filter_wants $params, 'qa_contact') {
        my $qa_login = $bug->qa_contact ? $bug->qa_contact->login : '';
        $item{'qa_contact'} = $self->type('string', $qa_login);
    }
    if (filter_wants $params, 'see_also') {
        my @see_also = map { $self->type('string', $_->name) }
                       @{ $bug->see_also };
        $item{'see_also'} = \@see_also;
    }

    # And now custom fields
    my @custom_fields = Bugzilla->active_custom_fields;
    foreach my $field (@custom_fields) {
        my $name = $field->name;
        next if !filter_wants $params, $name;
        if ($field->type == FIELD_TYPE_BUG_ID) {
            $item{$name} = $self->type('int', $bug->$name);
        }
        elsif ($field->type == FIELD_TYPE_DATETIME) {
            $item{$name} = $self->type('dateTime', $bug->$name);
        }
        elsif ($field->type == FIELD_TYPE_MULTI_SELECT) {
            my @values = map { $self->type('string', $_) } @{ $bug->$name };
            $item{$name} = \@values;
        }
        else {
            $item{$name} = $self->type('string', $bug->$name);
        }
    }

    # Timetracking fields are only sent if the user can see them.
    if (Bugzilla->user->is_timetracker) {
        $item{'estimated_time'} = $self->type('double', $bug->estimated_time);
        $item{'remaining_time'} = $self->type('double', $bug->remaining_time);
        # No need to format $bug->deadline specially, because Bugzilla::Bug
        # already does it for us.
        $item{'deadline'} = $self->type('string', $bug->deadline);
    }

    if (Bugzilla->user->id) {
        my $token = issue_hash_token([$bug->id, $bug->delta_ts]);
        $item{'update_token'} = $self->type('string', $token);
    }

    # The "accessible" bits go here because they have long names and it
    # makes the code look nicer to separate them out.
    $item{'is_cc_accessible'} = $self->type('boolean', 
                                            $bug->cclist_accessible);
    $item{'is_creator_accessible'} = $self->type('boolean',
                                                 $bug->reporter_accessible);

    return filter $params, \%item;
}

sub _attachment_to_hash {
    my ($self, $attach, $filters) = @_;

    # Skipping attachment flags for now.
    delete $attach->{flags};

    my $item = filter $filters, {
        creation_time    => $self->type('dateTime', $attach->attached),
        last_change_time => $self->type('dateTime', $attach->modification_time),
        id               => $self->type('int', $attach->id),
        bug_id           => $self->type('int', $attach->bug_id),
        file_name        => $self->type('string', $attach->filename),
        summary          => $self->type('string', $attach->description),
        description      => $self->type('string', $attach->description),
        content_type     => $self->type('string', $attach->contenttype),
        is_private       => $self->type('int', $attach->isprivate),
        is_obsolete      => $self->type('int', $attach->isobsolete),
        is_patch         => $self->type('int', $attach->ispatch),
    };

    # creator/attacher require an extra lookup, so we only send them if
    # the filter wants them.
    foreach my $field (qw(creator attacher)) {
        if (filter_wants $filters, $field) {
            $item->{$field} = $self->type('string', $attach->attacher->login);
        }
    }

    if (filter_wants $filters, 'data') {
        $item->{'data'} = $self->type('base64', $attach->data);
    }

    return $item;
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Bug - The API for creating, changing, and getting the
details of bugs.

=head1 DESCRIPTION

This part of the Bugzilla API allows you to file a new bug in Bugzilla,
or get information about bugs that have already been filed.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

=head1 Utility Functions

=head2 fields

B<UNSTABLE>

=over

=item B<Description>

Get information about valid bug fields, including the lists of legal values
for each field.

=item B<Params>

You can pass either field ids or field names.

B<Note>: If neither C<ids> nor C<names> is specified, then all 
non-obsolete fields will be returned.

In addition to the parameters below, this method also accepts the
standard L<include_fields|Bugzilla::WebService/include_fields> and
L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

=over

=item C<ids>   (array) - An array of integer field ids.

=item C<names> (array) - An array of strings representing field names.

=back

=item B<Returns>

A hash containing a single element, C<fields>. This is an array of hashes,
containing the following keys:

=over

=item C<id>

C<int> An integer id uniquely identifying this field in this installation only.

=item C<type>

C<int> The number of the fieldtype. The following values are defined:

=over

=item C<0> Unknown

=item C<1> Free Text

=item C<2> Drop Down

=item C<3> Multiple-Selection Box

=item C<4> Large Text Box

=item C<5> Date/Time

=item C<6> Bug Id

=item C<7> Bug URLs ("See Also")

=back

=item C<is_custom>

C<boolean> True when this is a custom field, false otherwise.

=item C<name>

C<string> The internal name of this field. This is a unique identifier for
this field. If this is not a custom field, then this name will be the same
across all Bugzilla installations.

=item C<display_name>

C<string> The name of the field, as it is shown in the user interface.

=item C<is_mandatory>

C<boolean> True if the field must have a value when filing new bugs.
Also, mandatory fields cannot have their value cleared when updating
bugs.

=item C<is_on_bug_entry>

C<boolean> For custom fields, this is true if the field is shown when you
enter a new bug. For standard fields, this is currently always false,
even if the field shows up when entering a bug. (To know whether or not
a standard field is valid on bug entry, see L</create>.)

=item C<visibility_field>

C<string>  The name of a field that controls the visibility of this field
in the user interface. This field only appears in the user interface when
the named field is equal to one of the values in C<visibility_values>.
Can be null.

=item C<visibility_values>

C<array> of C<string>s This field is only shown when C<visibility_field>
matches one of these values. When C<visibility_field> is null,
then this is an empty array.

=item C<value_field>

C<string>  The name of the field that controls whether or not particular
values of the field are shown in the user interface. Can be null.

=item C<values>

This is an array of hashes, representing the legal values for
select-type (drop-down and multiple-selection) fields. This is also
populated for the C<component>, C<version>, and C<target_milestone>
fields, but not for the C<product> field (you must use
L<Product.get_accessible_products|Bugzilla::WebService::Product/get_accessible_products>
for that.

For fields that aren't select-type fields, this will simply be an empty
array.

Each hash has the following keys:

=over 

=item C<name>

C<string> The actual value--this is what you would specify for this
field in L</create>, etc.

=item C<sort_key>

C<int> Values, when displayed in a list, are sorted first by this integer
and then secondly by their name.

=item C<sortkey>

B<DEPRECATED> - Use C<sort_key> instead.

=item C<visibility_values>

If C<value_field> is defined for this field, then this value is only shown
if the C<value_field> is set to one of the values listed in this array.
Note that for per-product fields, C<value_field> is set to C<'product'>
and C<visibility_values> will reflect which product(s) this value appears in.

=item C<is_open>

C<boolean> For C<bug_status> values, determines whether this status
specifies that the bug is "open" (true) or "closed" (false). This item
is only included for the C<bug_status> field.

=item C<can_change_to>

For C<bug_status> values, this is an array of hashes that determines which
statuses you can transition to from this status. (This item is only included
for the C<bug_status> field.)

Each hash contains the following items:

=over

=item C<name>

the name of the new status

=item C<comment_required>

this C<boolean> True if a comment is required when you change a bug into
this status using this transition.

=back

=back

=back

=item B<Errors>

=over

=item 51 (Invalid Field Name or Id)

You specified an invalid field name or id.

=back

=item B<History>

=over

=item Added in Bugzilla B<3.6>.

=item The C<is_mandatory> return value was added in Bugzilla B<4.0>.

=item C<sortkey> was renamed to C<sort_key> in Bugzilla B<4.2>.

=back

=back


=head2 legal_values

B<DEPRECATED> - Use L</fields> instead.

=over

=item B<Description>

Tells you what values are allowed for a particular field.

=item B<Params>

=over

=item C<field> - The name of the field you want information about.
This should be the same as the name you would use in L</create>, below.

=item C<product_id> - If you're picking a product-specific field, you have
to specify the id of the product you want the values for.

=back

=item B<Returns> 

C<values> - An array of strings: the legal values for this field.
The values will be sorted as they normally would be in Bugzilla.

=item B<Errors>

=over

=item 106 (Invalid Product)

You were required to specify a product, and either you didn't, or you
specified an invalid product (or a product that you can't access).

=item 108 (Invalid Field Name)

You specified a field that doesn't exist or isn't a drop-down field.

=back

=back

=head1 Bug Information

=head2 attachments

B<EXPERIMENTAL>

=over

=item B<Description>

It allows you to get data about attachments, given a list of bugs
and/or attachment ids.

B<Note>: Private attachments will only be returned if you are in the 
insidergroup or if you are the submitter of the attachment.

=item B<Params>

B<Note>: At least one of C<ids> or C<attachment_ids> is required.

=over

=item C<ids>

See the description of the C<ids> parameter in the L</get> method.

=item C<attachment_ids>

C<array> An array of integer attachment ids.

=back

Also accepts the L<include_fields|Bugzilla::WebService/include_fields>,
and L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

=item B<Returns>

A hash containing two elements: C<bugs> and C<attachments>. The return
value looks like this:

 {
     bugs => {
         1345 => [
             { (attachment) },
             { (attachment) }
         ],
         9874 => [
             { (attachment) },
             { (attachment) }
         ],
     },

     attachments => {
         234 => { (attachment) },
         123 => { (attachment) },
     }
 }

The attachments of any bugs that you specified in the C<ids> argument in
input are returned in C<bugs> on output. C<bugs> is a hash that has integer
bug IDs for keys and the values are arrayrefs that contain hashes as attachments.
(Fields for attachments are described below.)

For any attachments that you specified directly in C<attachment_ids>, they
are returned in C<attachments> on output. This is a hash where the attachment
ids point directly to hashes describing the individual attachment.

The fields for each attachment (where it says C<(attachment)> in the
diagram above) are:

=over

=item C<data>

C<base64> The raw data of the attachment, encoded as Base64.

=item C<creation_time>

C<dateTime> The time the attachment was created.

=item C<last_change_time>

C<dateTime> The last time the attachment was modified.

=item C<id>

C<int> The numeric id of the attachment.

=item C<bug_id>

C<int> The numeric id of the bug that the attachment is attached to.

=item C<file_name>

C<string> The file name of the attachment.

=item C<summary>

C<string> A short string describing the attachment.

Also returned as C<description>, for backwards-compatibility with older
Bugzillas. (However, this backwards-compatibility will go away in Bugzilla
5.0.)

=item C<content_type>

C<string> The MIME type of the attachment.

=item C<is_private>

C<boolean> True if the attachment is private (only visible to a certain
group called the "insidergroup"), False otherwise.

=item C<is_obsolete>

C<boolean> True if the attachment is obsolete, False otherwise.

=item C<is_patch>

C<boolean> True if the attachment is a patch, False otherwise.

=item C<creator>

C<string> The login name of the user that created the attachment.

Also returned as C<attacher>, for backwards-compatibility with older
Bugzillas. (However, this backwards-compatibility will go away in Bugzilla
5.0.)

=back

=item B<Errors>

This method can throw all the same errors as L</get>. In addition,
it can also throw the following error:

=over

=item 304 (Auth Failure, Attachment is Private)

You specified the id of a private attachment in the C<attachment_ids>
argument, and you are not in the "insider group" that can see
private attachments.

=back

=item B<History>

=over

=item Added in Bugzilla B<3.6>.

=item In Bugzilla B<4.0>, the C<attacher> return value was renamed to
C<creator>.

=item In Bugzilla B<4.0>, the C<description> return value was renamed to
C<summary>.

=item The C<data> return value was added in Bugzilla B<4.0>.

=item In Bugzilla B<4.2>, the C<is_url> return value was removed
(this attribute no longer exists for attachments).

=back

=back


=head2 comments

B<STABLE>

=over

=item B<Description>

This allows you to get data about comments, given a list of bugs 
and/or comment ids.

=item B<Params>

B<Note>: At least one of C<ids> or C<comment_ids> is required.

In addition to the parameters below, this method also accepts the
standard L<include_fields|Bugzilla::WebService/include_fields> and
L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

=over

=item C<ids>

C<array> An array that can contain both bug IDs and bug aliases.
All of the comments (that are visible to you) will be returned for the
specified bugs.

=item C<comment_ids> 

C<array> An array of integer comment_ids. These comments will be
returned individually, separate from any other comments in their
respective bugs.

=item C<new_since>

C<dateTime> If specified, the method will only return comments I<newer>
than this time. This only affects comments returned from the C<ids>
argument. You will always be returned all comments you request in the
C<comment_ids> argument, even if they are older than this date.

=back

=item B<Returns>

Two items are returned:

=over

=item C<bugs>

This is used for bugs specified in C<ids>. This is a hash,
where the keys are the numeric ids of the bugs, and the value is
a hash with a single key, C<comments>, which is an array of comments.
(The format of comments is described below.)

Note that any individual bug will only be returned once, so if you
specify an id multiple times in C<ids>, it will still only be
returned once.

=item C<comments>

Each individual comment requested in C<comment_ids> is returned here,
in a hash where the numeric comment id is the key, and the value
is the comment. (The format of comments is described below.) 

=back

A "comment" as described above is a hash that contains the following
keys:

=over

=item id

C<int> The globally unique ID for the comment.

=item bug_id

C<int> The ID of the bug that this comment is on.

=item attachment_id

C<int> If the comment was made on an attachment, this will be the
ID of that attachment. Otherwise it will be null.

=item text

C<string> The actual text of the comment.

=item creator

C<string> The login name of the comment's author.

Also returned as C<author>, for backwards-compatibility with older
Bugzillas. (However, this backwards-compatibility will go away in Bugzilla
5.0.)

=item time

C<dateTime> The time (in Bugzilla's timezone) that the comment was added.

=item is_private

C<boolean> True if this comment is private (only visible to a certain
group called the "insidergroup"), False otherwise.

=back

=item B<Errors>

This method can throw all the same errors as L</get>. In addition,
it can also throw the following errors:

=over

=item 110 (Comment Is Private)

You specified the id of a private comment in the C<comment_ids>
argument, and you are not in the "insider group" that can see
private comments.

=item 111 (Invalid Comment ID)

You specified an id in the C<comment_ids> argument that is invalid--either
you specified something that wasn't a number, or there is no comment with
that id.

=back

=item B<History>

=over

=item Added in Bugzilla B<3.4>.

=item C<attachment_id> was added to the return value in Bugzilla B<3.6>.

=item In Bugzilla B<4.0>, the C<author> return value was renamed to
C<creator>.

=back

=back


=head2 get

B<STABLE>

=over

=item B<Description>

Gets information about particular bugs in the database.

Note: Can also be called as "get_bugs" for compatibilty with Bugzilla 3.0 API.

=item B<Params>

In addition to the parameters below, this method also accepts the
standard L<include_fields|Bugzilla::WebService/include_fields> and
L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

=over

=item C<ids>

An array of numbers and strings.

If an element in the array is entirely numeric, it represents a bug_id
from the Bugzilla database to fetch. If it contains any non-numeric 
characters, it is considered to be a bug alias instead, and the bug with 
that alias will be loaded. 

Note that it's possible for aliases to be disabled in Bugzilla, in which
case you will be told that you have specified an invalid bug_id if you
try to specify an alias. (It will be error 100.)

=item C<permissive> B<EXPERIMENTAL>

C<boolean> Normally, if you request any inaccessible or invalid bug ids,
Bug.get will throw an error. If this parameter is True, instead of throwing an
error we return an array of hashes with a C<id>, C<faultString> and C<faultCode> 
for each bug that fails, and return normal information for the other bugs that 
were accessible.

=back

=item B<Returns>

Two items are returned:

=over

=item C<bugs>

An array of hashes that contains information about the bugs with 
the valid ids. Each hash contains the following items:

=over

=item C<alias>

C<string> The unique alias of this bug.

=item C<assigned_to>

C<string> The login name of the user to whom the bug is assigned.

=item C<blocks>

C<array> of C<int>s. The ids of bugs that are "blocked" by this bug.

=item C<cc>

C<array> of C<string>s. The login names of users on the CC list of this
bug.

=item C<classification>

C<string> The name of the current classification the bug is in.

=item C<component>

C<string> The name of the current component of this bug.

=item C<creation_time>

C<dateTime> When the bug was created.

=item C<creator>

C<string> The login name of the person who filed this bug (the reporter).

=item C<deadline>

C<string> The day that this bug is due to be completed, in the format
C<YYYY-MM-DD>.

If you are not in the time-tracking group, this field will not be included
in the return value.

=item C<depends_on>

C<array> of C<int>s. The ids of bugs that this bug "depends on".

=item C<dupe_of>

C<int> The bug ID of the bug that this bug is a duplicate of. If this bug 
isn't a duplicate of any bug, this will be null.

=item C<estimated_time>

C<double> The number of hours that it was estimated that this bug would
take.

If you are not in the time-tracking group, this field will not be included
in the return value.

=item C<groups>

C<array> of C<string>s. The names of all the groups that this bug is in.

=item C<id>

C<int> The unique numeric id of this bug.

=item C<is_cc_accessible>

C<boolean> If true, this bug can be accessed by members of the CC list,
even if they are not in the groups the bug is restricted to.

=item C<is_confirmed>

C<boolean> True if the bug has been confirmed. Usually this means that
the bug has at some point been moved out of the C<UNCONFIRMED> status
and into another open status.

=item C<is_open>

C<boolean> True if this bug is open, false if it is closed.

=item C<is_creator_accessible>

C<boolean> If true, this bug can be accessed by the creator (reporter)
of the bug, even if he or she is not a member of the groups the bug
is restricted to.

=item C<keywords>

C<array> of C<string>s. Each keyword that is on this bug.

=item C<last_change_time>

C<dateTime> When the bug was last changed.

=item C<op_sys>

C<string> The name of the operating system that the bug was filed against.

=item C<platform>

C<string> The name of the platform (hardware) that the bug was filed against.

=item C<priority>

C<string> The priority of the bug.

=item C<product>

C<string> The name of the product this bug is in.

=item C<qa_contact>

C<string> The login name of the current QA Contact on the bug.

=item C<remaining_time>

C<double> The number of hours of work remaining until work on this bug
is complete.

If you are not in the time-tracking group, this field will not be included
in the return value.

=item C<resolution>

C<string> The current resolution of the bug, or an empty string if the bug
is open.

=item C<see_also>

B<UNSTABLE>

C<array> of C<string>s. The URLs in the See Also field on the bug.

=item C<severity>

C<string> The current severity of the bug.

=item C<status>

C<string> The current status of the bug.

=item C<summary>

C<string> The summary of this bug.

=item C<target_milestone>

C<string> The milestone that this bug is supposed to be fixed by, or for
closed bugs, the milestone that it was fixed for.

=item C<update_token>

C<string> The token that you would have to pass to the F<process_bug.cgi>
page in order to update this bug. This changes every time the bug is
updated.

This field is not returned to logged-out users.

=item C<url>

B<UNSTABLE>

C<string> A URL that demonstrates the problem described in
the bug, or is somehow related to the bug report.

=item C<version>

C<string> The version the bug was reported against.

=item C<whiteboard>

C<string> The value of the "status whiteboard" field on the bug.

=item I<custom fields>

Every custom field in this installation will also be included in the
return value. Most fields are returned as C<string>s. However, some
field types have different return values:

=over

=item Bug ID Fields - C<int>

=item Multiple-Selection Fields - C<array> of C<string>s.

=item Date/Time Fields - C<dateTime>

=back 

=back

=item C<faults> B<EXPERIMENTAL>

An array of hashes that contains invalid bug ids with error messages
returned for them. Each hash contains the following items:

=over

=item id

C<int> The numeric bug_id of this bug.

=item faultString 

c<string> This will only be returned for invalid bugs if the C<permissive>
argument was set when calling Bug.get, and it is an error indicating that 
the bug id was invalid.

=item faultCode

c<int> This will only be returned for invalid bugs if the C<permissive>
argument was set when calling Bug.get, and it is the error code for the 
invalid bug error.

=back

=back

=item B<Errors>

=over

=item 100 (Invalid Bug Alias)

If you specified an alias and either: (a) the Bugzilla you're querying
doesn't support aliases or (b) there is no bug with that alias.

=item 101 (Invalid Bug ID)

The bug_id you specified doesn't exist in the database.

=item 102 (Access Denied)

You do not have access to the bug_id you specified.

=back

=item B<History>

=over

=item C<permissive> argument added to this method's params in Bugzilla B<3.4>. 

=item The following properties were added to this method's return values
in Bugzilla B<3.4>:

=over

=item For C<bugs>

=over

=item assigned_to

=item component 

=item dupe_of

=item is_open

=item priority

=item product

=item resolution

=item severity

=item status

=back 

=item C<faults>

=back 

=item In Bugzilla B<4.0>, the following items were added to the C<bugs>
return value: C<blocks>, C<cc>, C<classification>, C<creator>,
C<deadline>, C<depends_on>, C<estimated_time>, C<is_cc_accessible>, 
C<is_confirmed>, C<is_creator_accessible>, C<groups>, C<keywords>,
C<op_sys>, C<platform>, C<qa_contact>, C<remaining_time>, C<see_also>,
C<target_milestone>, C<update_token>, C<url>, C<version>, C<whiteboard>,
and all custom fields.

=back


=back

=head2 history

B<EXPERIMENTAL>

=over

=item B<Description>

Gets the history of changes for particular bugs in the database.

=item B<Params>

=over

=item C<ids>

An array of numbers and strings.

If an element in the array is entirely numeric, it represents a bug_id 
from the Bugzilla database to fetch. If it contains any non-numeric 
characters, it is considered to be a bug alias instead, and the data bug 
with that alias will be loaded. 

Note that it's possible for aliases to be disabled in Bugzilla, in which
case you will be told that you have specified an invalid bug_id if you
try to specify an alias. (It will be error 100.)

=back

=item B<Returns>

A hash containing a single element, C<bugs>. This is an array of hashes,
containing the following keys:

=over

=item id

C<int> The numeric id of the bug.

=item alias

C<string> The alias of this bug. If there is no alias or aliases are 
disabled in this Bugzilla, this will be undef.

=item history

C<array> An array of hashes, each hash having the following keys:

=over

=item when

C<dateTime> The date the bug activity/change happened.

=item who

C<string> The login name of the user who performed the bug change.

=item changes

C<array> An array of hashes which contain all the changes that happened
to the bug at this time (as specified by C<when>). Each hash contains 
the following items:

=over

=item field_name

C<string> The name of the bug field that has changed.

=item removed

C<string> The previous value of the bug field which has been deleted 
by the change.

=item added

C<string> The new value of the bug field which has been added by the change.

=item attachment_id

C<int> The id of the attachment that was changed. This only appears if 
the change was to an attachment, otherwise C<attachment_id> will not be
present in this hash.

=back

=back

=back

=item B<Errors>

The same as L</get>.

=item B<History>

=over

=item Added in Bugzilla B<3.4>.

=back

=back

=head2 possible_duplicates

B<UNSTABLE>

=over

=item B<Description>

Allows a user to find possible duplicate bugs based on a set of keywords
such as a user may use as a bug summary. Optionally the search can be
narrowed down to specific products.

=item B<Params>

=over

=item C<summary> (string) B<Required> - A string of keywords defining
the type of bug you are trying to report.

=item C<product> (array) - One or more product names to narrow the
duplicate search to. If omitted, all bugs are searched.

=back

=item B<Returns>

The same as L</get>.

Note that you will only be returned information about bugs that you
can see. Bugs that you can't see will be entirely excluded from the
results. So, if you want to see private bugs, you will have to first 
log in and I<then> call this method.

=item B<Errors>

=over

=item 50 (Param Required)

You must specify a value for C<summary> containing a string of keywords to 
search for duplicates.

=back

=item B<History>

=over

=item Added in Bugzilla B<4.0>.

=back

=back

=head2 search

B<UNSTABLE>

=over

=item B<Description>

Allows you to search for bugs based on particular criteria.

=item B<Params>

Unless otherwise specified in the description of a parameter, bugs are
returned if they match I<exactly> the criteria you specify in these 
parameters. That is, we don't match against substrings--if a bug is in
the "Widgets" product and you ask for bugs in the "Widg" product, you
won't get anything.

Criteria are joined in a logical AND. That is, you will be returned
bugs that match I<all> of the criteria, not bugs that match I<any> of
the criteria.

Each parameter can be either the type it says, or an array of the types
it says. If you pass an array, it means "Give me bugs with I<any> of
these values." For example, if you wanted bugs that were in either
the "Foo" or "Bar" products, you'd pass:

 product => ['Foo', 'Bar']

Some Bugzillas may treat your arguments case-sensitively, depending
on what database system they are using. Most commonly, though, Bugzilla is 
not case-sensitive with the arguments passed (because MySQL is the 
most-common database to use with Bugzilla, and MySQL is not case sensitive).

=over

=item C<alias>

C<string> The unique alias for this bug. Note that you can search
by alias even if the alias field is disabled in this Bugzilla, but
it's likely that there won't be any aliases set on bugs, in that case.

=item C<assigned_to>

C<string> The login name of a user that a bug is assigned to.

=item C<component>

C<string> The name of the Component that the bug is in. Note that
if there are multiple Components with the same name, and you search
for that name, bugs in I<all> those Components will be returned. If you
don't want this, be sure to also specify the C<product> argument.

=item C<creation_time>

C<dateTime> Searches for bugs that were created at this time or later.
May not be an array.

=item C<creator>

C<string> The login name of the user who created the bug.

You can also pass this argument with the name C<reporter>, for
backwards compatibility with older Bugzillas.

=item C<id>

C<int> The numeric id of the bug.

=item C<last_change_time>

C<dateTime> Searches for bugs that were modified at this time or later.
May not be an array.

=item C<limit>

C<int> Limit the number of results returned to C<int> records. If the limit
is more than zero and higher than the maximum limit set by the administrator,
then the maximum limit will be used instead. If you set the limit equal to zero,
then all matching results will be returned instead.

=item C<offset>

C<int> Used in conjunction with the C<limit> argument, C<offset> defines
the starting position for the search. For example, given a search that
would return 100 bugs, setting C<limit> to 10 and C<offset> to 10 would return
bugs 11 through 20 from the set of 100.

=item C<op_sys>

C<string> The "Operating System" field of a bug.

=item C<platform>

C<string> The Platform (sometimes called "Hardware") field of a bug.

=item C<priority>

C<string> The Priority field on a bug.

=item C<product>

C<string> The name of the Product that the bug is in.

=item C<resolution>

C<string> The current resolution--only set if a bug is closed. You can
find open bugs by searching for bugs with an empty resolution.

=item C<severity>

C<string> The Severity field on a bug.

=item C<status>

C<string> The current status of a bug (not including its resolution,
if it has one, which is a separate field above).

=item C<summary>

C<string> Searches for substrings in the single-line Summary field on
bugs. If you specify an array, then bugs whose summaries match I<any> of the
passed substrings will be returned.

Note that unlike searching in the Bugzilla UI, substrings are not split
on spaces. So searching for C<foo bar> will match "This is a foo bar"
but not "This foo is a bar". C<['foo', 'bar']>, would, however, match
the second item.

=item C<target_milestone>

C<string> The Target Milestone field of a bug. Note that even if this
Bugzilla does not have the Target Milestone field enabled, you can
still search for bugs by Target Milestone. However, it is likely that
in that case, most bugs will not have a Target Milestone set (it
defaults to "---" when the field isn't enabled).

=item C<qa_contact>

C<string> The login name of the bug's QA Contact. Note that even if
this Bugzilla does not have the QA Contact field enabled, you can
still search for bugs by QA Contact (though it is likely that no bug
will have a QA Contact set, if the field is disabled).

=item C<url>

C<string> The "URL" field of a bug.

=item C<version>

C<string> The Version field of a bug.

=item C<whiteboard>

C<string> Search the "Status Whiteboard" field on bugs for a substring.
Works the same as the C<summary> field described above, but searches the
Status Whiteboard field.

=back

=item B<Returns>

The same as L</get>.

Note that you will only be returned information about bugs that you
can see. Bugs that you can't see will be entirely excluded from the
results. So, if you want to see private bugs, you will have to first 
log in and I<then> call this method.

=item B<Errors>

If you specify an invalid value for a particular field, you just won't
get any results for that value.

=over

=item 1000 (Parameters Required)

You may not search without any search terms.

=back

=item B<History>

=over

=item Added in Bugzilla B<3.4>.

=item Searching by C<votes> was removed in Bugzilla B<4.0>.

=item The C<reporter> input parameter was renamed to C<creator>
in Bugzilla B<4.0>.

=item In B<4.2.6> and newer, added the ability to return all results if
C<limit> is set equal to zero. Otherwise maximum results returned are limited
by system configuration.

=back

=back


=head1 Bug Creation and Modification

=head2 create

B<STABLE>

=over

=item B<Description>

This allows you to create a new bug in Bugzilla. If you specify any
invalid fields, an error will be thrown stating which field is invalid.
If you specify any fields you are not allowed to set, they will just be
set to their defaults or ignored.

You cannot currently set all the items here that you can set on enter_bug.cgi.

The WebService interface may allow you to set things other than those listed
here, but realize that anything undocumented is B<UNSTABLE> and will very
likely change in the future.

=item B<Params>

Some params must be set, or an error will be thrown. These params are
marked B<Required>. 

Some parameters can have defaults set in Bugzilla, by the administrator.
If these parameters have defaults set, you can omit them. These parameters
are marked B<Defaulted>.

Clients that want to be able to interact uniformly with multiple
Bugzillas should always set both the params marked B<Required> and those 
marked B<Defaulted>, because some Bugzillas may not have defaults set
for B<Defaulted> parameters, and then this method will throw an error
if you don't specify them.

The descriptions of the parameters below are what they mean when Bugzilla is
being used to track software bugs. They may have other meanings in some
installations.

=over

=item C<product> (string) B<Required> - The name of the product the bug
is being filed against.

=item C<component> (string) B<Required> - The name of a component in the
product above.

=item C<summary> (string) B<Required> - A brief description of the bug being
filed.

=item C<version> (string) B<Required> - A version of the product above;
the version the bug was found in.

=item C<description> (string) B<Defaulted> - The initial description for 
this bug. Some Bugzilla installations require this to not be blank.

=item C<op_sys> (string) B<Defaulted> - The operating system the bug was
discovered on.

=item C<platform> (string) B<Defaulted> - What type of hardware the bug was
experienced on.

=item C<priority> (string) B<Defaulted> - What order the bug will be fixed
in by the developer, compared to the developer's other bugs.

=item C<severity> (string) B<Defaulted> - How severe the bug is.

=item C<alias> (string) - A brief alias for the bug that can be used 
instead of a bug number when accessing this bug. Must be unique in
all of this Bugzilla.

=item C<assigned_to> (username) - A user to assign this bug to, if you
don't want it to be assigned to the component owner.

=item C<cc> (array) - An array of usernames to CC on this bug.

=item C<comment_is_private> (boolean) - If set to true, the description
is private, otherwise it is assumed to be public.

=item C<groups> (array) - An array of group names to put this
bug into. You can see valid group names on the Permissions
tab of the Preferences screen, or, if you are an administrator,
in the Groups control panel.
If you don't specify this argument, then the bug will be added into
all the groups that are set as being "Default" for this product. (If
you want to avoid that, you should specify C<groups> as an empty array.)

=item C<qa_contact> (username) - If this installation has QA Contacts
enabled, you can set the QA Contact here if you don't want to use
the component's default QA Contact.

=item C<status> (string) - The status that this bug should start out as.
Note that only certain statuses can be set on bug creation.

=item C<target_milestone> (string) - A valid target milestone for this
product.

=back

In addition to the above parameters, if your installation has any custom
fields, you can set them just by passing in the name of the field and
its value as a string.

=item B<Returns>

A hash with one element, C<id>. This is the id of the newly-filed bug.

=item B<Errors>

=over

=item 51 (Invalid Object)

You specified a field value that is invalid. The error message will have
more details.

=item 103 (Invalid Alias)

The alias you specified is invalid for some reason. See the error message
for more details.

=item 104 (Invalid Field)

One of the drop-down fields has an invalid value, or a value entered in a
text field is too long. The error message will have more detail.

=item 105 (Invalid Component)

You didn't specify a component.

=item 106 (Invalid Product)

Either you didn't specify a product, this product doesn't exist, or
you don't have permission to enter bugs in this product.

=item 107 (Invalid Summary)

You didn't specify a summary for the bug.

=item 116 (Dependency Loop)

You specified values in the C<blocks> or C<depends_on> fields
that would cause a circular dependency between bugs.

=item 120 (Group Restriction Denied)

You tried to restrict the bug to a group which does not exist, or which
you cannot use with this product.

=item 504 (Invalid User)

Either the QA Contact, Assignee, or CC lists have some invalid user
in them. The error message will have more details.

=back

=item B<History>

=over

=item Before B<3.0.4>, parameters marked as B<Defaulted> were actually
B<Required>, due to a bug in Bugzilla.

=item The C<groups> argument was added in Bugzilla B<4.0>. Before
Bugzilla 4.0, bugs were only added into Mandatory groups by this
method. Since Bugzilla B<4.0.2>, passing an illegal group name will
throw an error. In Bugzilla 4.0 and 4.0.1, illegal group names were
silently ignored.

=item The C<comment_is_private> argument was added in Bugzilla B<4.0>.
Before Bugzilla 4.0, you had to use the undocumented C<commentprivacy>
argument.

=item Error 116 was added in Bugzilla B<4.0>. Before that, dependency
loop errors had a generic code of C<32000>.

=back

=back


=head2 add_attachment

B<UNSTABLE>

=over

=item B<Description>

This allows you to add an attachment to a bug in Bugzilla.

=item B<Params>

=over

=item C<ids>

B<Required> C<array> An array of ints and/or strings--the ids
or aliases of bugs that you want to add this attachment to.
The same attachment and comment will be added to all
these bugs.

=item C<data>

B<Required> C<base64> or C<string> The content of the attachment.
If the content of the attachment is not ASCII text, you must encode
it in base64 and declare it as the C<base64> type.

=item C<file_name>

B<Required> C<string> The "file name" that will be displayed
in the UI for this attachment.

=item C<summary>

B<Required> C<string> A short string describing the
attachment.

=item C<content_type>

B<Required> C<string> The MIME type of the attachment, like
C<text/plain> or C<image/png>.

=item C<comment>

C<string> A comment to add along with this attachment.

=item C<is_patch>

C<boolean> True if Bugzilla should treat this attachment as a patch.
If you specify this, you do not need to specify a C<content_type>.
The C<content_type> of the attachment will be forced to C<text/plain>.

Defaults to False if not specified.

=item C<is_private>

C<boolean> True if the attachment should be private (restricted
to the "insidergroup"), False if the attachment should be public.

Defaults to False if not specified.

=back

=item B<Returns>

A single item C<attachments>, which contains the created
attachments in the same format as the C<attachments> return
value from L</attachments>.

=item B<Errors>

This method can throw all the same errors as L</get>, plus:

=over

=item 600 (Attachment Too Large)

You tried to attach a file that was larger than Bugzilla will accept.

=item 601 (Invalid MIME Type)

You specified a C<content_type> argument that was blank, not a valid
MIME type, or not a MIME type that Bugzilla accepts for attachments.

=item 603 (File Name Not Specified)

You did not specify a valid for the C<file_name> argument.

=item 604 (Summary Required)

You did not specify a value for the C<summary> argument.

=item 606 (Empty Data)

You set the "data" field to an empty string.

=back

=item B<History>

=over

=item Added in Bugzilla B<4.0>.

=item The C<is_url> parameter was removed in Bugzilla B<4.2>.

=back

=back


=head2 add_comment

B<STABLE>

=over

=item B<Description>

This allows you to add a comment to a bug in Bugzilla.

=item B<Params>

=over

=item C<id> (int or string) B<Required> - The id or alias of the bug to append a 
comment to.

=item C<comment> (string) B<Required> - The comment to append to the bug.
If this is empty or all whitespace, an error will be thrown saying that
you did not set the C<comment> parameter.

=item C<is_private> (boolean) - If set to true, the comment is private, 
otherwise it is assumed to be public.

=item C<work_time> (double) - Adds this many hours to the "Hours Worked"
on the bug. If you are not in the time tracking group, this value will
be ignored.


=back

=item B<Returns>

A hash with one element, C<id> whose value is the id of the newly-created comment.

=item B<Errors>

=over

=item 54 (Hours Worked Too Large)

You specified a C<work_time> larger than the maximum allowed value of
C<99999.99>.

=item 100 (Invalid Bug Alias) 

If you specified an alias and either: (a) the Bugzilla you're querying
doesn't support aliases or (b) there is no bug with that alias.

=item 101 (Invalid Bug ID)

The id you specified doesn't exist in the database.

=item 109 (Bug Edit Denied)

You did not have the necessary rights to edit the bug.

=item 113 (Can't Make Private Comments)

You tried to add a private comment, but don't have the necessary rights.

=item 114 (Comment Too Long)

You tried to add a comment longer than the maximum allowed length
(65,535 characters).

=back

=item B<History>

=over

=item Added in Bugzilla B<3.2>.

=item Modified to return the new comment's id in Bugzilla B<3.4>

=item Modified to throw an error if you try to add a private comment
but can't, in Bugzilla B<3.4>.

=item Before Bugzilla B<3.6>, the C<is_private> argument was called
C<private>, and you can still call it C<private> for backwards-compatibility
purposes if you wish.

=item Before Bugzilla B<3.6>, error 54 and error 114 had a generic error
code of 32000.

=back

=back


=head2 update

B<UNSTABLE>

=over

=item B<Description>

Allows you to update the fields of a bug. Automatically sends emails
out about the changes.

=item B<Params>

=over

=item C<ids>

Array of C<int>s or C<string>s. The ids or aliases of the bugs that
you want to modify.

=back

B<Note>: All following fields specify the values you want to set on the
bugs you are updating.

=over

=item C<alias>

(string) The alias of the bug. You can only set this if you are modifying 
a single bug. If there is more than one bug specified in C<ids>, passing in
a value for C<alias> will cause an error to be thrown.

=item C<assigned_to>

C<string> The full login name of the user this bug is assigned to.

=item C<blocks>

=item C<depends_on>

C<hash> These specify the bugs that this bug blocks or depends on,
respectively. To set these, you should pass a hash as the value. The hash
may contain the following fields:

=over

=item C<add> An array of C<int>s. Bug ids to add to this field.

=item C<remove> An array of C<int>s. Bug ids to remove from this field.
If the bug ids are not already in the field, they will be ignored.

=item C<set> An array of C<int>s. An exact set of bug ids to set this
field to, overriding the current value. If you specify C<set>, then C<add>
and  C<remove> will be ignored.

=back

=item C<cc>

C<hash> The users on the cc list. To modify this field, pass a hash, which
may have the following fields:

=over

=item C<add> Array of C<string>s. User names to add to the CC list.
They must be full user names, and an error will be thrown if you pass
in an invalid user name.

=item C<remove> Array of C<string>s. User names to remove from the CC
list. They must be full user names, and an error will be thrown if you
pass in an invalid user name.

=back

=item C<is_cc_accessible>

C<boolean> Whether or not users in the CC list are allowed to access
the bug, even if they aren't in a group that can normally access the bug.

=item C<comment>

C<hash>. A comment on the change. The hash may contain the following fields:

=over

=item C<body> C<string> The actual text of the comment.
B<Note>: For compatibility with the parameters to L</add_comment>,
you can also call this field C<comment>, if you want.

=item C<is_private> C<boolean> Whether the comment is private or not.
If you try to make a comment private and you don't have the permission
to, an error will be thrown.

=back

=item C<comment_is_private>

C<hash> This is how you update the privacy of comments that are already
on a bug. This is a hash, where the keys are the C<int> id of comments (not
their count on a bug, like #1, #2, #3, but their globally-unique id,
as returned by L</comments>) and the value is a C<boolean> which specifies
whether that comment should become private (C<true>) or public (C<false>).

The comment ids must be valid for the bug being updated. Thus, it is not
practical to use this while updating multiple bugs at once, as a single
comment id will never be valid on multiple bugs.

=item C<component>

C<string> The Component the bug is in.

=item C<deadline>

C<string> The Deadline field--a date specifying when the bug must
be completed by, in the format C<YYYY-MM-DD>.

=item C<dupe_of>

C<int> The bug that this bug is a duplicate of. If you want to mark
a bug as a duplicate, the safest thing to do is to set this value
and I<not> set the C<status> or C<resolution> fields. They will
automatically be set by Bugzilla to the appropriate values for
duplicate bugs.

=item C<estimated_time>

C<double> The total estimate of time required to fix the bug, in hours.
This is the I<total> estimate, not the amount of time remaining to fix it.

=item C<groups>

C<hash> The groups a bug is in. To modify this field, pass a hash, which
may have the following fields:

=over

=item C<add> Array of C<string>s. The names of groups to add. Passing
in an invalid group name or a group that you cannot add to this bug will
cause an error to be thrown.

=item C<remove> Array of C<string>s. The names of groups to remove. Passing
in an invalid group name or a group that you cannot remove from this bug
will cause an error to be thrown.

=back

=item C<keywords>

C<hash> Keywords on the bug. To modify this field, pass a hash, which
may have the following fields:

=over

=item C<add> An array of C<strings>s. The names of keywords to add to
the field on the bug. Passing something that isn't a valid keyword name
will cause an error to be thrown. 

=item C<remove> An array of C<string>s. The names of keywords to remove
from the field on the bug. Passing something that isn't a valid keyword
name will cause an error to be thrown.

=item C<set> An array of C<strings>s. An exact set of keywords to set the
field to, on the bug. Passing something that isn't a valid keyword name
will cause an error to be thrown. Specifying C<set> overrides C<add> and
C<remove>.

=back

=item C<op_sys>

C<string> The Operating System ("OS") field on the bug.

=item C<platform>

C<string> The Platform or "Hardware" field on the bug.

=item C<priority>

C<string> The Priority field on the bug.

=item C<product>

C<string> The name of the product that the bug is in. If you change
this, you will probably also want to change C<target_milestone>,
C<version>, and C<component>, since those have different legal
values in every product. 

If you cannot change the C<target_milestone> field, it will be reset to
the default for the product, when you move a bug to a new product.

You may also wish to add or remove groups, as which groups are
valid on a bug depends on the product. Groups that are not valid
in the new product will be automatically removed, and groups which
are mandatory in the new product will be automaticaly added, but no
other automatic group changes will be done.

Note that users can only move a bug into a product if they would
normally have permission to file new bugs in that product.

=item C<qa_contact>

C<string> The full login name of the bug's QA Contact.

=item C<is_creator_accessible>

C<boolean> Whether or not the bug's reporter is allowed to access
the bug, even if he or she isn't in a group that can normally access
the bug.

=item C<remaining_time>

C<double> How much work time is remaining to fix the bug, in hours.
If you set C<work_time> but don't explicitly set C<remaining_time>,
then the C<work_time> will be deducted from the bug's C<remaining_time>.

=item C<reset_assigned_to>

C<boolean> If true, the C<assigned_to> field will be reset to the
default for the component that the bug is in. (If you have set the
component at the same time as using this, then the component used
will be the new component, not the old one.)

=item C<reset_qa_contact>

C<boolean> If true, the C<qa_contact> field will be reset  to the
default for the component that the bug is in. (If you have set the
component at the same time as using this, then the component used
will be the new component, not the old one.)

=item C<resolution>

C<string> The current resolution. May only be set if you are closing
a bug or if you are modifying an already-closed bug. Attempting to set
the resolution to I<any> value (even an empty or null string) on an
open bug will cause an error to be thrown.

If you change the C<status> field to an open status, the resolution
field will automatically be cleared, so you don't have to clear it
manually.

=item C<see_also>

C<hash> The See Also field on a bug, specifying URLs to bugs in other
bug trackers. To modify this field, pass a hash, which may have the
following fields:

=over

=item C<add> An array of C<strings>s. URLs to add to the field.
Each URL must be a valid URL to a bug-tracker, or an error will
be thrown.

=item C<remove> An array of C<string>s. URLs to remove from the field.
Invalid URLs will be ignored.

=back

=item C<severity>

C<string> The Severity field of a bug.

=item C<status>

C<string> The status you want to change the bug to. Note that if
a bug is changing from open to closed, you should also specify
a C<resolution>.

=item C<summary>

C<string> The Summary field of the bug.

=item C<target_milestone>

C<string> The bug's Target Milestone.

=item C<url>

C<string> The "URL" field of a bug.

=item C<version>

C<string> The bug's Version field.

=item C<whiteboard>

C<string> The Status Whiteboard field of a bug.

=item C<work_time>

C<double> The number of hours worked on this bug as part of this change.
If you set C<work_time> but don't explicitly set C<remaining_time>,
then the C<work_time> will be deducted from the bug's C<remaining_time>.

=back

You can also set the value of any custom field by passing its name as
a parameter, and the value to set the field to. For multiple-selection
fields, the value should be an array of strings.

=item B<Returns>

A C<hash> with a single field, "bugs". This points to an array of hashes
with the following fields:

=over

=item C<id>

C<int> The id of the bug that was updated.

=item C<alias>

C<string> The alias of the bug that was updated, if aliases are enabled and
this bug has an alias.

=item C<last_change_time>

C<dateTime> The exact time that this update was done at, for this bug.
If no update was done (that is, no fields had their values changed and
no comment was added) then this will instead be the last time the bug
was updated.

=item C<changes>

C<hash> The changes that were actually done on this bug. The keys are
the names of the fields that were changed, and the values are a hash
with two keys:

=over

=item C<added> (C<string>) The values that were added to this field,
possibly a comma-and-space-separated list if multiple values were added.

=item C<removed> (C<string>) The values that were removed from this
field, possibly a comma-and-space-separated list if multiple values were
removed.

=back

=back

Here's an example of what a return value might look like:

 { 
   bugs => [
     {
       id    => 123,
       alias => 'foo',
       last_change_time => '2010-01-01T12:34:56',
       changes => {
         status => {
           removed => 'NEW',
           added   => 'ASSIGNED'
         },
         keywords => {
           removed => 'bar',
           added   => 'qux, quo, qui', 
         }
       },
     }
   ]
 }

Currently, some fields are not tracked in changes: C<comment>,
C<comment_is_private>, and C<work_time>. This means that they will not
show up in the return value even if they were successfully updated.
This may change in a future version of Bugzilla.

=item B<Errors>

This function can throw all of the errors that L</get>, L</create>,
and L</add_comment> can throw, plus:

=over

=item 50 (Empty Field)

You tried to set some field to be empty, but that field cannot be empty.
The error message will have more details.

=item 52 (Input Not A Number)

You tried to set a numeric field to a value that wasn't numeric.

=item 54 (Number Too Large)

You tried to set a numeric field to a value larger than that field can
accept.

=item 55 (Number Too Small)

You tried to set a negative value in a numeric field that does not accept
negative values.

=item 56 (Bad Date/Time)

You specified an invalid date or time in a date/time field (such as
the C<deadline> field or a custom date/time field).

=item 112 (See Also Invalid)

You attempted to add an invalid value to the C<see_also> field.

=item 115 (Permission Denied)

You don't have permission to change a particular field to a particular value.
The error message will have more detail.

=item 116 (Dependency Loop)

You specified a value in the C<blocks> or C<depends_on> fields that causes
a dependency loop.

=item 117 (Invalid Comment ID)

You specified a comment id in C<comment_is_private> that isn't on this bug.

=item 118 (Duplicate Loop)

You specified a value for C<dupe_of> that causes an infinite loop of
duplicates.

=item 119 (dupe_of Required)

You changed the resolution to C<DUPLICATE> but did not specify a value
for the C<dupe_of> field.

=item 120 (Group Add/Remove Denied)

You tried to add or remove a group that you don't have permission to modify
for this bug, or you tried to add a group that isn't valid in this product.

=item 121 (Resolution Required)

You tried to set the C<status> field to a closed status, but you didn't
specify a resolution.

=item 122 (Resolution On Open Status)

This bug has an open status, but you specified a value for the C<resolution>
field.

=item 123 (Invalid Status Transition)

You tried to change from one status to another, but the status workflow
rules don't allow that change.

=back

=item B<History>

=over

=item Added in Bugzilla B<4.0>.

=back

=back


=head2 update_see_also

B<EXPERIMENTAL>

=over

=item B<Description>

Adds or removes URLs for the "See Also" field on bugs. These URLs must
point to some valid bug in some Bugzilla installation or in Launchpad.

=item B<Params>

=over

=item C<ids>

Array of C<int>s or C<string>s. The ids or aliases of bugs that you want
to modify.

=item C<add>

Array of C<string>s. URLs to Bugzilla bugs. These URLs will be added to
the See Also field. They must be valid URLs to C<show_bug.cgi> in a
Bugzilla installation or to a bug filed at launchpad.net.

If the URLs don't start with C<http://> or C<https://>, it will be assumed
that C<http://> should be added to the beginning of the string.

It is safe to specify URLs that are already in the "See Also" field on
a bug--they will just be silently ignored.

=item C<remove>

Array of C<string>s. These URLs will be removed from the See Also field.
You must specify the full URL that you want removed. However, matching
is done case-insensitively, so you don't have to specify the URL in
exact case, if you don't want to.

If you specify a URL that is not in the See Also field of a particular bug,
it will just be silently ignored. Invaild URLs are currently silently ignored,
though this may change in some future version of Bugzilla.

=back

NOTE: If you specify the same URL in both C<add> and C<remove>, it will
be I<added>. (That is, C<add> overrides C<remove>.)

=item B<Returns>

C<changes>, a hash where the keys are numeric bug ids and the contents
are a hash with one key, C<see_also>. C<see_also> points to a hash, which
contains two keys, C<added> and C<removed>. These are arrays of strings,
representing the actual changes that were made to the bug.

Here's a diagram of what the return value looks like for updating
bug ids 1 and 2:

 {
   changes => {
       1 => {
           see_also => {
               added   => (an array of bug URLs),
               removed => (an array of bug URLs),
           }
       },
       2 => {
           see_also => {
               added   => (an array of bug URLs),
               removed => (an array of bug URLs),
           }
       }
   }
 }

This return value allows you to tell what this method actually did. It is in
this format to be compatible with the return value of a future C<Bug.update>
method.

=item B<Errors>

This method can throw all of the errors that L</get> throws, plus:

=over

=item 109 (Bug Edit Denied)

You did not have the necessary rights to edit the bug.

=item 112 (Invalid Bug URL)

One of the URLs you provided did not look like a valid bug URL.

=item 115 (See Also Edit Denied)

You did not have the necessary rights to edit the See Also field for
this bug.

=back

=item B<History>

=over

=item Added in Bugzilla B<3.4>.

=item Before Bugzilla B<3.6>, error 115 had a generic error code of 32000.

=back

=back
