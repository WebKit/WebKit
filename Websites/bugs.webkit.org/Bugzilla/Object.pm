# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Object;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Hook;
use Bugzilla::Util;
use Bugzilla::Error;

use Date::Parse;
use List::MoreUtils qw(part);
use Scalar::Util qw(blessed);

use constant NAME_FIELD => 'name';
use constant ID_FIELD   => 'id';
use constant LIST_ORDER => NAME_FIELD;

use constant UPDATE_VALIDATORS => {};
use constant NUMERIC_COLUMNS   => ();
use constant DATE_COLUMNS      => ();
use constant VALIDATOR_DEPENDENCIES => {};
# XXX At some point, this will be joined with FIELD_MAP.
use constant REQUIRED_FIELD_MAP  => {};
use constant EXTRA_REQUIRED_FIELDS => ();
use constant AUDIT_CREATES => 1;
use constant AUDIT_UPDATES => 1;
use constant AUDIT_REMOVES => 1;

# When USE_MEMCACHED is true, the class is suitable for serialisation to
# Memcached.  See documentation in Bugzilla::Memcached for more information.
use constant USE_MEMCACHED => 1;

# When IS_CONFIG is true, the class is used to track seldom changed
# configuration objects.  This includes, but is not limited to, fields, field
# values, keywords, products, classifications, priorities, severities, etc.
use constant IS_CONFIG => 0;

# This allows the JSON-RPC interface to return Bugzilla::Object instances
# as though they were hashes. In the future, this may be modified to return
# less information.
sub TO_JSON { return { %{ $_[0] } }; }

###############################
####    Initialization     ####
###############################

sub new {
    my $invocant = shift;
    my $class    = ref($invocant) || $invocant;
    my $param    = shift;

    my $object = $class->_object_cache_get($param);
    return $object if $object;

    my ($data, $set_memcached);
    if (Bugzilla->memcached->enabled
        && $class->USE_MEMCACHED
        && ref($param) eq 'HASH' && $param->{cache})
    {
        if (defined $param->{id}) {
            $data = Bugzilla->memcached->get({
                table => $class->DB_TABLE,
                id    => $param->{id},
            });
        }
        elsif (defined $param->{name}) {
            $data = Bugzilla->memcached->get({
                table => $class->DB_TABLE,
                name  => $param->{name},
            });
        }
        $set_memcached = $data ? 0 : 1;
    }
    $data ||= $class->_load_from_db($param);

    if ($data && $set_memcached) {
        Bugzilla->memcached->set({
            table => $class->DB_TABLE,
            id    => $data->{$class->ID_FIELD},
            name  => $data->{$class->NAME_FIELD},
            data  => $data,
        });
    }

    $object = $class->new_from_hash($data);
    $class->_object_cache_set($param, $object);

    return $object;
}

