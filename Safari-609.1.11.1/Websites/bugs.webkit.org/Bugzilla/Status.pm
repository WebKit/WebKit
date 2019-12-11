# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Status;

use 5.10.1;
use strict;
use warnings;

# This subclasses Bugzilla::Field::Choice instead of implementing 
# ChoiceInterface, because a bug status literally is a special type
# of Field::Choice, not just an object that happens to have the same
# methods.
use parent qw(Bugzilla::Field::Choice Exporter);
@Bugzilla::Status::EXPORT = qw(
    BUG_STATE_OPEN
    SPECIAL_STATUS_WORKFLOW_ACTIONS

    is_open_state 
    closed_bug_statuses
);

use Bugzilla::Error;

################################
#####   Initialization     #####
################################

use constant SPECIAL_STATUS_WORKFLOW_ACTIONS => qw(
    none
    duplicate
    change_resolution
    clearresolution
);

use constant DB_TABLE => 'bug_status';

# This has all the standard Bugzilla::Field::Choice columns plus "is_open"
sub DB_COLUMNS {
    return ($_[0]->SUPER::DB_COLUMNS, 'is_open');
}

sub VALIDATORS {
    my $invocant = shift;
    my $validators = $invocant->SUPER::VALIDATORS;
    $validators->{is_open} = \&Bugzilla::Object::check_boolean;
    $validators->{value} = \&_check_value;
    return $validators;
}

#########################
# Database Manipulation #
#########################

sub create {
    my $class = shift;
    my $self = $class->SUPER::create(@_);
    delete Bugzilla->request_cache->{status_bug_state_open};
    add_missing_bug_status_transitions();
    return $self;
}

sub remove_from_db {
    my $self = shift;
    $self->SUPER::remove_from_db();
    delete Bugzilla->request_cache->{status_bug_state_open};
}

###############################
#####     Accessors        ####
###############################

sub is_active { return $_[0]->{'isactive'}; }
sub is_open   { return $_[0]->{'is_open'};  }

sub is_static {
    my $self = shift;
    if ($self->name eq 'UNCONFIRMED'
        || $self->name eq Bugzilla->params->{'duplicate_or_move_bug_status'}) 
    {
        return 1;
    }
    return 0;
}

##############
# Validators #
##############

sub _check_value {
    my $invocant = shift;
    my $value = $invocant->SUPER::_check_value(@_);

    if (grep { lc($value) eq lc($_) } SPECIAL_STATUS_WORKFLOW_ACTIONS) {
        ThrowUserError('fieldvalue_reserved_word',
                       { field => $invocant->field, value => $value });
    }
    return $value;
}


###############################
#####       Methods        ####
###############################

sub BUG_STATE_OPEN {
    my $dbh = Bugzilla->dbh;
    my $request_cache = Bugzilla->request_cache;
    my $cache_key = 'status_bug_state_open';
    return @{ $request_cache->{$cache_key} }
        if exists $request_cache->{$cache_key};

    my $rows = Bugzilla->memcached->get_config({ key => $cache_key });
    if (!$rows) {
        $rows = $dbh->selectcol_arrayref(
            'SELECT value FROM bug_status WHERE is_open = 1'
        );
        Bugzilla->memcached->set_config({ key => $cache_key, data => $rows });
    }

    $request_cache->{$cache_key} = $rows;
    return @$rows;
}

# Tells you whether or not the argument is a valid "open" state.
sub is_open_state {
    my ($state) = @_;
    return (grep($_ eq $state, BUG_STATE_OPEN) ? 1 : 0);
}

sub closed_bug_statuses {
    my @bug_statuses = Bugzilla::Status->get_all;
    @bug_statuses = grep { !$_->is_open } @bug_statuses;
    return @bug_statuses;
}

sub can_change_to {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!ref($self) || !defined $self->{'can_change_to'}) {
        my ($cond, @args, $self_exists);
        if (ref($self)) {
            $cond = '= ?';
            push(@args, $self->id);
            $self_exists = 1;
        }
        else {
            $cond = 'IS NULL';
            # Let's do it so that the code below works in all cases.
            $self = {};
        }

        my $new_status_ids = $dbh->selectcol_arrayref("SELECT new_status
                                                         FROM status_workflow
                                                   INNER JOIN bug_status
                                                           ON id = new_status
                                                        WHERE isactive = 1
                                                          AND old_status $cond
                                                     ORDER BY sortkey",
                                                        undef, @args);

        # Allow the bug status to remain unchanged.
        push(@$new_status_ids, $self->id) if $self_exists;
        $self->{'can_change_to'} = Bugzilla::Status->new_from_list($new_status_ids);
    }

    return $self->{'can_change_to'};
}

