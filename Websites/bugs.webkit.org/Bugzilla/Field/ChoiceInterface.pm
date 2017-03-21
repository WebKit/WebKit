# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Field::ChoiceInterface;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field;

use Scalar::Util qw(blessed);

# Helps implement the "field" accessor without subclasses having to
# write code.
sub FIELD_NAME { return $_[0]->DB_TABLE; }

####################
# Subclass Helpers #
####################

sub _check_if_controller {
    my $self = shift;
    my $vis_fields = $self->controls_visibility_of_fields;
    my $values = $self->controlled_values_array;
    if (@$vis_fields || @$values) {
        ThrowUserError('fieldvalue_is_controller',
            { value => $self, fields => [map($_->name, @$vis_fields)],
              vals => $self->controlled_values });
    }
}


#############
# Accessors #
#############

sub is_active { return $_[0]->{'isactive'}; }
sub sortkey   { return $_[0]->{'sortkey'};  }

sub bug_count {
    my $self = shift;
    return $self->{bug_count} if defined $self->{bug_count};
    my $dbh = Bugzilla->dbh;
    my $fname = $self->field->name;
    my $count;
    if ($self->field->type == FIELD_TYPE_MULTI_SELECT) {
        $count = $dbh->selectrow_array("SELECT COUNT(*) FROM bug_$fname
                                         WHERE value = ?", undef, $self->name);
    }
    else {
        $count = $dbh->selectrow_array("SELECT COUNT(*) FROM bugs 
                                         WHERE $fname = ?",
                                       undef, $self->name);
    }
    $self->{bug_count} = $count;
    return $count;
}

sub field {
    my $invocant = shift;
    my $class = ref $invocant || $invocant;
    my $cache = Bugzilla->request_cache;
    # This is just to make life easier for subclasses. Our auto-generated
    # subclasses from Bugzilla::Field::Choice->type() already have this set.
    $cache->{"field_$class"} ||=  
        new Bugzilla::Field({ name => $class->FIELD_NAME });
    return $cache->{"field_$class"};
}

sub is_default {
    my $self = shift;
    my $name = $self->DEFAULT_MAP->{$self->field->name};
    # If it doesn't exist in DEFAULT_MAP, then there is no parameter
    # related to this field.
    return 0 unless $name;
    return ($self->name eq Bugzilla->params->{$name}) ? 1 : 0;
}

sub is_static {
    my $self = shift;
    # If we need to special-case Resolution for *anything* else, it should
    # get its own subclass.
    if ($self->field->name eq 'resolution') {
        return grep($_ eq $self->name, ('', 'FIXED', 'DUPLICATE'))
               ? 1 : 0;
    }
    elsif ($self->field->custom) {
        return $self->name eq '---' ? 1 : 0;
    }
    return 0;
}

sub controls_visibility_of_fields {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!$self->{controls_visibility_of_fields}) {
        my $ids = $dbh->selectcol_arrayref(
            "SELECT id FROM fielddefs
               INNER JOIN field_visibility
                 ON fielddefs.id = field_visibility.field_id
             WHERE value_id = ? AND visibility_field_id = ?", undef,
            $self->id, $self->field->id);

        $self->{controls_visibility_of_fields} =
            Bugzilla::Field->new_from_list($ids);
   }

   return $self->{controls_visibility_of_fields};
}

sub visibility_value {
    my $self = shift;
    if ($self->{visibility_value_id}) {
        require Bugzilla::Field::Choice;
        $self->{visibility_value} ||=
            Bugzilla::Field::Choice->type($self->field->value_field)->new(
                $self->{visibility_value_id});
    }
    return $self->{visibility_value};
}

sub controlled_values {
    my $self = shift;
    return $self->{controlled_values} if defined $self->{controlled_values};
    my $fields = $self->field->controls_values_of;
    my %controlled_values;
    require Bugzilla::Field::Choice;
    foreach my $field (@$fields) {
        $controlled_values{$field->name} = 
            Bugzilla::Field::Choice->type($field)
            ->match({ visibility_value_id => $self->id });
    }
    $self->{controlled_values} = \%controlled_values;
    return $self->{controlled_values};
}

