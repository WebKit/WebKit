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
# The Initial Developer of the Original Code is Everything Solved.
# Portions created by Everything Solved are Copyright (C) 2006 
# Everything Solved. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

use strict;

package Bugzilla::Object;

use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;

use Date::Parse;

use constant NAME_FIELD => 'name';
use constant ID_FIELD   => 'id';
use constant LIST_ORDER => NAME_FIELD;

use constant UPDATE_VALIDATORS => {};
use constant NUMERIC_COLUMNS   => ();
use constant DATE_COLUMNS      => ();

###############################
####    Initialization     ####
###############################

sub new {
    my $invocant = shift;
    my $class    = ref($invocant) || $invocant;
    my $object   = $class->_init(@_);
    bless($object, $class) if $object;
    return $object;
}


# Note: Because this uses sql_istrcmp, if you make a new object use
# Bugzilla::Object, make sure that you modify bz_setup_database
# in Bugzilla::DB::Pg appropriately, to add the right LOWER
# index. You can see examples already there.
sub _init {
    my $class = shift;
    my ($param) = @_;
    my $dbh = Bugzilla->dbh;
    my $columns = join(',', $class->DB_COLUMNS);
    my $table   = $class->DB_TABLE;
    my $name_field = $class->NAME_FIELD;
    my $id_field   = $class->ID_FIELD;

    my $id = $param unless (ref $param eq 'HASH');
    my $object;

    if (defined $id) {
        # We special-case if somebody specifies an ID, so that we can
        # validate it as numeric.
        detaint_natural($id)
          || ThrowCodeError('param_must_be_numeric',
                            {function => $class . '::_init'});

        $object = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM $table
             WHERE $id_field = ?}, undef, $id);
    } else {
        unless (defined $param->{name} || (defined $param->{'condition'} 
                                           && defined $param->{'values'}))
        {
            ThrowCodeError('bad_arg', { argument => 'param',
                                        function => $class . '::new' });
        }

        my ($condition, @values);
        if (defined $param->{name}) {
            $condition = $dbh->sql_istrcmp($name_field, '?');
            push(@values, $param->{name});
        }
        elsif (defined $param->{'condition'} && defined $param->{'values'}) {
            caller->isa('Bugzilla::Object')
                || ThrowCodeError('protection_violation',
                       { caller    => caller, 
                         function  => $class . '::new',
                         argument  => 'condition/values' });
            $condition = $param->{'condition'};
            push(@values, @{$param->{'values'}});
        }

        map { trick_taint($_) } @values;
        $object = $dbh->selectrow_hashref(
            "SELECT $columns FROM $table WHERE $condition", undef, @values);
    }

    return $object;
}

sub check {
    my ($invocant, $param) = @_;
    my $class = ref($invocant) || $invocant;
    # If we were just passed a name, then just use the name.
    if (!ref $param) {
        $param = { name => $param };
    }
    # Don't allow empty names.
    if (exists $param->{name}) {
        $param->{name} = trim($param->{name});
        $param->{name} || ThrowUserError('object_name_not_specified',
                                          { class => $class });
    }
    my $obj = $class->new($param)
        || ThrowUserError('object_does_not_exist', {%$param, class => $class});
    return $obj;
}

sub new_from_list {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my ($id_list) = @_;
    my $id_field = $class->ID_FIELD;

    my @detainted_ids;
    foreach my $id (@$id_list) {
        detaint_natural($id) ||
            ThrowCodeError('param_must_be_numeric',
                          {function => $class . '::new_from_list'});
        push(@detainted_ids, $id);
    }
    # We don't do $invocant->match because some classes have
    # their own implementation of match which is not compatible
    # with this one. However, match() still needs to have the right $invocant
    # in order to do $class->DB_TABLE and so on.
    return match($invocant, { $id_field => \@detainted_ids });
}

