# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::BugUserLastVisit;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService);

use Bugzilla::Bug;
use Bugzilla::Error;
use Bugzilla::WebService::Util qw( validate filter );
use Bugzilla::Constants;

use constant PUBLIC_METHODS => qw(
    get
    update
);

sub update {
    my ($self, $params) = validate(@_, 'ids');
    my $user = Bugzilla->user;
    my $dbh  = Bugzilla->dbh;

    $user->login(LOGIN_REQUIRED);

    my $ids = $params->{ids} // [];
    ThrowCodeError('param_required', { param => 'ids' }) unless @$ids;

    # Cache permissions for bugs. This highly reduces the number of calls to the
    # DB.  visible_bugs() is only able to handle bug IDs, so we have to skip
    # aliases.
    $user->visible_bugs([grep /^[0-9]+$/, @$ids]);

    $dbh->bz_start_transaction();
    my @results;
    my $last_visit_ts = $dbh->selectrow_array('SELECT NOW()');
    foreach my $bug_id (@$ids) {
        my $bug = Bugzilla::Bug->check({ id => $bug_id, cache => 1 });

        ThrowUserError('user_not_involved', { bug_id => $bug->id })
            unless $user->is_involved_in_bug($bug);

        $bug->update_user_last_visit($user, $last_visit_ts);

        push(
            @results,
            $self->_bug_user_last_visit_to_hash(
                $bug->id, $last_visit_ts, $params
            ));
    }
    $dbh->bz_commit_transaction();

    return \@results;
}

sub get {
    my ($self, $params) = validate(@_, 'ids');
    my $user = Bugzilla->user;
    my $ids  = $params->{ids};

    $user->login(LOGIN_REQUIRED);

    my @last_visits;
    if ($ids) {
        # Cache permissions for bugs. This highly reduces the number of calls to
        # the DB.  visible_bugs() is only able to handle bug IDs, so we have to
        # skip aliases.
        $user->visible_bugs([grep /^[0-9]+$/, @$ids]);

        my %last_visit  = map { $_->bug_id => $_->last_visit_ts } @{ $user->last_visited($ids) };
        @last_visits = map { $self->_bug_user_last_visit_to_hash($_->id, $last_visit{$_}, $params) } @$ids;
    }
    else {
        @last_visits = map {
            $self->_bug_user_last_visit_to_hash($_->bug_id, $_->last_visit_ts, $params)
        } @{ $user->last_visited };
    }

    return \@last_visits;
}

sub _bug_user_last_visit_to_hash {
    my ($self, $bug_id, $last_visit_ts, $params) = @_;

    my %result = (id            => $self->type('int',      $bug_id),
                  last_visit_ts => $self->type('dateTime', $last_visit_ts));

    return filter($params, \%result);
}

1;

__END__
=head1 NAME

Bugzilla::WebService::BugUserLastVisit - Find and Store the last time a user
visited a bug.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

Although the data input and output is the same for JSONRPC, XMLRPC and REST,
the directions for how to access the data via REST is noted in each method
where applicable.

=head2 update

B<EXPERIMENTAL>

=over

=item B<Description>

Update the last visit time for the specified bug and current user.

=item B<REST>

To add a single bug id:

    POST /rest/bug_user_last_visit/<bug-id>

Tp add one or more bug ids at once:

    POST /rest/bug_user_last_visit

The returned data format is the same as below.

=item B<Params>

=over

=item C<ids> (array) - One or more bug ids to add.

=back

=item B<Returns>

=over

=item C<array> - An array of hashes containing the following:

=over

=item C<id> - (int) The bug id.

=item C<last_visit_ts> - (string) The timestamp the user last visited the bug.

=back

=back

=back

=head2 get

B<EXPERIMENTAL>

=over

=item B<Description>

Get the last visited timestamp for one or more specified bug ids.

=item B<REST>

To return the last visited timestamp for a single bug id:

    GET /rest/bug_user_last_visit/<bug-id>

=item B<Params>

=over

=item C<ids> (integer) - One or more optional bug ids to get.

=back

=item B<Returns>

=over

=item C<array> - An array of hashes containing the following:

=over

=item C<id> - (int) The bug id.

=item C<last_visit_ts> - (string) The timestamp the user last visited the bug.

=back

=back

=back