sub comment_required_on_change_from {
    my ($self, $old_status) = @_;
    my ($cond, $values) = $self->_status_condition($old_status);
    
    my ($require_comment) = Bugzilla->dbh->selectrow_array(
        "SELECT require_comment FROM status_workflow
          WHERE $cond", undef, @$values);
    return $require_comment;
}

# Used as a helper for various functions that have to deal with old_status
# sometimes being NULL and sometimes having a value.
sub _status_condition {
    my ($self, $old_status) = @_;
    my @values;
    my $cond = 'old_status IS NULL';
    # We may pass a fake status object to represent the initial unset state.
    if ($old_status && $old_status->id)  {
        $cond = 'old_status = ?';
        push(@values, $old_status->id);
    }
    $cond .= " AND new_status = ?";
    push(@values, $self->id);
    return ($cond, \@values);
}

sub add_missing_bug_status_transitions {
    my $bug_status = shift || Bugzilla->params->{'duplicate_or_move_bug_status'};
    my $dbh = Bugzilla->dbh;
    my $new_status = new Bugzilla::Status({name => $bug_status});
    # Silently discard invalid bug statuses.
    $new_status || return;

    my $missing_statuses = $dbh->selectcol_arrayref('SELECT id
                                                       FROM bug_status
                                                  LEFT JOIN status_workflow
                                                         ON old_status = id
                                                        AND new_status = ?
                                                      WHERE old_status IS NULL',
                                                      undef, $new_status->id);

    my $sth = $dbh->prepare('INSERT INTO status_workflow
                             (old_status, new_status) VALUES (?, ?)');

    foreach my $old_status_id (@$missing_statuses) {
        next if ($old_status_id == $new_status->id);
        $sth->execute($old_status_id, $new_status->id);
    }
}

1;

__END__

=head1 NAME

Bugzilla::Status - Bug status class.

=head1 SYNOPSIS

    use Bugzilla::Status;

    my $bug_status = new Bugzilla::Status({ name => 'IN_PROGRESS' });
    my $bug_status = new Bugzilla::Status(4);

    my @closed_bug_statuses = closed_bug_statuses();

    Bugzilla::Status::add_missing_bug_status_transitions($bug_status);

=head1 DESCRIPTION

Status.pm represents a bug status object. It is an implementation
of L<Bugzilla::Object>, and thus provides all methods that
L<Bugzilla::Object> provides.

The methods that are specific to C<Bugzilla::Status> are listed
below.

=head1 METHODS

=over

=item C<closed_bug_statuses>

 Description: Returns a list of C<Bugzilla::Status> objects which can have
              a resolution associated with them ("closed" bug statuses).

 Params:      none.

 Returns:     A list of Bugzilla::Status objects.

=item C<can_change_to>

 Description: Returns the list of active statuses a bug can be changed to
              given the current bug status. If this method is called as a
              class method, then it returns all bug statuses available on
              bug creation.

 Params:      none.

 Returns:     A list of Bugzilla::Status objects.

=item C<comment_required_on_change_from>

=over

=item B<Description>

Checks if a comment is required to change to this status from another
status, according to the current settings in the workflow.

Note that this doesn't implement the checks enforced by the various
C<commenton> parameters--those are checked by internal checks in
L<Bugzilla::Bug>.

=item B<Params>

C<$old_status> - The status you're changing from.

=item B<Returns>

C<1> if a comment is required on this change, C<0> if not.

=back

=item C<add_missing_bug_status_transitions>

 Description: Insert all missing transitions to a given bug status.

 Params:      $bug_status - The value (name) of a bug status.

 Returns:     nothing.

=back

=cut

=head1 B<Methods in need of POD>

=over

=item create

=item BUG_STATE_OPEN

=item is_static

=item is_open_state

=item is_active

=item remove_from_db

=item DB_COLUMNS

=item is_open

=item VALIDATORS

=back
