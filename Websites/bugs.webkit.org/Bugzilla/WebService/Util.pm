# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Util;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::Error;

use Storable qw(dclone);
use List::MoreUtils qw(any none);

use parent qw(Exporter);

# We have to "require", not "use" this, because otherwise it tries to
# use features of Test::More during import().
require Test::Taint;

our @EXPORT_OK = qw(
    extract_flags
    filter
    filter_wants
    taint_data
    validate
    translate
    params_to_objects
    fix_credentials
);

sub extract_flags {
    my ($flags, $bug, $attachment) = @_;
    my (@new_flags, @old_flags);

    my $flag_types    = $attachment ? $attachment->flag_types : $bug->flag_types;
    my $current_flags = $attachment ? $attachment->flags : $bug->flags;

    # Copy the user provided $flags as we may call extract_flags more than
    # once when editing multiple bugs or attachments.
    my $flags_copy = dclone($flags);

    foreach my $flag (@$flags_copy) {
        my $id      = $flag->{id};
        my $type_id = $flag->{type_id};

        my $new  = delete $flag->{new};
        my $name = delete $flag->{name};

        if ($id) {
            my $flag_obj = grep($id == $_->id, @$current_flags);
            $flag_obj || ThrowUserError('object_does_not_exist',
                                        { class => 'Bugzilla::Flag', id => $id });
        }
        elsif ($type_id) {
            my $type_obj = grep($type_id == $_->id, @$flag_types);
            $type_obj || ThrowUserError('object_does_not_exist',
                                        { class => 'Bugzilla::FlagType', id => $type_id });
            if (!$new) {
                my @flag_matches = grep($type_id == $_->type->id, @$current_flags);
                @flag_matches > 1 && ThrowUserError('flag_not_unique',
                                                     { value => $type_id });
                if (!@flag_matches) {
                    delete $flag->{id};
                }
                else {
                    delete $flag->{type_id};
                    $flag->{id} = $flag_matches[0]->id;
                }
            }
        }
        elsif ($name) {
            my @type_matches = grep($name eq $_->name, @$flag_types);
            @type_matches > 1 && ThrowUserError('flag_type_not_unique',
                                                { value => $name });
            @type_matches || ThrowUserError('object_does_not_exist',
                                            { class => 'Bugzilla::FlagType', name => $name });
            if ($new) {
                delete $flag->{id};
                $flag->{type_id} = $type_matches[0]->id;
            }
            else {
                my @flag_matches = grep($name eq $_->type->name, @$current_flags);
                @flag_matches > 1 && ThrowUserError('flag_not_unique', { value => $name });
                if (@flag_matches) {
                    $flag->{id} = $flag_matches[0]->id;
                }
                else {
                    delete $flag->{id};
                    $flag->{type_id} = $type_matches[0]->id;
                }
            }
        }

        if ($flag->{id}) {
            push(@old_flags, $flag);
        }
        else {
            push(@new_flags, $flag);
        }
    }

    return (\@old_flags, \@new_flags);
}

sub filter($$;$$) {
    my ($params, $hash, $types, $prefix) = @_;
    my %newhash = %$hash;

    foreach my $key (keys %$hash) {
        delete $newhash{$key} if !filter_wants($params, $key, $types, $prefix);
    }

    return \%newhash;
}

sub filter_wants($$;$$) {
    my ($params, $field, $types, $prefix) = @_;

    # Since this is operation is resource intensive, we will cache the results
    # This assumes that $params->{*_fields} doesn't change between calls
    my $cache = Bugzilla->request_cache->{filter_wants} ||= {};
    $field = "${prefix}.${field}" if $prefix;

    if (exists $cache->{$field}) {
        return $cache->{$field};
    }

    # Mimic old behavior if no types provided
    my %field_types = map { $_ => 1 } (ref $types ? @$types : ($types || 'default'));

    my %include = map { $_ => 1 } @{ $params->{'include_fields'} || [] };
    my %exclude = map { $_ => 1 } @{ $params->{'exclude_fields'} || [] };

    my %include_types;
    my %exclude_types;

    # Only return default fields if nothing is specified
    $include_types{default} = 1 if !%include;

    # Look for any field types requested
    foreach my $key (keys %include) {
        next if $key !~ /^_(.*)$/;
        $include_types{$1} = 1;
        delete $include{$key};
    }
    foreach my $key (keys %exclude) {
        next if $key !~ /^_(.*)$/;
        $exclude_types{$1} = 1;
        delete $exclude{$key};
    }

    # Explicit inclusion/exclusion
    return $cache->{$field} = 0 if $exclude{$field};
    return $cache->{$field} = 1 if $include{$field};

    # If the user has asked to include all or exclude all
    return $cache->{$field} = 0 if $exclude_types{'all'};
    return $cache->{$field} = 1 if $include_types{'all'};

    # If the user has not asked for any fields specifically or if the user has asked
    # for one or more of the field's types (and not excluded them)
    foreach my $type (keys %field_types) {
        return $cache->{$field} = 0 if $exclude_types{$type};
        return $cache->{$field} = 1 if $include_types{$type};
    }

    my $wants = 0;
    if ($prefix) {
        # Include the field if the parent is include (and this one is not excluded)
        $wants = 1 if $include{$prefix};
    }
    else {
        # We want to include this if one of the sub keys is included
        my $key = $field . '.';
        my $len = length($key);
        $wants = 1 if grep { substr($_, 0, $len) eq $key  } keys %include;
    }

    return $cache->{$field} = $wants;
}

sub taint_data {
    my @params = @_;
    return if !@params;
    # Though this is a private function, it hasn't changed since 2004 and
    # should be safe to use, and prevents us from having to write it ourselves
    # or require another module to do it.
    Test::Taint::_deeply_traverse(\&_delete_bad_keys, \@params);
    Test::Taint::taint_deeply(\@params);
}