# Note: Future extensions to this could be:
#  * Add a MATCH_JOIN constant so that we can join against
#    certain other tables for the WHERE criteria.
sub match {
    my ($invocant, $criteria) = @_;
    my $class = ref($invocant) || $invocant;
    my $dbh   = Bugzilla->dbh;

    return [$class->get_all] if !$criteria;

    my (@terms, @values);
    foreach my $field (keys %$criteria) {
        $class->_check_field($field, 'match');
        my $value = $criteria->{$field};
        if (ref $value eq 'ARRAY') {
            # IN () is invalid SQL, and if we have an empty list
            # to match against, we're just returning an empty
            # array anyhow.
            return [] if !scalar @$value;

            my @qmarks = ("?") x @$value;
            push(@terms, $dbh->sql_in($field, \@qmarks));
            push(@values, @$value);
        }
        elsif ($value eq NOT_NULL) {
            push(@terms, "$field IS NOT NULL");
        }
        elsif ($value eq IS_NULL) {
            push(@terms, "$field IS NULL");
        }
        else {
            push(@terms, "$field = ?");
            push(@values, $value);
        }
    }

    my $where = join(' AND ', @terms);
    return $class->_do_list_select($where, \@values);
}

sub _do_list_select {
    my ($class, $where, $values) = @_;
    my $table = $class->DB_TABLE;
    my $cols  = join(',', $class->DB_COLUMNS);
    my $order = $class->LIST_ORDER;

    my $sql = "SELECT $cols FROM $table";
    if (defined $where) {
        $sql .= " WHERE $where ";
    }
    $sql .= " ORDER BY $order";

    my $dbh = Bugzilla->dbh;
    my $objects = $dbh->selectall_arrayref($sql, {Slice=>{}}, @$values);
    bless ($_, $class) foreach @$objects;
    return $objects
}

###############################
####      Accessors      ######
###############################

sub id   { return $_[0]->{$_[0]->ID_FIELD};   }
sub name { return $_[0]->{$_[0]->NAME_FIELD}; }

###############################
####        Methods        ####
###############################

sub set {
    my ($self, $field, $value) = @_;

    # This method is protected. It's used to help implement set_ functions.
    caller->isa('Bugzilla::Object')
        || ThrowCodeError('protection_violation', 
                          { caller     => caller,
                            superclass => __PACKAGE__,
                            function   => 'Bugzilla::Object->set' });

    my %validators = (%{$self->VALIDATORS}, %{$self->UPDATE_VALIDATORS});
    if (exists $validators{$field}) {
        my $validator = $validators{$field};
        $value = $self->$validator($value, $field);
        trick_taint($value) if (defined $value && !ref($value));

        if ($self->can('_set_global_validator')) {
            $self->_set_global_validator($value, $field);
        }
    }

    $self->{$field} = $value;
}

sub update {
    my $self = shift;

    my $dbh      = Bugzilla->dbh;
    my $table    = $self->DB_TABLE;
    my $id_field = $self->ID_FIELD;

    $dbh->bz_start_transaction();

    my $old_self = $self->new($self->id);
    
    my %numeric = map { $_ => 1 } $self->NUMERIC_COLUMNS;
    my %date    = map { $_ => 1 } $self->DATE_COLUMNS;
    my (@update_columns, @values, %changes);
    foreach my $column ($self->UPDATE_COLUMNS) {
        my ($old, $new) = ($old_self->{$column}, $self->{$column});
        # This has to be written this way in order to allow us to set a field
        # from undef or to undef, and avoid warnings about comparing an undef
        # with the "eq" operator.
        if (!defined $new || !defined $old) {
            next if !defined $new && !defined $old;
        }
        elsif ( ($numeric{$column} && $old == $new) 
                || ($date{$column} && str2time($old) == str2time($new))
                || $old eq $new ) {
            next;
        }

        trick_taint($new) if defined $new;
        push(@values, $new);
        push(@update_columns, $column);
        # We don't use $new because we don't want to detaint this for
        # the caller.
        $changes{$column} = [$old, $self->{$column}];
    }

    my $columns = join(', ', map {"$_ = ?"} @update_columns);

    $dbh->do("UPDATE $table SET $columns WHERE $id_field = ?", undef, 
             @values, $self->id) if @values;

    $dbh->bz_commit_transaction();

    return \%changes;
}

###############################
####      Subroutines    ######
###############################

sub create {
    my ($class, $params) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();
    $class->check_required_create_fields($params);
    my $field_values = $class->run_create_validators($params);
    my $object = $class->insert_create_data($field_values);
    $dbh->bz_commit_transaction();

    return $object;
}