sub controlled_values_array {
    my ($self) = @_;
    my $values = $self->controlled_values;
    return [map { @{ $values->{$_} } } keys %$values];
}

sub is_visible_on_bug {
    my ($self, $bug) = @_;

    # Values currently set on the bug are always shown.
    return 1 if $self->is_set_on_bug($bug);

    # Inactive values are, otherwise, never shown.
    return 0 if !$self->is_active;

    # Values without a visibility value are, otherwise, always shown.
    my $visibility_value = $self->visibility_value;
    return 1 if !$visibility_value;

    # Values with a visibility value are only shown if the visibility
    # value is set on the bug.
    return $visibility_value->is_set_on_bug($bug); 
}

sub is_set_on_bug {
    my ($self, $bug) = @_;
    my $field_name = $self->FIELD_NAME;
    # This allows bug/create/create.html.tmpl to pass in a hashref that 
    # looks like a bug object.
    my $value = blessed($bug) ? $bug->$field_name : $bug->{$field_name};
    $value = $value->name if blessed($value);
    return 0 if !defined $value;

    if ($self->field->type == FIELD_TYPE_BUG_URLS
        or $self->field->type == FIELD_TYPE_MULTI_SELECT)
    {
        return grep($_ eq $self->name, @$value) ? 1 : 0;
    }
    return $value eq $self->name ? 1 : 0;
}

1;

__END__

=head1 NAME

Bugzilla::Field::ChoiceInterface - Makes an object act like a
Bugzilla::Field::Choice.

=head1 DESCRIPTION

This is an "interface", in the Java sense (sometimes called a "Role"
or a "Mixin" in other languages). L<Bugzilla::Field::Choice> is the
primary implementor of this interface, but other classes also implement
it if they want to "act like" L<Bugzilla::Field::Choice>.

=head1 METHODS

=head2 Accessors

These are in addition to the standard L<Bugzilla::Object> accessors.

=over

=item C<sortkey>

The key that determines the sort order of this item.

=item C<field>

The L<Bugzilla::Field> object that this field value belongs to.

=item C<is_active>

Whether or not this value should appear as an option on bugs that do
not already have it set as the current value.

=item C<is_static>

C<0> if this field value can be renamed or deleted, C<1> otherwise.

=item C<is_default>

C<1> if this is the default value for this field, C<0> otherwise.

=item C<bug_count>

An integer count of the number of bugs that have this value set.

=item C<controls_visibility_of_fields>

Returns an arrayref of L<Bugzilla::Field> objects, representing any
fields whose visibility are controlled by this field value.

=item C<controlled_values>

Tells you which values in B<other> fields appear (become visible) when this
value is set in its field.

Returns a hashref of arrayrefs. The hash keys are the names of fields,
and the values are arrays of objects that implement
C<Bugzilla::Field::ChoiceInterface>, representing values that this value 
controls the visibility of, for that field.

=item C<visibility_value>

Returns an object that implements C<Bugzilla::Field::ChoiceInterface>,
which represents the value that needs to be set in order for this
value to appear in the UI.

=item C<is_visible_on_bug>

Returns C<1> if, according to the settings of C<is_active> and 
C<visibility_value>, this value should be displayed as an option
when viewing a bug. Returns C<0> otherwise.

Takes a single argument, a L<Bugzilla::Bug> object or a hash with
similar fields to a L<Bugzilla::Bug> object.

=item C<is_set_on_bug>

Returns C<1> if this value is the current value set for its field on
the passed-in L<Bugzilla::Bug> object (or a hash that looks like a
L<Bugzilla::Bug>). For multi-valued fields, we return C<1> if
I<any> of the currently selected values are this value.

Returns C<0> otherwise.

=back

=head1 B<Methods in need of POD>

=over

=item FIELD_NAME

=item controlled_values_array

=back