sub _delete_bad_keys {
    foreach my $item (@_) {
        next if ref $item ne 'HASH';
        foreach my $key (keys %$item) {
            # Making something a hash key always untaints it, in Perl.
            # However, we need to validate our argument names in some way.
            # We know that all hash keys passed in to the WebService will 
            # match \w+, contain '.' or '-', so we delete any key that
            # doesn't match that.
            if ($key !~ /^[\w\.\-]+$/) {
                delete $item->{$key};
            }
        }
    }
    return @_;
}

sub validate  {
    my ($self, $params, @keys) = @_;

    # If $params is defined but not a reference, then we weren't
    # sent any parameters at all, and we're getting @keys where
    # $params should be.
    return ($self, undef) if (defined $params and !ref $params);

    my @id_params = qw(ids comment_ids);
    # If @keys is not empty then we convert any named 
    # parameters that have scalar values to arrayrefs
    # that match.
    foreach my $key (@keys) {
        if (exists $params->{$key}) {
            $params->{$key} = [ $params->{$key} ] unless ref $params->{$key};

            if (any { $key eq $_ } @id_params) {
                my $ids = $params->{$key};
                ThrowCodeError('param_scalar_array_required', { param => $key })
                  unless ref($ids) eq 'ARRAY' && none { ref $_ } @$ids;
            }
        }
    }

    return ($self, $params);
}

sub translate {
    my ($params, $mapped) = @_;
    my %changes;
    while (my ($key,$value) = each (%$params)) {
        my $new_field = $mapped->{$key} || $key;
        $changes{$new_field} = $value;
    }
    return \%changes;
}

sub params_to_objects {
    my ($params, $class) = @_;
    my (@objects, @objects_by_ids);

    @objects = map { $class->check($_) } 
        @{ $params->{names} } if $params->{names};

    @objects_by_ids = map { $class->check({ id => $_ }) } 
        @{ $params->{ids} } if $params->{ids};

    push(@objects, @objects_by_ids);
    my %seen;
    @objects = grep { !$seen{$_->id}++ } @objects;
    return \@objects;
}

sub fix_credentials {
    my ($params) = @_;
    # Allow user to pass in login=foo&password=bar as a convenience
    # even if not calling GET /login. We also do not delete them as
    # GET /login requires "login" and "password".
    if (exists $params->{'login'} && exists $params->{'password'}) {
        $params->{'Bugzilla_login'}    = delete $params->{'login'};
        $params->{'Bugzilla_password'} = delete $params->{'password'};
    }
    # Allow user to pass api_key=12345678 as a convenience which becomes
    # "Bugzilla_api_key" which is what the auth code looks for.
    if (exists $params->{api_key}) {
        $params->{Bugzilla_api_key} = delete $params->{api_key};
    }
    # Allow user to pass token=12345678 as a convenience which becomes
    # "Bugzilla_token" which is what the auth code looks for.
    if (exists $params->{'token'}) {
        $params->{'Bugzilla_token'} = delete $params->{'token'};
    }

    # Allow extensions to modify the credential data before login
    Bugzilla::Hook::process('webservice_fix_credentials', { params => $params });
}

__END__

=head1 NAME

Bugzilla::WebService::Util - Utility functions used inside of the WebService
code. These are B<not> functions that can be called via the WebService.

=head1 DESCRIPTION

This is somewhat like L<Bugzilla::Util>, but these functions are only used
internally in the WebService code.

=head1 SYNOPSIS

 filter({ include_fields => ['id', 'name'], 
          exclude_fields => ['name'] }, $hash);
 my $wants = filter_wants $params, 'field_name';
 validate(@_, 'ids');

=head1 METHODS

=head2 filter

This helps implement the C<include_fields> and C<exclude_fields> arguments
of WebService methods. Given a hash (the second argument to this subroutine),
this will remove any keys that are I<not> in C<include_fields> and then remove
any keys that I<are> in C<exclude_fields>.

An optional third option can be passed that prefixes the field name to allow
filtering of data two or more levels deep.

For example, if you want to filter out the C<id> key/value in components returned
by Product.get, you would use the value C<component.id> in your C<exclude_fields>
list.

=head2 filter_wants

Returns C<1> if a filter would preserve the specified field when passing
a hash to L</filter>, C<0> otherwise.

=head2 validate

This helps in the validation of parameters passed into the WebService
methods. Currently it converts listed parameters into an array reference
if the client only passed a single scalar value. It modifies the parameters
hash in place so other parameters should be unaltered.

=head2 translate

WebService methods frequently take parameters with different names than
the ones that we use internally in Bugzilla. This function takes a hashref
that has field names for keys and returns a hashref with those keys renamed
according to the mapping passed in with the second parameter (which is also
a hashref).

=head2 params_to_objects

Creates objects of the type passed in as the second parameter, using the
parameters passed to a WebService method (the first parameter to this function).
Helps make life simpler for WebService methods that internally create objects
via both "ids" and "names" fields. Also de-duplicates objects that were loaded
by both "ids" and "names". Returns an arrayref of objects.

=head2 fix_credentials

Allows for certain parameters related to authentication such as Bugzilla_login,
Bugzilla_password, and Bugzilla_token to have shorter named equivalents passed in.
This function converts the shorter versions to their respective internal names.

=head2 extract_flags

Subroutine that takes a list of hashes that are potential flag changes for
both bugs and attachments. Then breaks the list down into two separate lists
based on if the change is to add a new flag or to update an existing flag.

=head1 B<Methods in need of POD>

=over

=item taint_data

=back