# Used to validate that a field name is in fact a valid column in the
# current table before inserting it into SQL.
sub _check_field {
    my ($invocant, $field, $function) = @_;
    my $class = ref($invocant) || $invocant;
    if (!Bugzilla->dbh->bz_column_info($class->DB_TABLE, $field)) {
        ThrowCodeError('param_invalid', { param    => $field,
                                          function => "${class}::$function" });
    }
}

sub check_required_create_fields {
    my ($class, $params) = @_;

    foreach my $field ($class->REQUIRED_CREATE_FIELDS) {
        ThrowCodeError('param_required',
            { function => "${class}->create", param => $field })
            if !exists $params->{$field};
    }
}

sub run_create_validators {
    my ($class, $params) = @_;

    my $validators = $class->VALIDATORS;

    my %field_values;
    # We do the sort just to make sure that validation always
    # happens in a consistent order.
    foreach my $field (sort keys %$params) {
        my $value;
        if (exists $validators->{$field}) {
            my $validator = $validators->{$field};
            $value = $class->$validator($params->{$field}, $field);
        }
        else {
            $value = $params->{$field};
        }
        # We want people to be able to explicitly set fields to NULL,
        # and that means they can be set to undef.
        trick_taint($value) if defined $value && !ref($value);
        $field_values{$field} = $value;
    }

    return \%field_values;
}

sub insert_create_data {
    my ($class, $field_values) = @_;
    my $dbh = Bugzilla->dbh;

    my (@field_names, @values);
    while (my ($field, $value) = each %$field_values) {
        $class->_check_field($field, 'create');
        push(@field_names, $field);
        push(@values, $value);
    }

    my $qmarks = '?,' x @field_names;
    chop($qmarks);
    my $table = $class->DB_TABLE;
    $dbh->do("INSERT INTO $table (" . join(', ', @field_names)
             . ") VALUES ($qmarks)", undef, @values);
    my $id = $dbh->bz_last_key($table, $class->ID_FIELD);
    return $class->new($id);
}

sub get_all {
    my $class = shift;
    return @{$class->_do_list_select()};
}

###############################
####      Validators     ######
###############################

sub check_boolean { return $_[1] ? 1 : 0 }

1;

__END__

=head1 NAME

Bugzilla::Object - A base class for objects in Bugzilla.

=head1 SYNOPSIS

 my $object = new Bugzilla::Object(1);
 my $object = new Bugzilla::Object({name => 'TestProduct'});

 my $id          = $object->id;
 my $name        = $object->name;

=head1 DESCRIPTION

Bugzilla::Object is a base class for Bugzilla objects. You never actually
create a Bugzilla::Object directly, you only make subclasses of it.

Basically, Bugzilla::Object exists to allow developers to create objects
more easily. All you have to do is define C<DB_TABLE>, C<DB_COLUMNS>,
and sometimes C<LIST_ORDER> and you have a whole new object.

You should also define accessors for any columns other than C<name>
or C<id>.

=head1 CONSTANTS

Frequently, these will be the only things you have to define in your
subclass in order to have a fully-functioning object. C<DB_TABLE>
and C<DB_COLUMNS> are required.

=over

=item C<DB_TABLE>

The name of the table that these objects are stored in. For example,
for C<Bugzilla::Keyword> this would be C<keyworddefs>.

=item C<DB_COLUMNS>

The names of the columns that you want to read out of the database
and into this object. This should be an array.

=item C<NAME_FIELD>

The name of the column that should be considered to be the unique
"name" of this object. The 'name' is a B<string> that uniquely identifies
this Object in the database. Defaults to 'name'. When you specify 
C<{name => $name}> to C<new()>, this is the column that will be 
matched against in the DB.

=item C<ID_FIELD>

The name of the column that represents the unique B<integer> ID
of this object in the database. Defaults to 'id'.

=item C<LIST_ORDER>

The order that C<new_from_list> and C<get_all> should return objects
in. This should be the name of a database column. Defaults to
L</NAME_FIELD>.

=item C<REQUIRED_CREATE_FIELDS>

The list of fields that B<must> be specified when the user calls
C<create()>. This should be an array.

