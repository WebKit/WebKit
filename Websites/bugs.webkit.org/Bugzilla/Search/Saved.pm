# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Search::Saved;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::CGI;
use Bugzilla::Constants;
use Bugzilla::Group;
use Bugzilla::Error;
use Bugzilla::Search qw(IsValidQueryType);
use Bugzilla::User;
use Bugzilla::Util;

use Scalar::Util qw(blessed);

#############
# Constants #
#############

use constant DB_TABLE => 'namedqueries';
# Do not track buglists saved by users.
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;
use constant AUDIT_REMOVES => 0;

use constant DB_COLUMNS => qw(
    id
    userid
    name
    query
);

use constant VALIDATORS => {
    name       => \&_check_name,
    query      => \&_check_query,
    link_in_footer => \&_check_link_in_footer,
};

use constant UPDATE_COLUMNS => qw(name query);

###############
# Constructor #
###############

sub new {
    my $class = shift;
    my $param = shift;
    my $dbh = Bugzilla->dbh;

    my $user;
    if (ref $param) {
        $user = $param->{user} || Bugzilla->user;
        my $name = $param->{name};
        if (!defined $name) {
            ThrowCodeError('bad_arg',
                {argument => 'name',
                 function => "${class}::new"});
        }
        my $condition = 'userid = ? AND name = ?';
        my $user_id = blessed $user ? $user->id : $user;
        detaint_natural($user_id)
          || ThrowCodeError('param_must_be_numeric',
                            {function => $class . '::_init', param => 'user'});
        my @values = ($user_id, $name);
        $param = { condition => $condition, values => \@values };
    }

    unshift @_, $param;
    my $self = $class->SUPER::new(@_);
    if ($self) {
        $self->{user} = $user if blessed $user;

        # Some DBs (read: Oracle) incorrectly mark the query string as UTF-8
        # when it's coming out of the database, even though it has no UTF-8
        # characters in it, which prevents Bugzilla::CGI from later reading
        # it correctly.
        utf8::downgrade($self->{query}) if utf8::is_utf8($self->{query});
    }
    return $self;
}

sub check {
    my $class = shift;
    my $search = $class->SUPER::check(@_);
    my $user = Bugzilla->user;
    return $search if $search->user->id == $user->id;

    if (!$search->shared_with_group
        or !$user->in_group($search->shared_with_group)) 
    {
        ThrowUserError('missing_query', { name => $search->name,
                                          sharer_id => $search->user->id });
    }

    return $search;
}

##############
# Validators #
##############

sub _check_link_in_footer { return $_[1] ? 1 : 0; }

sub _check_name {
    my ($invocant, $name) = @_;
    $name = trim($name);
    $name || ThrowUserError("query_name_missing");
    $name !~ /[<>&]/ || ThrowUserError("illegal_query_name");
    if (length($name) > MAX_LEN_QUERY_NAME) {
        ThrowUserError("query_name_too_long");
    }
    return $name;
}

sub _check_query {
    my ($invocant, $query) = @_;
    $query || ThrowUserError("buglist_parameters_required");
    my $cgi = new Bugzilla::CGI($query);
    $cgi->clean_search_url;
    # Don't store the query name as a parameter.
    $cgi->delete('known_name');
    return $cgi->query_string;
}

#########################
# Database Manipulation #
#########################

sub create {
    my $class = shift;
    Bugzilla->login(LOGIN_REQUIRED);
    my $dbh = Bugzilla->dbh;
    $class->check_required_create_fields(@_);
    $dbh->bz_start_transaction();
    my $params = $class->run_create_validators(@_);

    # Right now you can only create a Saved Search for the current user.
    $params->{userid} = Bugzilla->user->id;

    my $lif = delete $params->{link_in_footer};
    my $obj = $class->insert_create_data($params);
    if ($lif) {
        $dbh->do('INSERT INTO namedqueries_link_in_footer 
                  (user_id, namedquery_id) VALUES (?,?)',
                 undef, $params->{userid}, $obj->id);
    }
    $dbh->bz_commit_transaction();

    return $obj;
}

sub rename_field_value {
    my ($class, $field, $old_value, $new_value) = @_;

    my $old = url_quote($old_value);
    my $new = url_quote($new_value);
    my $old_sql = $old;
    $old_sql =~ s/([_\%])/\\$1/g;

    my $table = $class->DB_TABLE;
    my $id_field = $class->ID_FIELD;

    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();

    my %queries = @{ $dbh->selectcol_arrayref(
        "SELECT $id_field, query FROM $table WHERE query LIKE ?",
        {Columns=>[1,2]}, "\%$old_sql\%") };
    foreach my $id (keys %queries) {
        my $query = $queries{$id};
        $query =~ s/\b$field=\Q$old\E\b/$field=$new/gi;
        # Fix boolean charts.
        while ($query =~ /\bfield(\d+-\d+-\d+)=\Q$field\E\b/gi) {
            my $chart_id = $1;
            # Note that this won't handle lists or substrings inside of
            # boolean charts. Users will have to fix those themselves.
            $query =~ s/\bvalue\Q$chart_id\E=\Q$old\E\b/value$chart_id=$new/i;
        }
        $dbh->do("UPDATE $table SET query = ? WHERE $id_field = ?",
                 undef, $query, $id);
        Bugzilla->memcached->clear({ table => $table, id => $id });
    }

    $dbh->bz_commit_transaction();
}

