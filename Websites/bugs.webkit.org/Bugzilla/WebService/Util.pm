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
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): 
#   Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::WebService::Util;
use strict;
use base qw(Exporter);

# We have to "require", not "use" this, because otherwise it tries to
# use features of Test::More during import().
require Test::Taint;

our @EXPORT_OK = qw(
    filter
    filter_wants
    taint_data
    validate
);

sub filter ($$) {
    my ($params, $hash) = @_;
    my %newhash = %$hash;

    foreach my $key (keys %$hash) {
        delete $newhash{$key} if !filter_wants($params, $key);
    }

    return \%newhash;
}

sub filter_wants ($$) {
    my ($params, $field) = @_;
    my %include = map { $_ => 1 } @{ $params->{'include_fields'} || [] };
    my %exclude = map { $_ => 1 } @{ $params->{'exclude_fields'} || [] };

    if (defined $params->{include_fields}) {
        return 0 if !$include{$field};
    }
    if (defined $params->{exclude_fields}) {
        return 0 if $exclude{$field};
    }

    return 1;
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
            # match \w+, so we delete any key that doesn't match that.
            if ($key !~ /^\w+$/) {
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
    
    # If @keys is not empty then we convert any named 
    # parameters that have scalar values to arrayrefs
    # that match.
    foreach my $key (@keys) {
        if (exists $params->{$key}) {
            $params->{$key} = ref $params->{$key} 
                              ? $params->{$key} 
                              : [ $params->{$key} ];
        }
    }

    return ($self, $params);
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

=head2 filter_wants

Returns C<1> if a filter would preserve the specified field when passing
a hash to L</filter>, C<0> otherwise.

=head2 validate

This helps in the validation of parameters passed into the WebService
methods. Currently it converts listed parameters into an array reference
if the client only passed a single scalar value. It modifies the parameters
hash in place so other parameters should be unaltered.
