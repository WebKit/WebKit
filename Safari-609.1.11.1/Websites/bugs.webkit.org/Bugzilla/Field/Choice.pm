# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Field::Choice;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Field::ChoiceInterface Bugzilla::Object);

use Bugzilla::Config qw(SetParam write_params);
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Field;
use Bugzilla::Util qw(trim detaint_natural);

use Scalar::Util qw(blessed);

##################
# Initialization #
##################

use constant IS_CONFIG => 1;

use constant DB_COLUMNS => qw(
    id
    value
    sortkey
    isactive
    visibility_value_id
);

use constant UPDATE_COLUMNS => qw(
    value
    sortkey
    isactive
    visibility_value_id
);

use constant NAME_FIELD => 'value';
use constant LIST_ORDER => 'sortkey, value';

use constant VALIDATORS => {
    value   => \&_check_value,
    sortkey => \&_check_sortkey,
    visibility_value_id => \&_check_visibility_value_id,
    isactive => \&_check_isactive,
};

use constant CLASS_MAP => {
    bug_status => 'Bugzilla::Status',
    classification => 'Bugzilla::Classification',
    component  => 'Bugzilla::Component',
    product    => 'Bugzilla::Product',
};

use constant DEFAULT_MAP => {
    op_sys       => 'defaultopsys',
    rep_platform => 'defaultplatform',
    priority     => 'defaultpriority',
    bug_severity => 'defaultseverity',
};

#################
# Class Factory #
#################

# Bugzilla::Field::Choice is actually an abstract base class. Every field
# type has its own dynamically-generated class for its values. This allows
# certain fields to have special types, like how bug_status's values
# are Bugzilla::Status objects.

sub type {
    my ($class, $field) = @_;
    my $field_obj = blessed $field ? $field : Bugzilla::Field->check($field);
    my $field_name = $field_obj->name;

    if (my $package = $class->CLASS_MAP->{$field_name}) {
        # Callers expect the module to be already loaded.
        eval "require $package";
        return $package;
    }

    # For generic classes, we use a lowercase class name, so as
    # not to interfere with any real subclasses we might make some day.
    my $package = "Bugzilla::Field::Choice::$field_name";
    Bugzilla->request_cache->{"field_$package"} = $field_obj;

    # This package only needs to be created once. We check if the DB_TABLE
    # glob for this package already exists, which tells us whether or not
    # we need to create the package (this works even under mod_perl, where
    # this package definition will persist across requests)).
    if (!defined *{"${package}::DB_TABLE"}) {
        eval <<EOC;
            package $package;
            use parent qw(Bugzilla::Field::Choice);
            use constant DB_TABLE => '$field_name';
EOC
    }

    return $package;
}

################
# Constructors #
################

# We just make new() enforce this, which should give developers 
# the understanding that you can't use Bugzilla::Field::Choice
# without calling type().
sub new {
    my $class = shift;
    if ($class eq 'Bugzilla::Field::Choice') {
        ThrowCodeError('field_choice_must_use_type');
    }
    $class->SUPER::new(@_);
}

#########################
# Database Manipulation #
#########################

# Our subclasses can take more arguments than we normally accept.
# So, we override create() to remove arguments that aren't valid
# columns. (Normally Bugzilla::Object dies if you pass arguments
# that aren't valid columns.)
sub create {
    my $class = shift;
    my ($params) = @_;
    foreach my $key (keys %$params) {
        if (!grep {$_ eq $key} $class->_get_db_columns) {
            delete $params->{$key};
        }
    }
    return $class->SUPER::create(@_);
}

sub update {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $fname = $self->field->name;

    $dbh->bz_start_transaction();

    my ($changes, $old_self) = $self->SUPER::update(@_);
    if (exists $changes->{value}) {
        my ($old, $new) = @{ $changes->{value} };
        if ($self->field->type == FIELD_TYPE_MULTI_SELECT) {
            $dbh->do("UPDATE bug_$fname SET value = ? WHERE value = ?",
                     undef, $new, $old);
        }
        else {
            $dbh->do("UPDATE bugs SET $fname = ? WHERE $fname = ?",
                     undef, $new, $old);
        }

        if ($old_self->is_default) {
            my $param = $self->DEFAULT_MAP->{$self->field->name};
            SetParam($param, $self->name);
            write_params();
        }
    }

    $dbh->bz_commit_transaction();
    return wantarray ? ($changes, $old_self) : $changes;
}

sub remove_from_db {
    my $self = shift;
    if ($self->is_default) {
        ThrowUserError('fieldvalue_is_default',
                       { field => $self->field, value => $self,
                         param_name => $self->DEFAULT_MAP->{$self->field->name},
                       });
    }
    if ($self->is_static) {
        ThrowUserError('fieldvalue_not_deletable', 
                       { field => $self->field, value => $self });
    }
    if ($self->bug_count) {
        ThrowUserError("fieldvalue_still_has_bugs",
                       { field => $self->field, value => $self });
    }
    $self->_check_if_controller(); # From ChoiceInterface.
    $self->SUPER::remove_from_db();
}