sub preload {
    my ($searches) = @_;
    my $dbh = Bugzilla->dbh;

    return unless scalar @$searches;

    my @query_ids = map { $_->id } @$searches;
    my $queries_in_footer = $dbh->selectcol_arrayref(
        'SELECT namedquery_id
           FROM namedqueries_link_in_footer
          WHERE ' . $dbh->sql_in('namedquery_id', \@query_ids) . ' AND user_id = ?',
          undef, Bugzilla->user->id);

    my %links_in_footer = map { $_ => 1 } @$queries_in_footer;
    foreach my $query (@$searches) {
        $query->{link_in_footer} = ($links_in_footer{$query->id}) ? 1 : 0;
    }
}
#####################
# Complex Accessors #
#####################

sub edit_link {
    my ($self) = @_;
    return $self->{edit_link} if defined $self->{edit_link};
    my $cgi = new Bugzilla::CGI($self->url);
    if (!$cgi->param('query_type') 
        || !IsValidQueryType($cgi->param('query_type')))
    {
        $cgi->param('query_type', 'advanced');
    }
    $self->{edit_link} = $cgi->canonicalise_query;
    return $self->{edit_link};
}

sub used_in_whine {
    my ($self) = @_;
    return $self->{used_in_whine} if exists $self->{used_in_whine};
    ($self->{used_in_whine}) = Bugzilla->dbh->selectrow_array(
        'SELECT 1 FROM whine_events INNER JOIN whine_queries
                       ON whine_events.id = whine_queries.eventid
          WHERE whine_events.owner_userid = ? AND query_name = ?', undef, 
          $self->{userid}, $self->name) || 0;
    return $self->{used_in_whine};
}

sub link_in_footer {
    my ($self, $user) = @_;
    # We only cache link_in_footer for the current Bugzilla->user.
    return $self->{link_in_footer} if exists $self->{link_in_footer} && !$user;
    my $user_id = $user ? $user->id : Bugzilla->user->id;
    my $link_in_footer = Bugzilla->dbh->selectrow_array(
        'SELECT 1 FROM namedqueries_link_in_footer
          WHERE namedquery_id = ? AND user_id = ?', 
        undef, $self->id, $user_id) || 0;
    $self->{link_in_footer} = $link_in_footer if !$user;
    return $link_in_footer;
}

sub shared_with_group {
    my ($self) = @_;
    return $self->{shared_with_group} if exists $self->{shared_with_group};
    # Bugzilla only currently supports sharing with one group, even
    # though the database backend allows for an infinite number.
    my ($group_id) = Bugzilla->dbh->selectrow_array(
        'SELECT group_id FROM namedquery_group_map WHERE namedquery_id = ?',
        undef, $self->id);
    $self->{shared_with_group} = $group_id ? new Bugzilla::Group($group_id) 
                                 : undef;
    return $self->{shared_with_group};
}

sub shared_with_users {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!exists $self->{shared_with_users}) {
        $self->{shared_with_users} =
          $dbh->selectrow_array('SELECT COUNT(*)
                                   FROM namedqueries_link_in_footer
                             INNER JOIN namedqueries
                                     ON namedquery_id = id
                                  WHERE namedquery_id = ?
                                    AND user_id != userid',
                                  undef, $self->id);
    }
    return $self->{shared_with_users};
}

####################
# Simple Accessors #
####################

sub url  { return $_[0]->{'query'}; }

sub user {
    my ($self) = @_;
    return $self->{user} ||=
        Bugzilla::User->new({ id => $self->{userid}, cache => 1 });
}

############
# Mutators #
############

sub set_name       { $_[0]->set('name',       $_[1]); }
sub set_url        { $_[0]->set('query',      $_[1]); }

1;

__END__

=head1 NAME

Bugzilla::Search::Saved - A saved search

=head1 SYNOPSIS

 use Bugzilla::Search::Saved;

 my $query = new Bugzilla::Search::Saved($query_id);

 my $edit_link  = $query->edit_link;
 my $search_url = $query->url;
 my $owner      = $query->user;
 my $num_subscribers = $query->shared_with_users;

=head1 DESCRIPTION

This module exists to represent a L<Bugzilla::Search> that has been
saved to the database.

This is an implementation of L<Bugzilla::Object>, and so has all the
same methods available as L<Bugzilla::Object>, in addition to what is
documented below.

=head1 METHODS

=head2 Constructors and Database Manipulation

=over

=item C<new>

Takes either an id, or the named parameters C<user> and C<name>.
C<user> can be either a L<Bugzilla::User> object or a numeric user id.

See also: L<Bugzilla::Object/new>.

=item C<preload>

Sets C<link_in_footer> for all given saved searches at once, for the
currently logged in user. This is much faster than calling this method
for each saved search individually.

=back


=head2 Accessors

These return data about the object, without modifying the object.

=over

=item C<edit_link>

A url with which you can edit the search.

=item C<url>

The CGI parameters for the search, as a string.

=item C<link_in_footer>

Whether or not this search should be displayed in the footer for the
I<current user> (not the owner of the search, but the person actually
using Bugzilla right now).

=item C<type>

The numeric id of the type of search this is (from L<Bugzilla::Constants>).

=item C<shared_with_group>

The L<Bugzilla::Group> that this search is shared with. C<undef> if
this search isn't shared.

=item C<shared_with_users>

Returns how many users (besides the author of the saved search) are
using the saved search, i.e. have it displayed in their footer.

=back

=head1 B<Methods in need of POD>

=over

=item create

=item set_name

=item set_url

=item rename_field_value

=item user

=item used_in_whine

=back
