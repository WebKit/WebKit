# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Search::ClauseGroup;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Search::Clause);

use Bugzilla::Error;
use Bugzilla::Search::Condition qw(condition);
use Bugzilla::Util qw(trick_taint);
use List::MoreUtils qw(uniq);

use constant UNSUPPORTED_FIELDS => qw(
    attach_data.thedata
    classification
    commenter
    component
    longdescs.count
    product
    owner_idle_time
);

sub new {
    my ($class) = @_;
    my $self = bless({ joiner => 'AND' }, $class);
    # Add a join back to the bugs table which will be used to group conditions
    # for this clause
    my $condition = Bugzilla::Search::Condition->new({});
    $condition->translated({
        joins => [{
            table => 'bugs',
            as    => 'bugs_g0',
            from  => 'bug_id',
            to    => 'bug_id',
            extra => [],
        }],
        term => '1 = 1',
    });
    $self->SUPER::add($condition);
    $self->{group_condition} = $condition;
    return $self;
}

sub add {
    my ($self, @args) = @_;
    my $field = scalar(@args) == 3 ? $args[0] : $args[0]->{field};

    # We don't support nesting of conditions under this clause
    if (scalar(@args) == 1 && !$args[0]->isa('Bugzilla::Search::Condition')) {
        ThrowUserError('search_grouped_invalid_nesting');
    }

    # Ensure all conditions use the same field
    if (!$self->{_field}) {
        $self->{_field} = $field;
    } elsif ($field ne $self->{_field}) {
        ThrowUserError('search_grouped_field_mismatch');
    }

    # Unsupported fields
    if (grep { $_ eq $field } UNSUPPORTED_FIELDS ) {
        # XXX - Hack till bug 916882 is fixed.
        my $operator = scalar(@args) == 3 ? $args[1] : $args[0]->{operator};
        ThrowUserError('search_grouped_field_invalid', { field => $field })
          unless (($field eq 'product' || $field eq 'component') && $operator =~ /^changed/);
    }

    $self->SUPER::add(@args);
}

sub update_search_args {
    my ($self, $search_args) = @_;

    # No need to change things if there's only one child condition
    return unless scalar(@{ $self->children }) > 1;

    # we want all the terms to use the same join table
    if (!exists $self->{_first_chart_id}) {
        $self->{_first_chart_id} = $search_args->{chart_id};
    } else {
        $search_args->{chart_id} = $self->{_first_chart_id};
    }

    my $suffix = '_g' . $self->{_first_chart_id};
    $self->{group_condition}->{translated}->{joins}->[0]->{as} = "bugs$suffix";

    $search_args->{full_field} =~ s/^bugs\./bugs$suffix\./;

    $search_args->{table_suffix} = $suffix;
    $search_args->{bugs_table} = "bugs$suffix";
}

1;

=head1 B<Methods in need of POD>

=over

=item add

=item update_search_args

=back
