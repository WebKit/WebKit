# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Bug;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService);

use Bugzilla::Comment;
use Bugzilla::Comment::TagWeights;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field;
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(extract_flags filter filter_wants validate translate);
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Util qw(trick_taint trim diff_arrays detaint_natural);
use Bugzilla::Version;
use Bugzilla::Milestone;
use Bugzilla::Status;
use Bugzilla::Token qw(issue_hash_token);
use Bugzilla::Search;
use Bugzilla::Product;
use Bugzilla::FlagType;
use Bugzilla::Search::Quicksearch;

use List::Util qw(max);
use List::MoreUtils qw(uniq);
use Storable qw(dclone);

#############
# Constants #
#############

use constant PRODUCT_SPECIFIC_FIELDS => qw(version target_milestone component);

use constant DATE_FIELDS => {
    comments => ['new_since'],
    history  => ['new_since'],
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

use constant PUBLIC_METHODS => qw(
    add_attachment
    add_comment
    attachments
    comments
    create
    fields
    get
    history
    legal_values
    possible_duplicates
    render_comment
    search
    search_comment_tags
    update
    update_attachment
    update_comment_tags
    update_see_also
    update_tags
);

use constant ATTACHMENT_MAPPED_SETTERS => {
    file_name => 'filename',
    summary   => 'description',
};

use constant ATTACHMENT_MAPPED_RETURNS => {
    description => 'summary',
    ispatch     => 'is_patch',
    isprivate   => 'is_private',
    isobsolete  => 'is_obsolete',
    filename    => 'file_name',
    mimetype    => 'content_type',
};

###########
# Methods #
###########

sub fields {
    my ($self, $params) = validate(@_, 'ids', 'names');

    Bugzilla->switch_to_shadow_db();

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
             or grep($_ eq $field->name, PRODUCT_SPECIFIC_FIELDS)
             or $field->name eq 'keywords')
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
                    is_active         => $self->type('boolean', $value->is_active),
                });
            }
        }
    }

    elsif ($field_name eq 'bug_status') {
        my @status_all = Bugzilla::Status->get_all;
        my $initial_status = bless({ id => 0, name => '', is_open => 1, sortkey => 0,
                                     can_change_to => Bugzilla::Status->can_change_to },
                                   'Bugzilla::Status');
        unshift(@status_all, $initial_status);

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

    elsif ($field_name eq 'keywords') {
        my @legal_keywords = Bugzilla::Keyword->get_all;
        foreach my $value (@legal_keywords) {
            push (@result, {
               name     => $self->type('string', $value->name),
               description => $self->type('string', $value->description),
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

    my $dbh  = Bugzilla->switch_to_shadow_db();
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

sub render_comment {
    my ($self, $params) = @_;

    unless (defined $params->{text}) {
        ThrowCodeError('params_required',
                       { function => 'Bug.render_comment',
                         params   => ['text'] });
    }

    Bugzilla->switch_to_shadow_db();
    my $bug = $params->{id} ? Bugzilla::Bug->check($params->{id}) : undef;

    my $tmpl = '[% text FILTER quoteUrls(bug) %]';
    my $html;
    my $template = Bugzilla->template;
    $template->process(
        \$tmpl,
        { bug => $bug, text => $params->{text}},
        \$html
    );

    return { html => $html };
}

# Helper for Bug.comments
sub _translate_comment {
    my ($self, $comment, $filters, $types, $prefix) = @_;
    my $attach_id = $comment->is_about_attachment ? $comment->extra_data
                                                  : undef;

    my $comment_hash = {
        id         => $self->type('int', $comment->id),
        bug_id     => $self->type('int', $comment->bug_id),
        creator    => $self->type('email', $comment->author->login),
        time       => $self->type('dateTime', $comment->creation_ts),
        creation_time => $self->type('dateTime', $comment->creation_ts),
        is_private => $self->type('boolean', $comment->is_private),
        text       => $self->type('string', $comment->body_full),
        attachment_id => $self->type('int', $attach_id),
        count      => $self->type('int', $comment->count),
    };

    # Don't load comment tags unless enabled
    if (Bugzilla->params->{'comment_taggers_group'}) {
        $comment_hash->{tags} = [
            map { $self->type('string', $_) }
            @{ $comment->tags }
        ];
    }

    return filter($filters, $comment_hash, $types, $prefix);
}

sub get {
    my ($self, $params) = validate(@_, 'ids');

    Bugzilla->switch_to_shadow_db() unless Bugzilla->user->id;

    my $ids = $params->{ids};
    defined $ids || ThrowCodeError('param_required', { param => 'ids' });

    my (@bugs, @faults, @hashes);

    # Cache permissions for bugs. This highly reduces the number of calls to the DB.
    # visible_bugs() is only able to handle bug IDs, so we have to skip aliases.
    my @int = grep { $_ =~ /^\d+$/ } @$ids;
    Bugzilla->user->visible_bugs(\@int);

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
        push(@bugs, $bug);
        push(@hashes, $self->_bug_to_hash($bug, $params));
    }

    # Set the ETag before inserting the update tokens
    # since the tokens will always be unique even if
    # the data has not changed.
    $self->bz_etag(\@hashes);

    $self->_add_update_tokens($params, \@bugs, \@hashes);

    return { bugs => \@hashes, faults => \@faults };
}

# this is a function that gets bug activity for list of bug ids 
# it can be called as the following:
# $call = $rpc->call( 'Bug.history', { ids => [1,2] });
sub history {
    my ($self, $params) = validate(@_, 'ids');

    Bugzilla->switch_to_shadow_db();

    my $ids = $params->{ids};
    defined $ids || ThrowCodeError('param_required', { param => 'ids' });

    my %api_name = reverse %{ Bugzilla::Bug::FIELD_MAP() };
    $api_name{'bug_group'} = 'groups';

    my @return;
    foreach my $bug_id (@$ids) {
        my %item;
        my $bug = Bugzilla::Bug->check($bug_id);
        $bug_id = $bug->id;
        $item{id} = $self->type('int', $bug_id);

        my ($activity) = $bug->get_activity(undef, $params->{new_since});

        my @history;
        foreach my $changeset (@$activity) {
            my %bug_history;
            $bug_history{when} = $self->type('dateTime', $changeset->{when});
            $bug_history{who}  = $self->type('string', $changeset->{who});
            $bug_history{changes} = [];
            foreach my $change (@{ $changeset->{changes} }) {
                my $api_field = $api_name{$change->{fieldname}} || $change->{fieldname};
                my $attach_id = delete $change->{attachid};
                if ($attach_id) {
                    $change->{attachment_id} = $self->type('int', $attach_id);
                }
                $change->{removed} = $self->type('string', $change->{removed});
                $change->{added}   = $self->type('string', $change->{added});
                $change->{field_name} = $self->type('string', $api_field);
                delete $change->{fieldname};
                push (@{$bug_history{changes}}, $change);
            }
            
            push (@history, \%bug_history);
        }

        $item{history} = \@history;

        # alias is returned in case users passes a mixture of ids and aliases
        # then they get to know which bug activity relates to which value  
        # they passed
        $item{alias} = [ map { $self->type('string', $_) } @{ $bug->alias } ];

        push(@return, \%item);
    }

    return { bugs => \@return };
}

sub search {
    my ($self, $params) = @_;
    my $user = Bugzilla->user;
    my $dbh  = Bugzilla->dbh;

    Bugzilla->switch_to_shadow_db();

    my $match_params = dclone($params);
    delete $match_params->{include_fields};
    delete $match_params->{exclude_fields};

    # Determine whether this is a quicksearch query
    if (exists $match_params->{quicksearch}) {
        my $quicksearch = quicksearch($match_params->{'quicksearch'});
        my $cgi = Bugzilla::CGI->new($quicksearch);
        $match_params = $cgi->Vars;
    }

    if ( defined($match_params->{offset}) and !defined($match_params->{limit}) ) {
        ThrowCodeError('param_required',
                       { param => 'limit', function => 'Bug.search()' });
    }

    my $max_results = Bugzilla->params->{max_search_results};
    unless (defined $match_params->{limit} && $match_params->{limit} == 0) {
        if (!defined $match_params->{limit} || $match_params->{limit} > $max_results) {
            $match_params->{limit} = $max_results;
        }
    }
    else {
        delete $match_params->{limit};
        delete $match_params->{offset};
    }

    $match_params = Bugzilla::Bug::map_fields($match_params);

    my %options = ( fields => ['bug_id'] );

    # Find the highest custom field id
    my @field_ids = grep(/^f(\d+)$/, keys %$match_params);
    my $last_field_id = @field_ids ? max @field_ids + 1 : 1;

    # Do special search types for certain fields.
    if (my $change_when = delete $match_params->{'delta_ts'}) {
        $match_params->{"f${last_field_id}"} = 'delta_ts';
        $match_params->{"o${last_field_id}"} = 'greaterthaneq';
        $match_params->{"v${last_field_id}"} = $change_when;
        $last_field_id++;
    }
    if (my $creation_when = delete $match_params->{'creation_ts'}) {
        $match_params->{"f${last_field_id}"} = 'creation_ts';
        $match_params->{"o${last_field_id}"} = 'greaterthaneq';
        $match_params->{"v${last_field_id}"} = $creation_when;
        $last_field_id++;
    }

    # Some fields require a search type such as short desc, keywords, etc.
    foreach my $param (qw(short_desc longdesc status_whiteboard bug_file_loc)) {
        if (defined $match_params->{$param} && !defined $match_params->{$param . '_type'}) {
            $match_params->{$param . '_type'} = 'allwordssubstr';
        }
    }
    if (defined $match_params->{'keywords'} && !defined $match_params->{'keywords_type'}) {
        $match_params->{'keywords_type'} = 'allwords';
    }

    # Backwards compatibility with old method regarding role search
    $match_params->{'reporter'} = delete $match_params->{'creator'} if $match_params->{'creator'};
    foreach my $role (qw(assigned_to reporter qa_contact longdesc cc)) {
        next if !exists $match_params->{$role};
        my $value = delete $match_params->{$role};
        $match_params->{"f${last_field_id}"} = $role;
        $match_params->{"o${last_field_id}"} = "anywordssubstr";
        $match_params->{"v${last_field_id}"} = ref $value ? join(" ", @{$value}) : $value;
        $last_field_id++;
    }

    # If no other parameters have been passed other than limit and offset
    # then we throw error if system is configured to do so.
    if (!grep(!/^(limit|offset)$/, keys %$match_params)
        && !Bugzilla->params->{search_allow_no_criteria})
    {
        ThrowUserError('buglist_parameters_required');
    }

    $options{order}  = [ split(/\s*,\s*/, delete $match_params->{order}) ] if $match_params->{order};
    $options{params} = $match_params;

    my $search = new Bugzilla::Search(%options);
    my ($data) = $search->data;

    if (!scalar @$data) {
        return { bugs => [] };
    }

    # Search.pm won't return bugs that the user shouldn't see so no filtering is needed.
    my @bug_ids = map { $_->[0] } @$data;
    my %bug_objects = map { $_->id => $_ } @{ Bugzilla::Bug->new_from_list(\@bug_ids) };
    my @bugs = map { $bug_objects{$_} } @bug_ids;
    @bugs = map { $self->_bug_to_hash($_, $params) } @bugs;

    return { bugs => \@bugs };
}

sub possible_duplicates {
    my ($self, $params) = validate(@_, 'products');
    my $user = Bugzilla->user;

    Bugzilla->switch_to_shadow_db();

    # Undo the array-ification that validate() does, for "summary".
    $params->{summary} || ThrowCodeError('param_required',
        { function => 'Bug.possible_duplicates', param => 'summary' });

    my @products;
    foreach my $name (@{ $params->{'products'} || [] }) {
        my $object = $user->can_enter_product($name, THROW_ERROR);
        push(@products, $object);
    }

    my $possible_dupes = Bugzilla::Bug->possible_duplicates(
        { summary => $params->{summary}, products => \@products,
          limit   => $params->{limit} });
    my @hashes = map { $self->_bug_to_hash($_, $params) } @$possible_dupes;
    $self->_add_update_tokens($params, $possible_dupes, \@hashes);
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

    my @bugs = map { Bugzilla::Bug->check_for_edit($_) } @$ids;

    my %values = %$params;
    $values{other_bugs} = \@bugs;

    if (exists $values{comment} and exists $values{comment}{comment}) {
        $values{comment}{body} = delete $values{comment}{comment};
    }

    # Prevent bugs that could be triggered by specifying fields that
    # have valid "set_" functions in Bugzilla::Bug, but shouldn't be
    # called using those field names.
    delete $values{dependencies};

    # For backwards compatibility, treat alias string or array as a set action
    if (exists $values{alias}) {
        if (not ref $values{alias}) {
            $values{alias} = { set => [ $values{alias} ] };
        }
        elsif (ref $values{alias} eq 'ARRAY') {
            $values{alias} = { set => $values{alias} };
        }
    }

    my $flags = delete $values{flags};

    foreach my $bug (@bugs) {
        $bug->set_all(\%values);
        if ($flags) {
            my ($old_flags, $new_flags) = extract_flags($flags, $bug);
            $bug->set_flags($old_flags, $new_flags);
        }
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
        $hash{alias} = [ map { $self->type('string', $_) } @{ $bug->alias } ];

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
    my $dbh = Bugzilla->dbh;

    Bugzilla->login(LOGIN_REQUIRED);

    $params = Bugzilla::Bug::map_fields($params);

    my $flags = delete $params->{flags};

    # We start a nested transaction in case flag setting fails
    # we want the bug creation to roll back as well.
    $dbh->bz_start_transaction();

    my $bug = Bugzilla::Bug->create($params);

    # Set bug flags
    if ($flags) {
        my ($flags, $new_flags) = extract_flags($flags, $bug);
        $bug->set_flags($flags, $new_flags);
        $bug->update($bug->creation_ts);
    }

    $dbh->bz_commit_transaction();

    $bug->send_changes();

    return { id => $self->type('int', $bug->bug_id) };
}

sub legal_values {
    my ($self, $params) = @_;

    Bugzilla->switch_to_shadow_db();

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

    my @bugs = map { Bugzilla::Bug->check_for_edit($_) } @{ $params->{ids} };

    my @created;
    $dbh->bz_start_transaction();
    my $timestamp = $dbh->selectrow_array('SELECT LOCALTIMESTAMP(0)');

    my $flags = delete $params->{flags};

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

        if ($flags) {
            my ($old_flags, $new_flags) = extract_flags($flags, $bug, $attachment);
            $attachment->set_flags($old_flags, $new_flags);
        }

        $attachment->update($timestamp);
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

    my @created_ids = map { $_->id } @created;

    return { ids => \@created_ids };
}

sub update_attachment {
    my ($self, $params) = validate(@_, 'ids');

    my $user = Bugzilla->login(LOGIN_REQUIRED);
    my $dbh = Bugzilla->dbh;

    my $ids = delete $params->{ids};
    defined $ids || ThrowCodeError('param_required', { param => 'ids' });

    # Some fields cannot be sent to set_all
    foreach my $key (qw(login password token)) {
        delete $params->{$key};
    }

    $params = translate($params, ATTACHMENT_MAPPED_SETTERS);

    # Get all the attachments, after verifying that they exist and are editable
    my @attachments = ();
    my %bugs = ();
    foreach my $id (@$ids) {
        my $attachment = Bugzilla::Attachment->new($id)
          || ThrowUserError("invalid_attach_id", { attach_id => $id });
        my $bug = $attachment->bug;
        $attachment->_check_bug;

        push @attachments, $attachment;
        $bugs{$bug->id} = $bug;
    }

    my $flags = delete $params->{flags};
    my $comment = delete $params->{comment};

    # Update the values
    foreach my $attachment (@attachments) {
        my ($update_flags, $new_flags) = $flags
            ? extract_flags($flags, $attachment->bug, $attachment)
            : ([], []);
        if ($attachment->validate_can_edit) {
            $attachment->set_all($params);
            $attachment->set_flags($update_flags, $new_flags) if $flags;
        }
        elsif (scalar @$update_flags && !scalar(@$new_flags) && !scalar keys %$params) {
            # Requestees can set flags targetted to them, even if they cannot
            # edit the attachment. Flag setters can edit their own flags too.
            my %flag_list = map { $_->{id} => $_ } @$update_flags;
            my $flag_objs = Bugzilla::Flag->new_from_list([ keys %flag_list ]);
            my @editable_flags;
            foreach my $flag_obj (@$flag_objs) {
                if ($flag_obj->setter_id == $user->id
                    || ($flag_obj->requestee_id && $flag_obj->requestee_id == $user->id))
                {
                    push(@editable_flags, $flag_list{$flag_obj->id});
                }
            }
            if (!scalar @editable_flags) {
                ThrowUserError("illegal_attachment_edit", { attach_id => $attachment->id });
            }
            $attachment->set_flags(\@editable_flags, []);
        }
        else {
            ThrowUserError("illegal_attachment_edit", { attach_id => $attachment->id });
        }
    }

    $dbh->bz_start_transaction();

    # Do the actual update and get information to return to user
    my @result;
    foreach my $attachment (@attachments) {
        my $changes = $attachment->update();

        if ($comment = trim($comment)) {
            $attachment->bug->add_comment($comment,
                { isprivate  => $attachment->isprivate,
                  type       => CMT_ATTACHMENT_UPDATED,
                  extra_data => $attachment->id });
        }

        $changes = translate($changes, ATTACHMENT_MAPPED_RETURNS);

        my %hash = (
            id               => $self->type('int', $attachment->id),
            last_change_time => $self->type('dateTime', $attachment->modification_time),
            changes          => {},
        );

        foreach my $field (keys %$changes) {
            my $change = $changes->{$field};

            # We normalize undef to an empty string, so that the API
            # stays consistent for things like Deadline that can become
            # empty.
            $hash{changes}->{$field} = {
                removed => $self->type('string', $change->[0] // ''),
                added   => $self->type('string', $change->[1] // '')
            };
        }

        push(@result, \%hash);
    }

    $dbh->bz_commit_transaction();

    # Email users about the change
    foreach my $bug (values %bugs) {
        $bug->update();
        $bug->send_changes();
    }

    # Return the information to the user
    return { attachments => \@result };
}

sub add_comment {
    my ($self, $params) = @_;

    # The user must login in order add a comment
    my $user = Bugzilla->login(LOGIN_REQUIRED);

    # Check parameters
    defined $params->{id} 
        || ThrowCodeError('param_required', { param => 'id' }); 
    my $comment = $params->{comment}; 
    (defined $comment && trim($comment) ne '')
        || ThrowCodeError('param_required', { param => 'comment' });
    
    my $bug = Bugzilla::Bug->check_for_edit($params->{id});

    # Backwards-compatibility for versions before 3.6    
    if (defined $params->{private}) {
        $params->{is_private} = delete $params->{private};
    }
    # Append comment
    $bug->add_comment($comment, { isprivate => $params->{is_private},
                                  work_time => $params->{work_time} });
    $bug->update();

    my $new_comment_id = $bug->{added_comments}[0]->id;

    # Send mail.
    Bugzilla::BugMail::Send($bug->bug_id, { changer => $user });

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
        my $bug = Bugzilla::Bug->check_for_edit($id);
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

    Bugzilla->switch_to_shadow_db() unless Bugzilla->user->id;

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

sub update_tags {
    my ($self, $params) = @_;

    Bugzilla->login(LOGIN_REQUIRED);

    my $ids  = $params->{ids};
    my $tags = $params->{tags};

    ThrowCodeError('param_required',
                   { function => 'Bug.update_tags', 
                     param    => 'ids' }) if !defined $ids;

    ThrowCodeError('param_required',
                   { function => 'Bug.update_tags', 
                     param    => 'tags' }) if !defined $tags;

    my %changes;
    foreach my $bug_id (@$ids) {
        my $bug = Bugzilla::Bug->check($bug_id);
        my @old_tags = @{ $bug->tags };

        $bug->remove_tag($_) foreach @{ $tags->{remove} || [] };
        $bug->add_tag($_) foreach @{ $tags->{add} || [] };

        my ($removed, $added) = diff_arrays(\@old_tags, $bug->tags);

        my @removed = map { $self->type('string', $_) } @$removed;
        my @added   = map { $self->type('string', $_) } @$added;

        $changes{$bug->id}->{tags} = {
            removed => \@removed,
            added   => \@added
        };
    }

    return { changes => \%changes };
}

sub update_comment_tags {
    my ($self, $params) = @_;

    my $user = Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->params->{'comment_taggers_group'}
        || ThrowUserError("comment_tag_disabled");
    $user->can_tag_comments
        || ThrowUserError("auth_failure",
                          { group  => Bugzilla->params->{'comment_taggers_group'},
                            action => "update",
                            object => "comment_tags" });

    my $comment_id  = $params->{comment_id}
        // ThrowCodeError('param_required',
                          { function => 'Bug.update_comment_tags',
                            param    => 'comment_id' });

    ThrowCodeError('param_integer_required', { function => 'Bug.update_comment_tags',
                                               param => 'comment_id' })
      unless $comment_id =~ /^[0-9]+$/;

    my $comment = Bugzilla::Comment->new($comment_id)
        || return [];
    $comment->bug->check_is_visible();
    if ($comment->is_private && !$user->is_insider) {
        ThrowUserError('comment_is_private', { id => $comment_id });
    }

    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    foreach my $tag (@{ $params->{add} || [] }) {
        $comment->add_tag($tag) if defined $tag;
    }
    foreach my $tag (@{ $params->{remove} || [] }) {
        $comment->remove_tag($tag) if defined $tag;
    }
    $comment->update();
    $dbh->bz_commit_transaction();

    return $comment->tags;
}

sub search_comment_tags {
    my ($self, $params) = @_;

    Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->params->{'comment_taggers_group'}
        || ThrowUserError("comment_tag_disabled");
    Bugzilla->user->can_tag_comments
        || ThrowUserError("auth_failure", { group  => Bugzilla->params->{'comment_taggers_group'},
                                            action => "search",
                                            object => "comment_tags"});

    my $query = $params->{query};
    $query
        // ThrowCodeError('param_required', { param => 'query' });
    my $limit = $params->{limit} || 7;
    detaint_natural($limit)
        || ThrowCodeError('param_must_be_numeric', { param    => 'limit',
                                                     function => 'Bug.search_comment_tags' });


    my $tags = Bugzilla::Comment::TagWeights->match({
        WHERE => {
            'tag LIKE ?' => "\%$query\%",
        },
        LIMIT => $limit,
    });
    return [ map { $_->tag } @$tags ];
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
    my %item = %{ filter $params, {
        # No need to format $bug->deadline specially, because Bugzilla::Bug
        # already does it for us.
        deadline         => $self->type('string', $bug->deadline),
        id               => $self->type('int', $bug->bug_id),
        is_confirmed     => $self->type('boolean', $bug->everconfirmed),
        op_sys           => $self->type('string', $bug->op_sys),
        platform         => $self->type('string', $bug->rep_platform),
        priority         => $self->type('string', $bug->priority),
        resolution       => $self->type('string', $bug->resolution),
        severity         => $self->type('string', $bug->bug_severity),
        status           => $self->type('string', $bug->bug_status),
        summary          => $self->type('string', $bug->short_desc),
        target_milestone => $self->type('string', $bug->target_milestone),
        url              => $self->type('string', $bug->bug_file_loc),
        version          => $self->type('string', $bug->version),
        whiteboard       => $self->type('string', $bug->status_whiteboard),
    } };

    # First we handle any fields that require extra work (such as date parsing
    # or SQL calls).
    if (filter_wants $params, 'alias') {
        $item{alias} = [ map { $self->type('string', $_) } @{ $bug->alias } ];
    }
    if (filter_wants $params, 'assigned_to') {
        $item{'assigned_to'} = $self->type('email', $bug->assigned_to->login);
        $item{'assigned_to_detail'} = $self->_user_to_hash($bug->assigned_to, $params, undef, 'assigned_to');
    }
    if (filter_wants $params, 'blocks') {
        my @blocks = map { $self->type('int', $_) } @{ $bug->blocked };
        $item{'blocks'} = \@blocks;
    }
    if (filter_wants $params, 'classification') {
        $item{classification} = $self->type('string', $bug->classification);
    }
    if (filter_wants $params, 'component') {
        $item{component} = $self->type('string', $bug->component);
    }
    if (filter_wants $params, 'cc') {
        my @cc = map { $self->type('email', $_) } @{ $bug->cc };
        $item{'cc'} = \@cc;
        $item{'cc_detail'} = [ map { $self->_user_to_hash($_, $params, undef, 'cc') } @{ $bug->cc_users } ];
    }
    if (filter_wants $params, 'creation_time') {
        $item{'creation_time'} = $self->type('dateTime', $bug->creation_ts);
    }
    if (filter_wants $params, 'creator') {
        $item{'creator'} = $self->type('email', $bug->reporter->login);
        $item{'creator_detail'} = $self->_user_to_hash($bug->reporter, $params, undef, 'creator');
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
    if (filter_wants $params, 'last_change_time') {
        $item{'last_change_time'} = $self->type('dateTime', $bug->delta_ts);
    }
    if (filter_wants $params, 'product') {
        $item{product} = $self->type('string', $bug->product);
    }
    if (filter_wants $params, 'qa_contact') {
        my $qa_login = $bug->qa_contact ? $bug->qa_contact->login : '';
        $item{'qa_contact'} = $self->type('email', $qa_login);
        if ($bug->qa_contact) {
            $item{'qa_contact_detail'} = $self->_user_to_hash($bug->qa_contact, $params, undef, 'qa_contact');
        }
    }
    if (filter_wants $params, 'see_also') {
        my @see_also = map { $self->type('string', $_->name) }
                       @{ $bug->see_also };
        $item{'see_also'} = \@see_also;
    }
    if (filter_wants $params, 'flags') {
        $item{'flags'} = [ map { $self->_flag_to_hash($_) } @{$bug->flags} ];
    }
    if (filter_wants $params, 'tags', 'extra') {
        $item{'tags'} = $bug->tags;
    }

    # And now custom fields
    my @custom_fields = Bugzilla->active_custom_fields;
    foreach my $field (@custom_fields) {
        my $name = $field->name;
        next if !filter_wants($params, $name, ['default', 'custom']);
        if ($field->type == FIELD_TYPE_BUG_ID) {
            $item{$name} = $self->type('int', $bug->$name);
        }
        elsif ($field->type == FIELD_TYPE_DATETIME
               || $field->type == FIELD_TYPE_DATE)
        {
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
        if (filter_wants $params, 'estimated_time') {
            $item{'estimated_time'} = $self->type('double', $bug->estimated_time);
        }
        if (filter_wants $params, 'remaining_time') {
            $item{'remaining_time'} = $self->type('double', $bug->remaining_time);
        }
        if (filter_wants $params, 'actual_time') {
            $item{'actual_time'} = $self->type('double', $bug->actual_time);
        }
    }

    # The "accessible" bits go here because they have long names and it
    # makes the code look nicer to separate them out.
    if (filter_wants $params, 'is_cc_accessible') {
        $item{'is_cc_accessible'} = $self->type('boolean', $bug->cclist_accessible);
    }
    if (filter_wants $params, 'is_creator_accessible') {
        $item{'is_creator_accessible'} = $self->type('boolean', $bug->reporter_accessible);
    }

    return \%item;
}

sub _user_to_hash {
    my ($self, $user, $filters, $types, $prefix) = @_;
    my $item = filter $filters, {
        id        => $self->type('int', $user->id),
        real_name => $self->type('string', $user->name),
        name      => $self->type('email', $user->login),
        email     => $self->type('email', $user->email),
    }, $types, $prefix;
    return $item;
}

sub _attachment_to_hash {
    my ($self, $attach, $filters, $types, $prefix) = @_;

    my $item = filter $filters, {
        creation_time    => $self->type('dateTime', $attach->attached),
        last_change_time => $self->type('dateTime', $attach->modification_time),
        id               => $self->type('int', $attach->id),
        bug_id           => $self->type('int', $attach->bug_id),
        file_name        => $self->type('string', $attach->filename),
        summary          => $self->type('string', $attach->description),
        content_type     => $self->type('string', $attach->contenttype),
        is_private       => $self->type('int', $attach->isprivate),
        is_obsolete      => $self->type('int', $attach->isobsolete),
        is_patch         => $self->type('int', $attach->ispatch),
    }, $types, $prefix;

    # creator requires an extra lookup, so we only send them if
    # the filter wants them.
    if (filter_wants $filters, 'creator', $types, $prefix) {
        $item->{'creator'} = $self->type('email', $attach->attacher->login);
    }

    if (filter_wants $filters, 'data', $types, $prefix) {
        $item->{'data'} = $self->type('base64', $attach->data);
    }

    if (filter_wants $filters, 'size', $types, $prefix) {
        $item->{'size'} = $self->type('int', $attach->datasize);
    }

    if (filter_wants $filters, 'flags', $types, $prefix) {
        $item->{'flags'} = [ map { $self->_flag_to_hash($_) } @{$attach->flags} ];
    }

    return $item;
}

sub _flag_to_hash {
    my ($self, $flag) = @_;

    my $item = {
        id                => $self->type('int', $flag->id),
        name              => $self->type('string', $flag->name),
        type_id           => $self->type('int', $flag->type_id),
        creation_date     => $self->type('dateTime', $flag->creation_date), 
        modification_date => $self->type('dateTime', $flag->modification_date), 
        status            => $self->type('string', $flag->status)
    };

    foreach my $field (qw(setter requestee)) {
        my $field_id = $field . "_id";
        $item->{$field} = $self->type('email', $flag->$field->login)
            if $flag->$field_id;
    }

    return $item;
}

sub _add_update_tokens {
    my ($self, $params, $bugs, $hashes) = @_;

    return if !Bugzilla->user->id;
    return if !filter_wants($params, 'update_token');

    for(my $i = 0; $i < @$bugs; $i++) {
        my $token = issue_hash_token([$bugs->[$i]->id, $bugs->[$i]->delta_ts]);
        $hashes->[$i]->{'update_token'} = $self->type('string', $token);
    }
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

Although the data input and output is the same for JSONRPC, XMLRPC and REST,
the directions for how to access the data via REST is noted in each method
where applicable.

=head1 Utility Functions

=head2 fields

B<UNSTABLE>

=over

=item B<Description>

Get information about valid bug fields, including the lists of legal values
for each field.

=item B<REST>

You have several options for retreiving information about fields. The first
part is the request method and the rest is the related path needed.

To get information about all fields:

GET /rest/field/bug

To get information related to a single field:

GET /rest/field/bug/<id_or_name>

The returned data format is the same as below.

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

=item C<8> Keywords

=item C<9> Date

=item C<10> Integer value

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
populated for the C<component>, C<version>, C<target_milestone>, and C<keywords>
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

=item C<is_active>

C<boolean> This value is defined only for certain product specific fields
such as version, target_milestone or component. When true, the value is active,
otherwise the value is not active.

=item C<description>

C<string> The description of the value. This item is only included for the
C<keywords> field.

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

=item C<is_active> return key for C<values> was added in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>

=back

=back

=head2 legal_values

B<DEPRECATED> - Use L</fields> instead.

=over

=item B<Description>

Tells you what values are allowed for a particular field.

=item B<REST>

To get information on the values for a field based on field name:

GET /rest/field/bug/<field_name>/values

To get information based on field name and a specific product:

GET /rest/field/bug/<field_name>/<product_id>/values

The returned data format is the same as below.

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

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

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

=item B<REST>

To get all current attachments for a bug:

GET /rest/bug/<bug_id>/attachment

To get a specific attachment based on attachment ID:

GET /rest/bug/attachment/<attachment_id>

The returned data format is the same as below.

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

=item C<size>

C<int> The length (in bytes) of the attachment.

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

=item C<flags>

An array of hashes containing the information about flags currently set
for each attachment. Each flag hash contains the following items:

=over

=item C<id> 

C<int> The id of the flag.

=item C<name>

C<string> The name of the flag.

=item C<type_id>

C<int> The type id of the flag.

=item C<creation_date>

C<dateTime> The timestamp when this flag was originally created.

=item C<modification_date>

C<dateTime> The timestamp when the flag was last modified.

=item C<status>

C<string> The current status of the flag.

=item C<setter>

C<string> The login name of the user who created or last modified the flag.

=item C<requestee>

C<string> The login name of the user this flag has been requested to be granted or denied.
Note, this field is only returned if a requestee is set.

=back

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

=item The C<size> return value was added in Bugzilla B<4.4>.

=item The C<flags> array was added in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back


=head2 comments

B<STABLE>

=over

=item B<Description>

This allows you to get data about comments, given a list of bugs 
and/or comment ids.

=item B<REST>

To get all comments for a particular bug using the bug ID or alias:

GET /rest/bug/<id_or_alias>/comment

To get a specific comment based on the comment ID:

GET /rest/bug/comment/<comment_id>

The returned data format is the same as below.

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

=item count

C<int> The number of the comment local to the bug. The Description is 0,
comments start with 1.

=item text

C<string> The actual text of the comment.

=item creator

C<string> The login name of the comment's author.

=item time

C<dateTime> The time (in Bugzilla's timezone) that the comment was added.

=item creation_time

C<dateTime> This is exactly same as the C<time> key. Use this field instead of
C<time> for consistency with other methods including L</get> and L</attachments>.
For compatibility, C<time> is still usable. However, please note that C<time>
may be deprecated and removed in a future release.

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

=item C<count> was added to the return value in Bugzilla B<4.4>.

=item C<creation_time> was added in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back


=head2 get

B<STABLE>

=over

=item B<Description>

Gets information about particular bugs in the database.

=item B<REST>

To get information about a particular bug using its ID or alias:

GET /rest/bug/<id_or_alias>

The returned data format is the same as below.

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

These fields are returned by default or by specifying C<_default>
in C<include_fields>.

=over

=item C<actual_time>

C<double> The total number of hours that this bug has taken (so far).

If you are not in the time-tracking group, this field will not be included
in the return value.

=item C<alias>

C<array> of C<string>s The unique aliases of this bug. An empty array will be
returned if this bug has no aliases.

=item C<assigned_to>

C<string> The login name of the user to whom the bug is assigned.

=item C<assigned_to_detail> 

C<hash> A hash containing detailed user information for the assigned_to. To see the 
keys included in the user detail hash, see below.

=item C<blocks>

C<array> of C<int>s. The ids of bugs that are "blocked" by this bug.

=item C<cc>

C<array> of C<string>s. The login names of users on the CC list of this
bug.

=item C<cc_detail>

C<array> of hashes containing detailed user information for each of the cc list
members. To see the keys included in the user detail hash, see below.

=item C<classification>

C<string> The name of the current classification the bug is in.

=item C<component>

C<string> The name of the current component of this bug.

=item C<creation_time>

C<dateTime> When the bug was created.

=item C<creator>

C<string> The login name of the person who filed this bug (the reporter).

=item C<creator_detail> 

C<hash> A hash containing detailed user information for the creator. To see the 
keys included in the user detail hash, see below.

=item C<deadline>

C<string> The day that this bug is due to be completed, in the format
C<YYYY-MM-DD>.

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

=item C<flags>

An array of hashes containing the information about flags currently set
for the bug. Each flag hash contains the following items:

=over

=item C<id> 

C<int> The id of the flag.

=item C<name>

C<string> The name of the flag.

=item C<type_id>

C<int> The type id of the flag.

=item C<creation_date>

C<dateTime> The timestamp when this flag was originally created.

=item C<modification_date>

C<dateTime> The timestamp when the flag was last modified.

=item C<status>

C<string> The current status of the flag.

=item C<setter>

C<string> The login name of the user who created or last modified the flag.

=item C<requestee>

C<string> The login name of the user this flag has been requested to be granted or denied.
Note, this field is only returned if a requestee is set.

=back

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
of the bug, even if they are not a member of the groups the bug
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

=item C<qa_contact_detail>

C<hash> A hash containing detailed user information for the qa_contact. To see the
keys included in the user detail hash, see below.

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
field types have different return values.

Normally custom fields are returned by default similar to normal bug
fields or you can specify only custom fields by using C<_custom> in
C<include_fields>.

=over

=item Bug ID Fields - C<int>

=item Multiple-Selection Fields - C<array> of C<string>s.

=item Date/Time Fields - C<dateTime>

=back 

=item I<user detail hashes> 

Each user detail hash contains the following items:

=over

=item C<id>

C<int> The user id for this user.

=item C<real_name>

C<string> The 'real' name for this user, if any.

=item C<name>

C<string> The user's Bugzilla login.

=item C<email>

C<string> The user's email address. Currently this is the same value as the name.

=back

=back

These fields are returned only by specifying "_extra" or the field name in "include_fields".

=over

=item C<tags>

C<array> of C<string>s.  Each array item is a tag name.

Note that tags are personal to the currently logged in user.

=back

=item C<faults> B<EXPERIMENTAL>

An array of hashes that contains invalid bug ids with error messages
returned for them. Each hash contains the following items:

=over

=item id

C<int> The numeric bug_id of this bug.

=item faultString 

C<string> This will only be returned for invalid bugs if the C<permissive>
argument was set when calling Bug.get, and it is an error indicating that 
the bug id was invalid.

=item faultCode

C<int> This will only be returned for invalid bugs if the C<permissive>
argument was set when calling Bug.get, and it is the error code for the 
invalid bug error.

=back

=back

=item B<Errors>

=over

=item 100 (Invalid Bug Alias)

If you specified an alias and there is no bug with that alias.

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

=item The C<flags> array was added in Bugzilla B<4.4>.

=item The C<actual_time> item was added to the C<bugs> return value
in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=item In Bugzilla B<5.0>, the following items were added to the bugs return value: C<assigned_to_detail>, C<creator_detail>, C<qa_contact_detail>. 

=back

=back

=head2 history

B<EXPERIMENTAL>

=over

=item B<Description>

Gets the history of changes for particular bugs in the database.

=item B<REST>

To get the history for a specific bug ID:

GET /rest/bug/<bug_id>/history

The returned data format will be the same as below.

=item B<Params>

=over

=item C<ids>

An array of numbers and strings.

If an element in the array is entirely numeric, it represents a bug_id 
from the Bugzilla database to fetch. If it contains any non-numeric 
characters, it is considered to be a bug alias instead, and the data bug 
with that alias will be loaded.

=item C<new_since>

C<dateTime> If specified, the method will only return changes I<newer>
than this time.

=back

=item B<Returns>

A hash containing a single element, C<bugs>. This is an array of hashes,
containing the following keys:

=over

=item id

C<int> The numeric id of the bug.

=item alias

C<array> of C<string>s The unique aliases of this bug. An empty array will be
returned if this bug has no aliases.

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

=item Field names returned by the C<field_name> field changed to be
consistent with other methods. Since Bugzilla B<4.4>, they now match
names used by L<Bug.update|/"update"> for consistency.

=item REST API call added Bugzilla B<5.0>.

=item Added C<new_since> parameter if Bugzilla B<5.0>.

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

=item C<products> (array) - One or more product names to narrow the
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

=item The C<product> parameter has been renamed to C<products> in
Bugzilla B<5.0>.

=back

=back

=head2 search

B<UNSTABLE>

=over

=item B<Description>

Allows you to search for bugs based on particular criteria.

=item <REST>

To search for bugs:

GET /bug

The URL parameters and the returned data format are the same as below.

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

In addition to the fields listed below, you may also use criteria that
is similar to what is used in the Advanced Search screen of the Bugzilla
UI. This includes fields specified by C<Search by Change History> and
C<Custom Search>. The easiest way to determine what the field names are and what
format Bugzilla expects, is to first construct your query using the
Advanced Search UI, execute it and use the query parameters in they URL
as your key/value pairs for the WebService call. With REST, you can
just reuse the query parameter portion in the REST call itself.

=over

=item C<alias>

C<array> of C<string>s The unique aliases of this bug. An empty array will be
returned if this bug has no aliases.

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

=item C<tags>

C<string> Searches for a bug with the specified tag.  If you specify an
array, then any bugs that match I<any> of the tags will be returned.

Note that tags are personal to the currently logged in user.

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

=item C<quicksearch>

C<string> Search for bugs using quicksearch syntax.

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

=item REST API call added in Bugzilla B<5.0>.

=item Updated to allow for full search capability similar to the Bugzilla UI
in Bugzilla B<5.0>.

=item Updated to allow quicksearch capability in Bugzilla B<5.0>.

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

=item B<REST>

To create a new bug in Bugzilla:

POST /rest/bug

The params to include in the POST body as well as the returned data format,
are the same as below.

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

=item C<alias> (array) - A brief alias for the bug that can be used
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

=item C<resolution> (string) - If you are filing a closed bug, then
you will have to specify a resolution. You cannot currently specify
a resolution of C<DUPLICATE> for new bugs, though. That must be done
with L</update>.

=item C<target_milestone> (string) - A valid target milestone for this
product.

=item C<flags>

C<array> An array of hashes with flags to add to the bug. To create a flag,
at least the status and the type_id or name must be provided. An optional
requestee can be passed if the flag type is requestable to a specific user.

=over

=item C<name>

C<string> The name of the flag type.

=item C<type_id>

C<int> The internal flag type id.

=item C<status>

C<string> The flags new status (i.e. "?", "+", "-" or "X" to clear a flag).

=item C<requestee>

C<string> The login of the requestee if the flag type is requestable to a specific user.

=back

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

=item 129 (Flag Status Invalid)

The flag status is invalid.

=item 130 (Flag Modification Denied)

You tried to request, grant, or deny a flag but only a user with the required
permissions may make the change.

=item 131 (Flag not Requestable from Specific Person)

You can't ask a specific person for the flag.

=item 133 (Flag Type not Unique)

The flag type specified matches several flag types. You must specify
the type id value to update or add a flag.

=item 134 (Inactive Flag Type)

The flag type is inactive and cannot be used to create new flags.

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

=item The ability to file new bugs with a C<resolution> was added in
Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back


=head2 add_attachment

B<STABLE>

=over

=item B<Description>

This allows you to add an attachment to a bug in Bugzilla.

=item B<REST>

To create attachment on a current bug:

POST /rest/bug/<bug_id>/attachment

The params to include in the POST body, as well as the returned
data format are the same as below. The C<ids> param will be
overridden as it it pulled from the URL path.

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

=item C<flags>

C<array> An array of hashes with flags to add to the attachment. to create a flag,
at least the status and the type_id or name must be provided. An optional requestee
can be passed if the flag type is requestable to a specific user.

=over

=item C<name>

C<string> The name of the flag type.

=item C<type_id>

C<int> The internal flag type id.

=item C<status>

C<string> The flags new status (i.e. "?", "+", "-" or "X" to clear a flag).

=item C<requestee>

C<string> The login of the requestee if the flag type is requestable to a specific user.

=back

=back

=item B<Returns>

A single item C<ids>, which contains an array of the
attachment id(s) created.

=item B<Errors>

This method can throw all the same errors as L</get>, plus:

=over

=item 129 (Flag Status Invalid)

The flag status is invalid.

=item 130 (Flag Modification Denied)

You tried to request, grant, or deny a flag but only a user with the required
permissions may make the change.

=item 131 (Flag not Requestable from Specific Person)

You can't ask a specific person for the flag.

=item 133 (Flag Type not Unique)

The flag type specified matches several flag types. You must specify
the type id value to update or add a flag.

=item 134 (Inactive Flag Type)

The flag type is inactive and cannot be used to create new flags.

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

=item The return value has changed in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back


=head2 update_attachment

B<UNSTABLE>

=over

=item B<Description>

This allows you to update attachment metadata in Bugzilla.

=item B<REST>

To update attachment metadata on a current attachment:

PUT /rest/bug/attachment/<attach_id>

The params to include in the POST body, as well as the returned
data format are the same as below. The C<ids> param will be
overridden as it it pulled from the URL path.

=item B<Params>

=over

=item C<ids>

B<Required> C<array> An array of integers -- the ids of the attachments you
want to update.

=item C<file_name>

C<string> The "file name" that will be displayed
in the UI for this attachment.

=item C<summary>

C<string> A short string describing the
attachment.

=item C<comment>

C<string> An optional comment to add to the attachment's bug.

=item C<content_type>

C<string> The MIME type of the attachment, like
C<text/plain> or C<image/png>.

=item C<is_patch>

C<boolean> True if Bugzilla should treat this attachment as a patch.
If you specify this, you do not need to specify a C<content_type>.
The C<content_type> of the attachment will be forced to C<text/plain>.

=item C<is_private>

C<boolean> True if the attachment should be private (restricted
to the "insidergroup"), False if the attachment should be public.

=item C<is_obsolete>

C<boolean> True if the attachment is obsolete, False otherwise.

=item C<flags>

C<array> An array of hashes with changes to the flags. The following values
can be specified. At least the status and one of type_id, id, or name must
be specified. If a type_id or name matches a single currently set flag,
the flag will be updated unless new is specified.

=over

=item C<name>

C<string> The name of the flag that will be created or updated.

=item C<type_id>

C<int> The internal flag type id that will be created or updated. You will
need to specify the C<type_id> if more than one flag type of the same name exists.

=item C<status>

C<string> The flags new status (i.e. "?", "+", "-" or "X" to clear a flag).

=item C<requestee>

C<string> The login of the requestee if the flag type is requestable to a specific user.

=item C<id>

C<int> Use id to specify the flag to be updated. You will need to specify the C<id>
if more than one flag is set of the same name.

=item C<new>

C<boolean> Set to true if you specifically want a new flag to be created.

=back

=item B<Returns>

A C<hash> with a single field, "attachments". This points to an array of hashes
with the following fields:

=over

=item C<id>

C<int> The id of the attachment that was updated.

=item C<last_change_time>

C<dateTime> The exact time that this update was done at, for this attachment.
If no update was done (that is, no fields had their values changed and
no comment was added) then this will instead be the last time the attachment
was updated.

=item C<changes>

C<hash> The changes that were actually done on this bug. The keys are
the names of the fields that were changed, and the values are a hash
with two keys:

=over

=item C<added> (C<string>) The values that were added to this field.
possibly a comma-and-space-separated list if multiple values were added.

=item C<removed> (C<string>) The values that were removed from this
field.

=back

=back

Here's an example of what a return value might look like:

 {
   attachments => [
     {
       id    => 123,
       last_change_time => '2010-01-01T12:34:56',
       changes => {
         summary => {
           removed => 'Sample ptach',
           added   => 'Sample patch'
         },
         is_obsolete => {
           removed => '0',
           added   => '1',
         }
       },
     }
   ]
 }

=item B<Errors>

This method can throw all the same errors as L</get>, plus:

=over

=item 129 (Flag Status Invalid)

The flag status is invalid.

=item 130 (Flag Modification Denied)

You tried to request, grant, or deny a flag but only a user with the required
permissions may make the change.

=item 131 (Flag not Requestable from Specific Person)

You can't ask a specific person for the flag.

=item 132 (Flag not Unique)

The flag specified has been set multiple times. You must specify the id
value to update the flag.

=item 133 (Flag Type not Unique)

The flag type specified matches several flag types. You must specify
the type id value to update or add a flag.

=item 134 (Inactive Flag Type)

The flag type is inactive and cannot be used to create new flags.

=item 601 (Invalid MIME Type)

You specified a C<content_type> argument that was blank, not a valid
MIME type, or not a MIME type that Bugzilla accepts for attachments.

=item 603 (File Name Not Specified)

You did not specify a valid for the C<file_name> argument.

=item 604 (Summary Required)

You did not specify a value for the C<summary> argument.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back

=back

=head2 add_comment

B<STABLE>

=over

=item B<Description>

This allows you to add a comment to a bug in Bugzilla.

=item B<REST>

To create a comment on a current bug:

POST /rest/bug/<bug_id>/comment

The params to include in the POST body as well as the returned data format,
are the same as below.

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

If you specified an alias and there is no bug with that alias.

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

=item REST API call added in Bugzilla B<5.0>.

=back

=back


=head2 update

B<UNSTABLE>

=over

=item B<Description>

Allows you to update the fields of a bug. Automatically sends emails
out about the changes.

=item B<REST>

To update the fields of a current bug:

PUT /rest/bug/<bug_id>

The params to include in the PUT body as well as the returned data format,
are the same as below. The C<ids> param will be overridden as it is
pulled from the URL path.

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

C<hash> These specify the aliases of a bug that can be used instead of a bug
number when acessing this bug. To set these, you should pass a hash as the
value. The hash may contain the following fields:

=over

=item C<add> An array of C<string>s. Aliases to add to this field.

=item C<remove> An array of C<string>s. Aliases to remove from this field.
If the aliases are not already in the field, they will be ignored.

=item C<set> An array of C<string>s. An exact set of aliases to set this
field to, overriding the current value. If you specify C<set>, then C<add>
and  C<remove> will be ignored.

=back

You can only set this if you are modifying a single bug. If there is more
than one bug specified in C<ids>, passing in a value for C<alias> will cause
an error to be thrown.

For backwards compatibility, you can also specify a single string. This will
be treated as if you specified the set key above.

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

=item C<flags>

C<array> An array of hashes with changes to the flags. The following values
can be specified. At least the status and one of type_id, id, or name must
be specified. If a type_id or name matches a single currently set flag,
the flag will be updated unless new is specified.

=over

=item C<name>

C<string> The name of the flag that will be created or updated.

=item C<type_id>

C<int> The internal flag type id that will be created or updated. You will
need to specify the C<type_id> if more than one flag type of the same name exists.

=item C<status>

C<string> The flags new status (i.e. "?", "+", "-" or "X" to clear a flag).

=item C<requestee>

C<string> The login of the requestee if the flag type is requestable to a specific user.

=item C<id>

C<int> Use id to specify the flag to be updated. You will need to specify the C<id>
if more than one flag is set of the same name.

=item C<new>

C<boolean> Set to true if you specifically want a new flag to be created.

=back

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
the bug, even if they aren't in a group that can normally access
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

C<array> of C<string>s The aliases of the bug that was updated, if this bug
has any alias.

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
       alias => [ 'foo' ],
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

=item 129 (Flag Status Invalid)

The flag status is invalid.

=item 130 (Flag Modification Denied)

You tried to request, grant, or deny a flag but only a user with the required
permissions may make the change.

=item 131 (Flag not Requestable from Specific Person)

You can't ask a specific person for the flag.

=item 132 (Flag not Unique)

The flag specified has been set multiple times. You must specify the id
value to update the flag.

=item 133 (Flag Type not Unique)

The flag type specified matches several flag types. You must specify
the type id value to update or add a flag.

=item 134 (Inactive Flag Type)

The flag type is inactive and cannot be used to create new flags.

=back

=item B<History>

=over

=item Added in Bugzilla B<4.0>.

=item REST API call added Bugzilla B<5.0>.

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


=head2 update_tags

B<UNSTABLE>

=over

=item B<Description>

Adds or removes tags on bugs.

=item B<Params>

=over

=item C<ids>

B<Required> C<array> An array of ints and/or strings--the ids
or aliases of bugs that you want to add or remove tags to. All the tags
will be added or removed to all these bugs.

=item C<tags>

B<Required> C<hash> A hash representing tags to be added and/or removed.
The hash has the following fields:

=over

=item C<add> An array of C<string>s representing tag names
to be added to the bugs.

It is safe to specify tags that are already associated with the 
bugs--they will just be silently ignored.

=item C<remove> An array of C<string>s representing tag names
to be removed from the bugs.

It is safe to specify tags that are not associated with any
bugs--they will just be silently ignored.

=back

=back

=item B<Returns>

C<changes>, a hash containing bug IDs as keys and one single value
name "tags" which is also a hash, with C<added> and C<removed> as keys.
See L</update_see_also> for an example of how it looks like.

=item B<Errors>

This method can throw the same errors as L</get>.

=item B<History>

=over

=item Added in Bugzilla B<4.4>.

=back

=back

=head2 search_comment_tags

B<UNSTABLE>

=over

=item B<Description>

Searches for tags which contain the provided substring.

=item B<REST>

To search for comment tags:

GET /rest/bug/comment/tags/<query>

=item B<Params>

=over

=item C<query>

B<Required> C<string> Only tags containg this substring will be returned.

=item C<limit>

C<int> If provided will return no more than C<limit> tags.  Defaults to C<10>.

=back

=item B<Returns>

An C<array of strings> of matching tags.

=item B<Errors>

This method can throw all of the errors that L</get> throws, plus:

=over

=item 125 (Comment Tagging Disabled)

Comment tagging support is not available or enabled.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back

=head2 update_comment_tags

B<UNSTABLE>

=over

=item B<Description>

Adds or removes tags from a comment.

=item B<REST>

To update the tags comments attached to a comment:

PUT /rest/bug/comment/tags

The params to include in the PUT body as well as the returned data format,
are the same as below.

=item B<Params>

=over

=item C<comment_id>

B<Required> C<int> The ID of the comment to update.

=item C<add>

C<array of strings> The tags to attach to the comment.

=item C<remove>

C<array of strings> The tags to detach from the comment.

=back

=item B<Returns>

An C<array of strings> containing the comment's updated tags.

=item B<Errors>

This method can throw all of the errors that L</get> throws, plus:

=over

=item 125 (Comment Tagging Disabled)

Comment tagging support is not available or enabled.

=item 126 (Invalid Comment Tag)

The comment tag provided was not valid (eg. contains invalid characters).

=item 127 (Comment Tag Too Short)

The comment tag provided is shorter than the minimum length.

=item 128 (Comment Tag Too Long)

The comment tag provided is longer than the maximum length.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back

=head2 render_comment

B<UNSTABLE>

=over

=item B<Description>

Returns the HTML rendering of the provided comment text.

=item B<Params>

=over

=item C<text>

B<Required> C<strings> Text comment text to render.

=item C<id>

C<int> The ID of the bug to render the comment against.

=back

=item B<Returns>

C<html> containing the HTML rendering.

=item B<Errors>

This method can throw all of the errors that L</get> throws.

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back