=item C<VALIDATORS>

A hashref that points to a function that will validate each param to
L</create>. 

Validators are called both by L</create> and L</set>. When
they are called by L</create>, the first argument will be the name
of the class (what we normally call C<$class>).

When they are called by L</set>, the first argument will be
a reference to the current object (what we normally call C<$self>).

The second argument will be the value passed to L</create> or 
L</set>for that field. 

The third argument will be the name of the field being validated.
This may be required by validators which validate several distinct fields.

These functions should call L<Bugzilla::Error/ThrowUserError> if they fail.

The validator must return the validated value.

=item C<UPDATE_VALIDATORS>

This is just like L</VALIDATORS>, but these validators are called only
when updating an object, not when creating it. Any validator that appears
here must not appear in L</VALIDATORS>.

L<Bugzilla::Bug> has good examples in its code of when to use this.

=item C<UPDATE_COLUMNS>

A list of columns to update when L</update> is called.
If a field can't be changed, it shouldn't be listed here. (For example,
the L</ID_FIELD> usually can't be updated.)

=item C<NUMERIC_COLUMNS>

When L</update> is called, it compares each column in the object to its
current value in the database. It only updates columns that have changed.

Any column listed in NUMERIC_COLUMNS is treated as a number, not as a string,
during these comparisons.

=item C<DATE_COLUMNS>

This is much like L</NUMERIC_COLUMNS>, except that it treats strings as
dates when being compared. So, for example, C<2007-01-01> would be
equal to C<2007-01-01 00:00:00>.

=back

=head1 METHODS

=head2 Constructors

=over

=item C<new>

=over

=item B<Description>

The constructor is used to load an existing object from the database,
by id or by name.

=item B<Params>

If you pass an integer, the integer is the id of the object, 
from the database, that we  want to read in. (id is defined
as the value in the L</ID_FIELD> column).

If you pass in a hashref, you can pass a C<name> key. The 
value of the C<name> key is the case-insensitive name of the object 
(from L</NAME_FIELD>) in the DB.

B<Additional Parameters Available for Subclasses>

If you are a subclass of C<Bugzilla::Object>, you can pass
C<condition> and C<values> as hash keys, instead of the above.

C<condition> is a set of SQL conditions for the WHERE clause, which contain
placeholders.

C<values> is a reference to an array. The array contains the values
for each placeholder in C<condition>, in order.

This is to allow subclasses to have complex parameters, and then to
translate those parameters into C<condition> and C<values> when they
call C<$self->SUPER::new> (which is this function, usually).

If you try to call C<new> outside of a subclass with the C<condition>
and C<values> parameters, Bugzilla will throw an error. These parameters
are intended B<only> for use by subclasses.

=item B<Returns>

A fully-initialized object, or C<undef> if there is no object in the
database matching the parameters you passed in.

=back

=item C<check>

=over

=item B<Description>

Checks if there is an object in the database with the specified name, and
throws an error if you specified an empty name, or if there is no object
in the database with that name.

=item B<Params>

The parameters are the same as for L</new>, except that if you don't pass
a hashref, the single argument is the I<name> of the object, not the id.

=item B<Returns>

A fully initialized object, guaranteed.

=item B<Notes For Implementors>

If you implement this in your subclass, make sure that you also update
the C<object_name> block at the bottom of the F<global/user-error.html.tmpl>
template.

=back

=item C<new_from_list(\@id_list)>

 Description: Creates an array of objects, given an array of ids.

 Params:      \@id_list - A reference to an array of numbers, database ids.
                          If any of these are not numeric, the function
                          will throw an error. If any of these are not
                          valid ids in the database, they will simply 
                          be skipped.

 Returns:     A reference to an array of objects.

=item C<match>

=over

=item B<Description>

Gets a list of objects from the database based on certain criteria.

Basically, a simple way of doing a sort of "SELECT" statement (like SQL)
to get objects.

All criteria are joined by C<AND>, so adding more criteria will give you
a smaller set of results, not a larger set.

=item B<Params>

A hashref, where the keys are column names of the table, pointing to the 
value that you want to match against for that column. 

There are two special values, the constants C<NULL> and C<NOT_NULL>,
which means "give me objects where this field is NULL or NOT NULL,
respectively."

If you don't specify any criteria, calling this function is the same
as doing C<[$class-E<gt>get_all]>.

=item B<Returns>

An arrayref of objects, or an empty arrayref if there are no matches.

=back

=back

=head2 Database Manipulation

=over

=item C<create>

Description: Creates a new item in the database.
             Throws a User Error if any of the passed-in params
             are invalid.

Params:      C<$params> - hashref - A value to put in each database
               field for this object. Certain values must be set (the 
               ones specified in L</REQUIRED_CREATE_FIELDS>), and
               the function will throw a Code Error if you don't set
               them.

Returns:     The Object just created in the database.

Notes:       In order for this function to work in your subclass,
             your subclass's L</ID_FIELD> must be of C<SERIAL>
             type in the database. Your subclass also must
             define L</REQUIRED_CREATE_FIELDS> and L</VALIDATORS>.

             Subclass Implementors: This function basically just
             calls L</check_required_create_fields>, then
             L</run_create_validators>, and then finally
             L</insert_create_data>. So if you have a complex system that
             you need to implement, you can do it by calling these
             three functions instead of C<SUPER::create>.

=item C<check_required_create_fields>

=over

=item B<Description>

Part of L</create>. Throws an error if any of the L</REQUIRED_CREATE_FIELDS>
have not been specified in C<$params>

=item B<Params>

=over

=item C<$params> - The same as C<$params> from L</create>.

=back

=item B<Returns> (nothing)

=back

=item C<run_create_validators>

Description: Runs the validation of input parameters for L</create>.
             This subroutine exists so that it can be overridden
             by subclasses who need to do special validations
             of their input parameters. This method is B<only> called
             by L</create>.

Params:      The same as L</create>.

Returns:     A hash, in a similar format as C<$params>, except that
             these are the values to be inserted into the database,
             not the values that were input to L</create>.

=item C<insert_create_data>

Part of L</create>.

Takes the return value from L</run_create_validators> and inserts the
data into the database. Returns a newly created object. 

=item C<update>

=over

=item B<Description>

Saves the values currently in this object to the database.
Only the fields specified in L</UPDATE_COLUMNS> will be
updated, and they will only be updated if their values have changed.

=item B<Params> (none)

=item B<Returns>

A hashref showing what changed during the update. The keys are the column
names from L</UPDATE_COLUMNS>. If a field was not changed, it will not be
in the hash at all. If the field was changed, the key will point to an arrayref.
The first item of the arrayref will be the old value, and the second item
will be the new value.

If there were no changes, we return a reference to an empty hash.

=back

=back

=head2 Subclass Helpers

These functions are intended only for use by subclasses. If
you call them from anywhere else, they will throw a C<CodeError>.

=over

=item C<set>

=over

=item B<Description>

Sets a certain hash member of this class to a certain value.
Used for updating fields. Calls the validator for this field,
if it exists. Subclasses should use this function
to implement the various C<set_> mutators for their different
fields.

If your class defines a method called C<_set_global_validator>,
C<set> will call it with C<($value, $field)> as arguments, after running
the validator for this particular field. C<_set_global_validator> does not
return anything.


See L</VALIDATORS> for more information.

=item B<Params>

=over

=item C<$field> - The name of the hash member to update. This should
be the same as the name of the field in L</VALIDATORS>, if it exists there.

=item C<$value> - The value that you're setting the field to.

=back

=item B<Returns> (nothing)

=back

=back

=head2 Simple Validators

You can use these in your subclass L</VALIDATORS> or L</UPDATE_VALIDATORS>.
Note that you have to reference them like C<\&Bugzilla::Object::check_boolean>,
you can't just write C<\&check_boolean>.

=over

=item C<check_boolean>

Returns C<1> if the passed-in value is true, C<0> otherwise.

=back

=head1 CLASS FUNCTIONS

=over

=item C<get_all>

 Description: Returns all objects in this table from the database.

 Params:      none.

 Returns:     A list of objects, or an empty list if there are none.

 Notes:       Note that you must call this as C<$class->get_all>. For 
              example, C<Bugzilla::Keyword->get_all>. 
              C<Bugzilla::Keyword::get_all> will not work.

=back

=cut