############
# Mutators #
############

sub set_is_active { $_[0]->set('isactive', $_[1]); }
sub set_name      { $_[0]->set('value', $_[1]);    }
sub set_sortkey   { $_[0]->set('sortkey', $_[1]);  }
sub set_visibility_value {
    my ($self, $value) = @_;
    $self->set('visibility_value_id', $value);
    delete $self->{visibility_value};
}

##############
# Validators #
##############

sub _check_isactive {
    my ($invocant, $value) = @_;
    $value = Bugzilla::Object::check_boolean($invocant, $value);
    if (!$value and ref $invocant) {
        if ($invocant->is_default) {
            my $field = $invocant->field;
            ThrowUserError('fieldvalue_is_default', 
                           { value => $invocant, field => $field,
                             param_name => $invocant->DEFAULT_MAP->{$field->name}
                           });
        }
        if ($invocant->is_static) {
            ThrowUserError('fieldvalue_not_deletable',
                           { value => $invocant, field => $invocant->field });
        }
    }
    return $value;
}

sub _check_value {
    my ($invocant, $value) = @_;

    my $field = $invocant->field;

    $value = trim($value);

    # Make sure people don't rename static values
    if (blessed($invocant) && $value ne $invocant->name 
        && $invocant->is_static) 
    {
        ThrowUserError('fieldvalue_not_editable',
                       { field => $field, old_value => $invocant });
    }

    ThrowUserError('fieldvalue_undefined') if !defined $value || $value eq "";
    ThrowUserError('fieldvalue_name_too_long', { value => $value })
        if length($value) > MAX_FIELD_VALUE_SIZE;

    my $exists = $invocant->type($field)->new({ name => $value });
    if ($exists && (!blessed($invocant) || $invocant->id != $exists->id)) {
        ThrowUserError('fieldvalue_already_exists', 
                       { field => $field, value => $exists });
    }

    return $value;
}

sub _check_sortkey {
    my ($invocant, $value) = @_;
    $value = trim($value);
    return 0 if !$value;
    # Store for the error message in case detaint_natural clears it.
    my $orig_value = $value;
    (detaint_natural($value) && $value <= MAX_SMALLINT)
        || ThrowUserError('fieldvalue_sortkey_invalid',
                          { sortkey => $orig_value,
                            field   => $invocant->field });
    return $value;
}

sub _check_visibility_value_id {
    my ($invocant, $value_id) = @_;
    $value_id = trim($value_id);
    my $field = $invocant->field->value_field;
    return undef if !$field || !$value_id;
    my $value_obj = Bugzilla::Field::Choice->type($field)
                    ->check({ id => $value_id });
    return $value_obj->id;
}

1;

__END__

=head1 NAME

Bugzilla::Field::Choice - A legal value for a <select>-type field.

=head1 SYNOPSIS

 my $field = new Bugzilla::Field({name => 'bug_status'});

 my $choice = new Bugzilla::Field::Choice->type($field)->new(1);

 my $choices = Bugzilla::Field::Choice->type($field)->new_from_list([1,2,3]);
 my $choices = Bugzilla::Field::Choice->type($field)->get_all();
 my $choices = Bugzilla::Field::Choice->type($field->match({ sortkey => 10 }); 

=head1 DESCRIPTION

This is an implementation of L<Bugzilla::Object>, but with a twist.
You can't call any class methods (such as C<new>, C<create>, etc.) 
directly on C<Bugzilla::Field::Choice> itself. Instead, you have to
call C<Bugzilla::Field::Choice-E<gt>type($field)> to get the class
you're going to instantiate, and then you call the methods on that.

We do that because each field has its own database table for its values, so
each value type needs its own class.

See the L</SYNOPSIS> for examples of how this works.

This class implements L<Bugzilla::Field::ChoiceInterface>, and so all
methods of that class are also available here.

=head1 METHODS

=head2 Class Factory

In object-oriented design, a "class factory" is a method that picks
and returns the right class for you, based on an argument that you pass.

=over

=item C<type>

Takes a single argument, which is either the name of a field from the
C<fielddefs> table, or a L<Bugzilla::Field> object representing a field.

Returns an appropriate subclass of C<Bugzilla::Field::Choice> that you
can now call class methods on (like C<new>, C<create>, C<match>, etc.)

B<NOTE>: YOU CANNOT CALL CLASS METHODS ON C<Bugzilla::Field::Choice>. You
must call C<type> to get a class you can call methods on.

=back

=head2 Mutators

This class implements mutators for all of the settable accessors in
L<Bugzilla::Field::ChoiceInterface>.

=head1 B<Methods in need of POD>

=over

=item create

=item remove_from_db

=item set_is_active

=item set_sortkey

=item set_name

=item update

=item set_visibility_value

=back