# Note: Because this uses sql_istrcmp, if you make a new object use
# Bugzilla::Object, make sure that you modify bz_setup_database
# in Bugzilla::DB::Pg appropriately, to add the right LOWER
# index. You can see examples already there.
sub _load_from_db {
    my $class = shift;
    my ($param) = @_;
    my $dbh = Bugzilla->dbh;
    my $columns = join(',', $class->_get_db_columns);
    my $table   = $class->DB_TABLE;
    my $name_field = $class->NAME_FIELD;
    my $id_field   = $class->ID_FIELD;

    my $id = $param;
    if (ref $param eq 'HASH') {
        $id = $param->{id};
    }

    my $object_data;
    if (defined $id) {
        # We special-case if somebody specifies an ID, so that we can
        # validate it as numeric.
        detaint_natural($id)
          || ThrowCodeError('param_must_be_numeric',
                            {function => $class . '::_load_from_db'});

        # Too large integers make PostgreSQL crash.
        return if $id > MAX_INT_32;

        $object_data = $dbh->selectrow_hashref(qq{
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
        $object_data = $dbh->selectrow_hashref(
            "SELECT $columns FROM $table WHERE $condition", undef, @values);
    }
    return $object_data;
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
        # Too large integers make PostgreSQL crash.
        next if $id > MAX_INT_32;
        push(@detainted_ids, $id);
    }

    # We don't do $invocant->match because some classes have
    # their own implementation of match which is not compatible
    # with this one. However, match() still needs to have the right $invocant
    # in order to do $class->DB_TABLE and so on.
    return match($invocant, { $id_field => \@detainted_ids });
}

sub new_from_hash {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $object_data = shift || return;
    $class->_serialisation_keys($object_data);
    bless($object_data, $class);
    $object_data->initialize();
    return $object_data;
}

sub initialize {
    # abstract
}

# Provides a mechanism for objects to be cached in the request_cache

sub object_cache_get {
    my ($class, $id) = @_;
    return $class->_object_cache_get(
        { id => $id, cache => 1},
        $class
    );
}

sub object_cache_set {
    my $self = shift;
    return $self->_object_cache_set(
        { id => $self->id, cache => 1 },
        $self
    );
}

sub _object_cache_get {
    my $class = shift;
    my ($param) = @_;
    my $cache_key = $class->object_cache_key($param)
      || return;
    return Bugzilla->request_cache->{$cache_key};
}

sub _object_cache_set {
    my $class = shift;
    my ($param, $object) = @_;
    my $cache_key = $class->object_cache_key($param)
      || return;
    Bugzilla->request_cache->{$cache_key} = $object;
}

sub _object_cache_remove {
    my $class = shift;
    my ($param) = @_;
    $param->{cache} = 1;
    my $cache_key = $class->object_cache_key($param)
      || return;
    delete Bugzilla->request_cache->{$cache_key};
}

sub object_cache_key {
    my $class = shift;
    my ($param) = @_;
    if (ref($param) && $param->{cache} && ($param->{id} || $param->{name})) {
        $class = blessed($class) if blessed($class);
        return $class  . ',' . ($param->{id} || $param->{name});
    } else {
        return;
    }
}

# To support serialisation, we need to capture the keys in an object's default
# hashref.
sub _serialisation_keys {
    my ($class, $object) = @_;
    my $cache = Bugzilla->request_cache->{serialisation_keys} ||= {};
    $cache->{$class} = [ keys %$object ] if $object && !exists $cache->{$class};
    return @{ $cache->{$class} };
}

sub check {
    my ($invocant, $param) = @_;
    my $class = ref($invocant) || $invocant;
    # If we were just passed a name, then just use the name.
    if (!ref $param) {
        $param = { name => $param };
    }

    # Don't allow empty names or ids.
    my $check_param = exists $param->{id} ? 'id' : 'name';
    $param->{$check_param} = trim($param->{$check_param});
    # If somebody passes us "0", we want to throw an error like
    # "there is no X with the name 0". This is true even for ids. So here,
    # we only check if the parameter is undefined or empty.
    if (!defined $param->{$check_param} or $param->{$check_param} eq '') {
        ThrowUserError('object_not_specified', { class => $class });
    }

    my $obj = $class->new($param);
    if (!$obj) {
        # We don't want to override the normal template "user" object if
        # "user" is one of the params.
        delete $param->{user};
        if (my $error = delete $param->{_error}) {
            ThrowUserError($error, { %$param, class => $class });
        }
        else {
            ThrowUserError('object_does_not_exist', { %$param, class => $class });
        }
    }
    return $obj;
}

# Note: Future extensions to this could be:
#  * Add a MATCH_JOIN constant so that we can join against
#    certain other tables for the WHERE criteria.
sub match {
    my ($invocant, $criteria) = @_;
    my $class = ref($invocant) || $invocant;
    my $dbh   = Bugzilla->dbh;

    return [$class->get_all] if !$criteria;

    my (@terms, @values, $postamble);
    foreach my $field (keys %$criteria) {
        my $value = $criteria->{$field};
        
        # allow for LIMIT and OFFSET expressions via the criteria.
        next if $field eq 'OFFSET';
        if ( $field eq 'LIMIT' ) {
            next unless defined $value;
            detaint_natural($value)
              or ThrowCodeError('param_must_be_numeric', 
                                { param    => 'LIMIT', 
                                  function => "${class}::match" });
            my $offset;
            if (defined $criteria->{OFFSET}) {
                $offset = $criteria->{OFFSET};
                detaint_signed($offset)
                  or ThrowCodeError('param_must_be_numeric', 
                                    { param    => 'OFFSET',
                                      function => "${class}::match" });
            }
            $postamble = $dbh->sql_limit($value, $offset);
            next;
        }
        elsif ( $field eq 'WHERE' ) {
            # the WHERE value is a hashref where the keys are
            # "column_name operator ?" and values are the placeholder's
            # value (either a scalar or an array of values).
            foreach my $k (keys %$value) {
                push(@terms, $k);
                my @this_value = ref($value->{$k}) ? @{ $value->{$k} } 
                                                   : ($value->{$k});
                push(@values, @this_value);
            }            
            next;
        }

        # It's always safe to use the field defined by classes as being
        # their ID field. In particular, this means that new_from_list()
        # is exempted from this check.
        $class->_check_field($field, 'match') unless $field eq $class->ID_FIELD;

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

    my $where = join(' AND ', @terms) if scalar @terms;
    return $class->_do_list_select($where, \@values, $postamble);
}

sub _do_list_select {
    my ($class, $where, $values, $postamble) = @_;
    my $table = $class->DB_TABLE;
    my $cols  = join(',', $class->_get_db_columns);
    my $order = $class->LIST_ORDER;

    # Unconditional requests for configuration data are cacheable.
    my ($objects, $set_memcached, $memcached_key);
    if (!defined $where
        && Bugzilla->memcached->enabled
        && $class->IS_CONFIG)
    {
        $memcached_key = "$class:get_all";
        $objects = Bugzilla->memcached->get_config({ key => $memcached_key });
        $set_memcached = $objects ? 0 : 1;
    }

    if (!$objects) {
        my $sql = "SELECT $cols FROM $table";
        if (defined $where) {
            $sql .= " WHERE $where ";
        }
        $sql .= " ORDER BY $order";
        $sql .= " $postamble" if $postamble;

        my $dbh = Bugzilla->dbh;
        # Sometimes the values are tainted, but we don't want to untaint them
        # for the caller. So we copy the array. It's safe to untaint because
        # they're only used in placeholders here.
        my @untainted = @{ $values || [] };
        trick_taint($_) foreach @untainted;
        $objects = $dbh->selectall_arrayref($sql, {Slice=>{}}, @untainted);
        $class->_serialisation_keys($objects->[0]) if @$objects;
    }

    if ($objects && $set_memcached) {
        Bugzilla->memcached->set_config({
            key  => $memcached_key,
            data => $objects
        });
    }

    foreach my $object (@$objects) {
        $object = $class->new_from_hash($object);
    }
    return $objects;
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
    my $caller = caller;
    $caller->isa('Bugzilla::Object') || $caller->isa('Bugzilla::Extension')
        || ThrowCodeError('protection_violation', 
                          { caller     => caller,
                            superclass => __PACKAGE__,
                            function   => 'Bugzilla::Object->set' });

    Bugzilla::Hook::process('object_before_set',
                            { object => $self, field => $field,
                              value => $value });

    my %validators = (%{$self->_get_validators}, %{$self->UPDATE_VALIDATORS});
    if (exists $validators{$field}) {
        my $validator = $validators{$field};
        $value = $self->$validator($value, $field);
        trick_taint($value) if (defined $value && !ref($value));

        if ($self->can('_set_global_validator')) {
            $self->_set_global_validator($value, $field);
        }
    }

    $self->{$field} = $value;

    Bugzilla::Hook::process('object_end_of_set',
                            { object => $self, field => $field });
}

sub set_all {
    my ($self, $params) = @_;

    # Don't let setters modify the values in $params for the caller.
    my %field_values = %$params;

    my @sorted_names = $self->_sort_by_dep(keys %field_values);

    foreach my $key (@sorted_names) {
        # It's possible for one set_ method to delete a key from $params
        # for another set method, so if that's happened, we don't call the
        # other set method.
        next if !exists $field_values{$key};
        my $method = "set_$key";
        if (!$self->can($method)) {
            my $class = ref($self) || $self;
            ThrowCodeError("unknown_method", { method => "${class}::${method}" });
        }
        $self->$method($field_values{$key}, \%field_values);
    }
    Bugzilla::Hook::process('object_end_of_set_all', 
                            { object => $self, params => \%field_values });
}

sub update {
    my $self = shift;

    my $dbh      = Bugzilla->dbh;
    my $table    = $self->DB_TABLE;
    my $id_field = $self->ID_FIELD;

    $dbh->bz_start_transaction();

    my $old_self = $self->new($self->id);
   
    my @all_columns = $self->UPDATE_COLUMNS;
    my @hook_columns;
    Bugzilla::Hook::process('object_update_columns',
                            { object => $self, columns => \@hook_columns });
    push(@all_columns, @hook_columns);

    my %numeric = map { $_ => 1 } $self->NUMERIC_COLUMNS;
    my %date    = map { $_ => 1 } $self->DATE_COLUMNS;
    my (@update_columns, @values, %changes);
    foreach my $column (@all_columns) {
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

    Bugzilla::Hook::process('object_end_of_update',
                            { object => $self, old_object => $old_self,
                              changes => \%changes });

    $self->audit_log(\%changes) if $self->AUDIT_UPDATES;

    $dbh->bz_commit_transaction();
    if ($self->USE_MEMCACHED && @values) {
        Bugzilla->memcached->clear({ table => $table, id => $self->id });
        Bugzilla->memcached->clear_config()
            if $self->IS_CONFIG;
    }
    $self->_object_cache_remove({ id => $self->id });
    $self->_object_cache_remove({ name => $self->name }) if $self->name;

    if (wantarray) {
        return (\%changes, $old_self);
    }

    return \%changes;
}

sub remove_from_db {
    my $self = shift;
    Bugzilla::Hook::process('object_before_delete', { object => $self });
    my $table = $self->DB_TABLE;
    my $id_field = $self->ID_FIELD;
    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    $self->audit_log(AUDIT_REMOVE) if $self->AUDIT_REMOVES;
    $dbh->do("DELETE FROM $table WHERE $id_field = ?", undef, $self->id);
    $dbh->bz_commit_transaction();
    if ($self->USE_MEMCACHED) {
        Bugzilla->memcached->clear({ table => $table, id => $self->id });
        Bugzilla->memcached->clear_config()
            if $self->IS_CONFIG;
    }
    $self->_object_cache_remove({ id => $self->id });
    $self->_object_cache_remove({ name => $self->name }) if $self->name;
    undef $self;
}

sub audit_log {
    my ($self, $changes) = @_;
    my $class = ref $self;
    my $dbh = Bugzilla->dbh;
    my $user_id = Bugzilla->user->id || undef;
    my $sth = $dbh->prepare(
        'INSERT INTO audit_log (user_id, class, object_id, field,
                                removed, added, at_time) 
              VALUES (?,?,?,?,?,?,LOCALTIMESTAMP(0))');
    # During creation or removal, $changes is actually just a string
    # indicating whether we're creating or removing the object.
    if ($changes eq AUDIT_CREATE or $changes eq AUDIT_REMOVE) {
        # We put the object's name in the "added" or "removed" field.
        # We do this thing with NAME_FIELD because $self->name returns
        # the wrong thing for Bugzilla::User.
        my $name = $self->{$self->NAME_FIELD};
        my @added_removed = $changes eq AUDIT_CREATE ? (undef, $name) 
                                                     : ($name, undef);
        $sth->execute($user_id, $class, $self->id, $changes, @added_removed);
        return;
    }

    # During update, it's the actual %changes hash produced by update().
    foreach my $field (keys %$changes) {
        # Skip private changes.
        next if $field =~ /^_/;
        my ($from, $to) = $self->_sanitize_audit_log($field, $changes->{$field});
        $sth->execute($user_id, $class, $self->id, $field, $from, $to);
    }
}

sub _sanitize_audit_log {
    my ($self, $field, $changes) = @_;
    my $class = ref($self) || $self;

    # Do not store hashed passwords. Only record the algorithm used to encode them.
    if ($class eq 'Bugzilla::User' && $field eq 'cryptpassword') {
        foreach my $passwd (@$changes) {
            next unless $passwd;
            my $algorithm = 'unknown_algorithm';
            if ($passwd =~ /{([^}]+)}$/) {
                $algorithm = $1;
            }
            $passwd = "hashed_with_$algorithm";
        }
    }
    return @$changes;
}

sub flatten_to_hash {
    my $self = shift;
    my $class = blessed($self);
    my %hash = map { $_ => $self->{$_} } $class->_serialisation_keys;
    return \%hash;
}

###############################
####      Subroutines    ######
###############################

sub any_exist {
    my $class = shift;
    my $table = $class->DB_TABLE;
    my $dbh = Bugzilla->dbh;
    my $any_exist = $dbh->selectrow_array(
        "SELECT 1 FROM $table " . $dbh->sql_limit(1));
    return $any_exist ? 1 : 0;
}

sub create {
    my ($class, $params) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_start_transaction();
    $class->check_required_create_fields($params);
    my $field_values = $class->run_create_validators($params);
    my $object = $class->insert_create_data($field_values);
    $dbh->bz_commit_transaction();

    if (Bugzilla->memcached->enabled
        && $class->USE_MEMCACHED
        && $class->IS_CONFIG)
    {
        Bugzilla->memcached->clear_config();
    }

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

    # This hook happens here so that even subclasses that don't call
    # SUPER::create are still affected by the hook.
    Bugzilla::Hook::process('object_before_create', { class => $class,
                                                      params => $params });

    my @check_fields = $class->_required_create_fields();
    foreach my $field (@check_fields) {
        $params->{$field} = undef if !exists $params->{$field};
    }
}

sub run_create_validators {
    my ($class, $params, $options) = @_;

    my $validators = $class->_get_validators;
    my %field_values = %$params;

    # Make a hash skiplist for easier searching later
    my %skip_list = map { $_ => 1 } @{ $options->{skip} || [] };

    # Get the sorted field names
    my @sorted_names = $class->_sort_by_dep(keys %field_values);

    # Remove the skipped names
    my @unskipped = grep { !$skip_list{$_} } @sorted_names;

    foreach my $field (@unskipped) {
        my $value;
        if (exists $validators->{$field}) {
            my $validator = $validators->{$field};
            $value = $class->$validator($field_values{$field}, $field,
                                        \%field_values);
        }
        else {
            $value = $field_values{$field};
        }

        # We want people to be able to explicitly set fields to NULL,
        # and that means they can be set to undef.
        trick_taint($value) if defined $value && !ref($value);
        $field_values{$field} = $value;
    }

    Bugzilla::Hook::process('object_end_of_create_validators',
                            { class => $class, params => \%field_values });

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

    my $object = $class->new($id);

    Bugzilla::Hook::process('object_end_of_create', { class => $class,
                                                      object => $object });
    $object->audit_log(AUDIT_CREATE) if $object->AUDIT_CREATES;

    return $object;
}

sub get_all {
    my $class = shift;
    return @{ $class->_do_list_select() };
}

###############################
####      Validators     ######
###############################

sub check_boolean { return $_[1] ? 1 : 0 }

sub check_time {
    my ($invocant, $value, $field, $params, $allow_negative) = @_;

    # If we don't have a current value default to zero
    my $current = blessed($invocant) ? $invocant->{$field}
                                     : 0;
    $current ||= 0;

    # Get the new value or zero if it isn't defined
    $value = trim($value) || 0;

    # Make sure the new value is well formed
    _validate_time($value, $field, $allow_negative);

    return $value;
}


###################
# General Helpers #
###################

sub _validate_time {
    my ($time, $field, $allow_negative) = @_;

    # regexp verifies one or more digits, optionally followed by a period and
    # zero or more digits, OR we have a period followed by one or more digits
    # (allow negatives, though, so people can back out errors in time reporting)
    if ($time !~ /^-?(?:\d+(?:\.\d*)?|\.\d+)$/) {
        ThrowUserError("number_not_numeric",
                       {field => $field, num => "$time"});
    }

    # Callers can optionally allow negative times
    if ( ($time < 0) && !$allow_negative ) {
        ThrowUserError("number_too_small",
                       {field => $field, num => "$time", min_num => "0"});
    }

    if ($time > 99999.99) {
        ThrowUserError("number_too_large",
                       {field => $field, num => "$time", max_num => "99999.99"});
    }
}

# Sorts fields according to VALIDATOR_DEPENDENCIES. This is not a
# traditional topological sort, because a "dependency" does not
# *have* to be in the list--it just has to be earlier than its dependent
# if it *is* in the list.
sub _sort_by_dep {
    my ($invocant, @fields) = @_;

    my $dependencies = $invocant->VALIDATOR_DEPENDENCIES;
    my ($has_deps, $no_deps) = part { $dependencies->{$_} ? 0 : 1 } @fields;

    # For fields with no dependencies, we sort them alphabetically,
    # so that validation always happens in a consistent order.
    # Fields with no dependencies come at the start of the list.
    my @result = sort @{ $no_deps || [] };

    # Fields with dependencies all go at the end of the list, and if
    # they have dependencies on *each other*, then they have to be
    # sorted properly. We go through $has_deps in sorted order to be
    # sure that fields always validate in a consistent order.
    foreach my $field (sort @{ $has_deps || [] }) {
        if (!grep { $_ eq $field } @result) {
            _insert_dep_field($field, $has_deps, $dependencies, \@result);
        }
    }
    return @result;
}

sub _insert_dep_field {
    my ($field, $insert_me, $dependencies, $result, $loop_tracking) = @_;

    if ($loop_tracking->{$field}) {
        ThrowCodeError('object_dep_sort_loop', 
                       { field => $field, 
                         considered => [keys %$loop_tracking] });
    }
    $loop_tracking->{$field} = 1;

    my $required_fields = $dependencies->{$field};
    # Imagine Field A requires field B...
    foreach my $required_field (@$required_fields) {
        # If our dependency is already satisfied, we're good.
        next if grep { $_ eq $required_field } @$result;

        # If our dependency is not in the remaining fields to insert,
        # then we're also OK.
        next if !grep { $_ eq $required_field } @$insert_me;

        # So, at this point, we know that Field B is in $insert_me.
        # So let's put the required field into the result.
        _insert_dep_field($required_field, $insert_me, $dependencies,
                          $result, $loop_tracking);
    }
    push(@$result, $field);
}

####################
# Constant Helpers #
####################

# For some classes, some constants take time to generate, so we cache them
# and only access them through the below methods. This also allows certain
# hooks to only run once per request instead of multiple times on each
# page.

sub _get_db_columns {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $cache = Bugzilla->request_cache;
    my $cache_key = "object_${class}_db_columns";
    return @{ $cache->{$cache_key} } if $cache->{$cache_key};
    # Currently you can only add new columns using object_columns, not
    # remove or modify existing columns, because removing columns would
    # almost certainly cause Bugzilla to function improperly.
    my @add_columns;
    Bugzilla::Hook::process('object_columns',
                            { class => $class, columns => \@add_columns });
    my @columns = ($invocant->DB_COLUMNS, @add_columns);
    $cache->{$cache_key} = \@columns;
    return @{ $cache->{$cache_key} };
}

# This method is private and should only be called by Bugzilla::Object.
sub _get_validators {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $cache = Bugzilla->request_cache;
    my $cache_key = "object_${class}_validators";
    return $cache->{$cache_key} if $cache->{$cache_key};
    # We copy this into a hash so that the hook doesn't modify the constant.
    # (That could be bad in mod_perl.)
    my %validators = %{ $invocant->VALIDATORS };
    Bugzilla::Hook::process('object_validators', 
                            { class => $class, validators => \%validators });
    $cache->{$cache_key} = \%validators;
    return $cache->{$cache_key};
}

# These are all the fields that need to be checked, always, when
# calling create(), because they have no DEFAULT and they are marked
# NOT NULL.
sub _required_create_fields {
    my $class = shift;
    my $dbh = Bugzilla->dbh;
    my $table = $class->DB_TABLE;

    my @columns = $dbh->bz_table_columns($table);
    my @required;
    foreach my $column (@columns) {
        my $def = $dbh->bz_column_info($table, $column);
        if ($def->{NOTNULL} and !defined $def->{DEFAULT}
            # SERIAL fields effectively have a DEFAULT, but they're not
            # listed as having a DEFAULT in DB::Schema.
            and $def->{TYPE} !~ /serial/i) 
        {
            my $field = $class->REQUIRED_FIELD_MAP->{$column} || $column;
            push(@required, $field);
        }
    }
    push(@required, $class->EXTRA_REQUIRED_FIELDS);
    return @required;
}

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

I<Note>: Though normally you will never need to access this constant's data 
directly in your subclass, if you do, you should access it by calling the
C<_get_db_columns> method instead of accessing the constant directly. (The
only exception to this rule is calling C<SUPER::DB_COLUMNS> from within
your own C<DB_COLUMNS> subroutine in a subclass.)

=item C<NAME_FIELD>

The name of the column that should be considered to be the unique
"name" of this object. The 'name' is a B<string> that uniquely identifies
this Object in the database. Defaults to 'name'. When you specify 
C<< {name => $name} >> to C<new()>, this is the column that will be 
matched against in the DB.

=item C<ID_FIELD>

The name of the column that represents the unique B<integer> ID
of this object in the database. Defaults to 'id'.

=item C<LIST_ORDER>

The order that C<new_from_list> and C<get_all> should return objects
in. This should be the name of a database column. Defaults to
L</NAME_FIELD>.

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

=item C<VALIDATOR_DEPENDENCIES>

During L</create> and L</set_all>, validators are normally called in
a somewhat-random order.  If you need one field to be validated and set
before another field, this constant is how you do it, by saying that
one field "depends" on the value of other fields.

This is a hashref, where the keys are field names and the values are
arrayrefs of field names. You specify what fields a field depends on using
the arrayrefs. So, for example, to say that a C<component> field depends
on the C<product> field being set, you would do:

 component => ['product']

=item C<UPDATE_COLUMNS>

A list of columns to update when L</update> is called.
If a field can't be changed, it shouldn't be listed here. (For example,
the L</ID_FIELD> usually can't be updated.)

=item C<REQUIRED_FIELD_MAP>

This is a hashref that maps database column names to L</create> argument
names. You only need to specify values for fields where the argument passed
to L</create> has a different name in the database than it does in the
L</create> arguments. (For example, L<Bugzilla::Bug/create> takes a
C<product> argument, but the column name in the C<bugs> table is
C<product_id>.)

=item C<EXTRA_REQUIRED_FIELDS>

Normally, Bugzilla::Object automatically figures out which fields
are required for L</create>. It then I<always> runs those fields' validators,
even if those fields weren't passed as arguments to L</create>. That way,
any default values or required checks can be done for those fields by
the validators.

L</create> figures out which fields are required by looking for database
columns in the L</DB_TABLE> that are NOT NULL and have no DEFAULT set.
However, there are some fields that this check doesn't work for:

=over

=item *

Fields that have database defaults (or are marked NULL in the database)
but actually have different defaults specified by validators. (For example,
the qa_contact field in the C<bugs> table can be NULL, so it won't be
caught as being required. However, in reality it defaults to the
component's initial_qa_contact.)

=item * 

Fields that have defaults that should be set by validators, but are
actually stored in a table different from L</DB_TABLE> (like the "cc"
field for bugs, which defaults to the "initialcc" of the Component, but won't
be caught as a normal required field because it's in a separate table.) 

=back

Any field matching the above criteria needs to have its name listed in
this constant. For an example of use, see the code of L<Bugzilla::Bug>.

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
(from L</NAME_FIELD>) in the DB. You can also pass in an C<id> key
which will be interpreted as the id of the object you want (overriding the 
C<name> key).

B<Additional Parameters Available for Subclasses>

If you are a subclass of C<Bugzilla::Object>, you can pass
C<condition> and C<values> as hash keys, instead of the above.

C<condition> is a set of SQL conditions for the WHERE clause, which contain
placeholders.

C<values> is a reference to an array. The array contains the values
for each placeholder in C<condition>, in order.

This is to allow subclasses to have complex parameters, and then to
translate those parameters into C<condition> and C<values> when they
call C<< $self->SUPER::new >> (which is this function, usually).

If you try to call C<new> outside of a subclass with the C<condition>
and C<values> parameters, Bugzilla will throw an error. These parameters
are intended B<only> for use by subclasses.

=item B<Returns>

A fully-initialized object, or C<undef> if there is no object in the
database matching the parameters you passed in.

=back

=item C<initialize>

=over

=item B<Description>

Abstract method to allow subclasses to perform initialization tasks after an
object has been created.

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

=item C<new_from_hash($hashref)>

  Description: Create an object from the given hash.

  Params:      $hashref - A reference to a hash which was created by
                          flatten_to_hash.

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

In addition to the column keys, there are a few special keys that
can be used to rig the underlying database queries. These are 
C<LIMIT>, C<OFFSET>, and C<WHERE>.

The value for the C<LIMIT> key is expected to be an integer defining 
the number of objects to return, while the value for C<OFFSET> defines
the position, relative to the number of objects the query would normally 
return, at which to begin the result set. If C<OFFSET> is defined without 
a corresponding C<LIMIT> it is silently ignored.

The C<WHERE> key provides a mechanism for adding arbitrary WHERE
clauses to the underlying query. Its value is expected to a hash 
reference whose keys are the columns, operators and placeholders, and the 
values are the placeholders' bind value. For example:

 WHERE => { 'some_column >= ?' => $some_value }

would constrain the query to only those objects in the table whose
'some_column' column has a value greater than or equal to $some_value.

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
             field for this object.

Returns:     The Object just created in the database.

Notes:       In order for this function to work in your subclass,
             your subclass's L</ID_FIELD> must be of C<SERIAL>
             type in the database.

Subclass Implementors:
             This function basically just calls 
             L</check_required_create_fields>, then
             L</run_create_validators>, and then finally
             L</insert_create_data>. So if you have a complex system that
             you need to implement, you can do it by calling these
             three functions instead of C<SUPER::create>.

=item C<check_required_create_fields>

=over

=item B<Description>

Part of L</create>. Modifies the incoming C<$params> argument so that
any field that does not have a database default will be checked
later by L</run_create_validators>, even if that field wasn't specified
as an argument to L</create>.

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

Params:      C<$params> - hashref - A value to put in each database
             field for this object.
             C<$options> - hashref - Processing options. Currently
             the only option supported is B<skip>, which can be
             used to specify a list of fields to not validate.

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

B<In scalar context:>

A hashref showing what changed during the update. The keys are the column
names from L</UPDATE_COLUMNS>. If a field was not changed, it will not be
in the hash at all. If the field was changed, the key will point to an arrayref.
The first item of the arrayref will be the old value, and the second item
will be the new value.

If there were no changes, we return a reference to an empty hash.

B<In array context:>

Returns a list, where the first item is the above hashref. The second item
is the object as it was in the database before update() was called. (This
is mostly useful to subclasses of C<Bugzilla::Object> that are implementing
C<update>.)

=back

=item C<remove_from_db>

Removes this object from the database. Will throw an error if you can't
remove it for some reason. The object will then be destroyed, as it is
not safe to use the object after it has been removed from the database.

=back

=head2 Mutators

These are used for updating the values in objects, before calling
C<update>.

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

B<NOTE>: This function is intended only for use by subclasses. If
you call it from anywhere else, it will throw a C<CodeError>.

=item B<Params>

=over

=item C<$field> - The name of the hash member to update. This should
be the same as the name of the field in L</VALIDATORS>, if it exists there.

=item C<$value> - The value that you're setting the field to.

=back

=item B<Returns> (nothing)

=back


=item C<set_all>

=over

=item B<Description>

This is a convenience function which is simpler than calling many different
C<set_> functions in a row. You pass a hashref of parameters and it calls
C<set_$key($value)> for every item in the hashref.

=item B<Params>

Takes a hashref of the fields that need to be set, pointing to the value
that should be passed to the C<set_> function that is called.

=item B<Returns> (nothing)

=back


=back

=head2 Simple Methods

=over

=item C<flatten_to_hash>

Returns a hashref suitable for serialisation and re-inflation with C<new_from_hash>.

=back


=head2 Simple Validators

You can use these in your subclass L</VALIDATORS> or L</UPDATE_VALIDATORS>.
Note that you have to reference them like C<\&Bugzilla::Object::check_boolean>,
you can't just write C<\&check_boolean>.

=over

=item C<check_boolean>

Returns C<1> if the passed-in value is true, C<0> otherwise.

=back

=head2 CACHE FUNCTIONS

=over

=item C<object_cache_get>

=over

=item B<Description>

Class function which returns an object from the object-cache for the provided
C<$id>.

=item B<Params>

Takes an integer C<$id> of the object to retrieve.

=item B<Returns>

Returns the object from the cache if found, otherwise returns C<undef>.

=item B<Example>

my $bug_from_cache = Bugzilla::Bug->object_cache_get(35);

=back

=item C<object_cache_set>

=over

=item B<Description>

Object function which injects the object into the object-cache, using the
object's C<id> as the key.

=item B<Params>

(none)

=item B<Returns>

(nothing)

=item B<Example>

$bug->object_cache_set();

=back

=back

=head1 CLASS FUNCTIONS

=over

=item C<any_exist>

Returns C<1> if there are any of these objects in the database,
C<0> otherwise.

=item C<get_all>

 Description: Returns all objects in this table from the database.

 Params:      none.

 Returns:     A list of objects, or an empty list if there are none.

 Notes:       Note that you must call this as $class->get_all. For 
              example, Bugzilla::Keyword->get_all. 
              Bugzilla::Keyword::get_all will not work.

=back

=cut

=head1 B<Methods in need of POD>

=over

=item object_cache_key

=item check_time

=item id

=item TO_JSON

=item audit_log

=back
