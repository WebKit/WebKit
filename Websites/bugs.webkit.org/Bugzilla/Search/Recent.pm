# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Search::Recent;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;

#############
# Constants #
#############

use constant DB_TABLE => 'profile_search';
use constant LIST_ORDER => 'id DESC';
# Do not track buglists viewed by users.
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;
use constant AUDIT_REMOVES => 0;

use constant DB_COLUMNS => qw(
    id
    user_id
    bug_list
    list_order
);

use constant VALIDATORS => {
    user_id    => \&_check_user_id,
    bug_list   => \&_check_bug_list,
    list_order => \&_check_list_order,
};

use constant UPDATE_COLUMNS => qw(bug_list list_order);

# There's no gain to caching these objects
use constant USE_MEMCACHED => 0;

###################
# DB Manipulation #
###################

sub create {
    my $class = shift;
    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    my $search = $class->SUPER::create(@_);
    my $user_id = $search->user_id;

    # Enforce there only being SAVE_NUM_SEARCHES per user.
    my @ids = @{ $dbh->selectcol_arrayref(
        "SELECT id FROM profile_search WHERE user_id = ? ORDER BY id",
        undef, $user_id) };
    if (scalar(@ids) > SAVE_NUM_SEARCHES) {
        splice(@ids, - SAVE_NUM_SEARCHES);
        $dbh->do(
            "DELETE FROM profile_search WHERE id IN (" . join(',', @ids) . ")");
    }
    $dbh->bz_commit_transaction();
    return $search;
}

sub create_placeholder {
    my $class = shift;
    return $class->create({ user_id  => Bugzilla->user->id,
                            bug_list => '' });
}

###############
# Constructor #
###############

sub check {
    my $class = shift;
    my $search = $class->SUPER::check(@_);
    my $user = Bugzilla->user;
    if ($search->user_id != $user->id) {
        ThrowUserError('object_does_not_exist', { id => $search->id });
    }
    return $search;
}

sub check_quietly {
    my $class = shift;
    my $error_mode = Bugzilla->error_mode;
    Bugzilla->error_mode(ERROR_MODE_DIE);
    my $search = eval { $class->check(@_) };
    Bugzilla->error_mode($error_mode);
    return $search;
}

sub new_from_cookie {
    my ($invocant, $bug_ids) = @_;
    my $class = ref($invocant) || $invocant;

    my $search = { id       => 'cookie',
                   user_id  => Bugzilla->user->id,
                   bug_list => join(',', @$bug_ids) };

    bless $search, $class;
    return $search;
}

####################
# Simple Accessors #
####################

sub bug_list   { return [split(',', $_[0]->{'bug_list'})]; }
sub list_order { return $_[0]->{'list_order'}; }
sub user_id    { return $_[0]->{'user_id'}; }

############
# Mutators #
############

sub set_bug_list   { $_[0]->set('bug_list',   $_[1]); }
sub set_list_order { $_[0]->set('list_order', $_[1]); }

##############
# Validators #
##############

sub _check_user_id {
    my ($invocant, $id) = @_;
    require Bugzilla::User;
    return Bugzilla::User->check({ id => $id })->id;
}

sub _check_bug_list {
    my ($invocant, $list) = @_;

    my @bug_ids = ref($list) ? @$list : split(',', $list || '');
    detaint_natural($_) foreach @bug_ids;
    return join(',', @bug_ids);
}

sub _check_list_order { defined $_[1] ? trim($_[1]) : '' }

1;

__END__

=head1 NAME

Bugzilla::Search::Recent - A search recently run by a logged-in user.

=head1 SYNOPSIS

 use Bugzilla::Search::Recent;


=head1 DESCRIPTION

This is an implementation of L<Bugzilla::Object>, and so has all the
same methods available as L<Bugzilla::Object>, in addition to what is
documented below.

=head1 B<Methods in need of POD>

=over

=item create

=item list_order

=item check_quietly

=item new_from_cookie

=item create_placeholder

=item bug_list

=item set_bug_list

=item user_id

=item set_list_order

=back
