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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Mads Bondo Dydensborg <mbd@dbc.dk>
#                 Byron Jones <glob@mozilla.com>

package Bugzilla::WebService::Product;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(validate filter filter_wants);

use constant READ_ONLY => qw(
    get
    get_accessible_products
    get_enterable_products
    get_selectable_products
);

use constant FIELD_MAP => {
    has_unconfirmed => 'allows_unconfirmed',
    is_open         => 'isactive',
};

##################################################
# Add aliases here for method name compatibility #
##################################################

BEGIN { *get_products = \&get }

# Get the ids of the products the user can search
sub get_selectable_products {
    return {ids => [map {$_->id} @{Bugzilla->user->get_selectable_products}]}; 
}

# Get the ids of the products the user can enter bugs against
sub get_enterable_products {
    return {ids => [map {$_->id} @{Bugzilla->user->get_enterable_products}]}; 
}

# Get the union of the products the user can search and enter bugs against.
sub get_accessible_products {
    return {ids => [map {$_->id} @{Bugzilla->user->get_accessible_products}]}; 
}

# Get a list of actual products, based on list of ids or names
sub get {
    my ($self, $params) = validate(@_, 'ids', 'names');
    
    # Only products that are in the users accessible products, 
    # can be allowed to be returned
    my $accessible_products = Bugzilla->user->get_accessible_products;

    my @requested_accessible;

    if (defined $params->{ids}) {
        # Create a hash with the ids the user wants
        my %ids = map { $_ => 1 } @{$params->{ids}};
        
        # Return the intersection of this, by grepping the ids from 
        # accessible products.
        push(@requested_accessible,
            grep { $ids{$_->id} } @$accessible_products);
    }

    if (defined $params->{names}) {
        # Create a hash with the names the user wants
        my %names = map { lc($_) => 1 } @{$params->{names}};
        
        # Return the intersection of this, by grepping the names from 
        # accessible products, union'ed with products found by ID to
        # avoid duplicates
        foreach my $product (grep { $names{lc $_->name} }
                                  @$accessible_products) {
            next if grep { $_->id == $product->id }
                         @requested_accessible;
            push @requested_accessible, $product;
        }
    }

    # Now create a result entry for each.
    my @products = map { $self->_product_to_hash($params, $_) }
                       @requested_accessible;
    return { products => \@products };
}

sub create {
    my ($self, $params) = @_;

    Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->user->in_group('editcomponents') 
        || ThrowUserError("auth_failure", { group  => "editcomponents",
                                            action => "add",
                                            object => "products"});
    # Create product
    my $args = {
        name             => $params->{name},
        description      => $params->{description},
        version          => $params->{version},
        defaultmilestone => $params->{default_milestone},
        # create_series has no default value.
        create_series    => defined $params->{create_series} ?
                              $params->{create_series} : 1
    };
    foreach my $field (qw(has_unconfirmed is_open classification)) {
        if (defined $params->{$field}) {
            my $name = FIELD_MAP->{$field} || $field;
            $args->{$name} = $params->{$field};
        }
    }
    my $product = Bugzilla::Product->create($args);
    return { id => $self->type('int', $product->id) };
}

sub _product_to_hash {
    my ($self, $params, $product) = @_;

    my $field_data = {
        id          => $self->type('int', $product->id),
        name        => $self->type('string', $product->name),
        description => $self->type('string', $product->description),
        is_active   => $self->type('boolean', $product->is_active),
        default_milestone => $self->type('string', $product->default_milestone),
        has_unconfirmed   => $self->type('boolean', $product->allows_unconfirmed),
        classification => $self->type('string', $product->classification->name),
    };
    if (filter_wants($params, 'components')) {
        $field_data->{components} = [map {
            $self->_component_to_hash($_)
        } @{$product->components}];
    }
    if (filter_wants($params, 'versions')) {
        $field_data->{versions} = [map {
            $self->_version_to_hash($_)
        } @{$product->versions}];
    }
    if (filter_wants($params, 'milestones')) {
        $field_data->{milestones} = [map {
            $self->_milestone_to_hash($_)
        } @{$product->milestones}];
    }
    return filter($params, $field_data);
}

sub _component_to_hash {
    my ($self, $component) = @_;
    return {
        id =>
            $self->type('int', $component->id),
        name =>
            $self->type('string', $component->name),
        description =>
            $self->type('string' , $component->description),
        default_assigned_to =>
            $self->type('string' , $component->default_assignee->login),
        default_qa_contact =>
            $self->type('string' , $component->default_qa_contact->login),
        sort_key =>  # sort_key is returned to match Bug.fields
            0,
        is_active =>
            $self->type('boolean', $component->is_active),
    };
}

sub _version_to_hash {
    my ($self, $version) = @_;
    return {
        id =>
            $self->type('int', $version->id),
        name =>
            $self->type('string', $version->name),
        sort_key =>  # sort_key is returened to match Bug.fields
            0,
        is_active =>
            $self->type('boolean', $version->is_active),
    };
}

sub _milestone_to_hash {
    my ($self, $milestone) = @_;
    return {
        id =>
            $self->type('int', $milestone->id),
        name =>
            $self->type('string', $milestone->name),
        sort_key =>
            $self->type('int', $milestone->sortkey),
        is_active =>
            $self->type('boolean', $milestone->is_active),
    };
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Product - The Product API

=head1 DESCRIPTION

This part of the Bugzilla API allows you to list the available Products and
get information about them.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

=head1 List Products

=head2 get_selectable_products

B<EXPERIMENTAL>

=over

=item B<Description>

Returns a list of the ids of the products the user can search on.

=item B<Params> (none)

=item B<Returns>    

A hash containing one item, C<ids>, that contains an array of product
ids.

=item B<Errors> (none)

=back

=head2 get_enterable_products

B<EXPERIMENTAL>

=over

=item B<Description>

Returns a list of the ids of the products the user can enter bugs
against.

=item B<Params> (none)

=item B<Returns>

A hash containing one item, C<ids>, that contains an array of product
ids.

=item B<Errors> (none)

=back

=head2 get_accessible_products

B<UNSTABLE>

=over

=item B<Description>

Returns a list of the ids of the products the user can search or enter
bugs against.

=item B<Params> (none)

=item B<Returns>

A hash containing one item, C<ids>, that contains an array of product
ids.

=item B<Errors> (none)

=back

=head2 get

B<EXPERIMENTAL>

=over

=item B<Description>

Returns a list of information about the products passed to it.

Note: Can also be called as "get_products" for compatibilty with Bugzilla 3.0 API.

=item B<Params>

In addition to the parameters below, this method also accepts the
standard L<include_fields|Bugzilla::WebService/include_fields> and
L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

=over

=item C<ids>

An array of product ids

=item C<names>

An array of product names

=back

=item B<Returns> 

A hash containing one item, C<products>, that is an array of
hashes. Each hash describes a product, and has the following items:

=over

=item C<id>

C<int> An integer id uniquely identifying the product in this installation only.

=item C<name>

C<string> The name of the product.  This is a unique identifier for the
product.

=item C<description>

C<string> A description of the product, which may contain HTML.

=item C<is_active>

C<boolean> A boolean indicating if the product is active.

=item C<default_milestone>

C<string> The name of the default milestone for the product.

=item C<has_unconfirmed>

C<boolean> Indicates whether the UNCONFIRMED bug status is available
for this product.

=item C<classification>

C<string> The classification name for the product.

=item C<components>

C<array> An array of hashes, where each hash describes a component, and has the
following items:

=over

=item C<id>

C<int> An integer id uniquely identifying the component in this installation
only.

=item C<name>

C<string> The name of the component.  This is a unique identifier for this
component.

=item C<description>

C<string> A description of the component, which may contain HTML.

=item C<default_assigned_to>

C<string> The login name of the user to whom new bugs will be assigned by
default.

=item C<default_qa_contact>

C<string> The login name of the user who will be set as the QA Contact for
new bugs by default.

=item C<sort_key>

C<int> Components, when displayed in a list, are sorted first by this integer
and then secondly by their name.

=item C<is_active>

C<boolean> A boolean indicating if the component is active.  Inactive
components are not enabled for new bugs.

=back

=item C<versions>

C<array> An array of hashes, where each hash describes a version, and has the
following items: C<name>, C<sort_key> and C<is_active>.

=item C<milestones>

C<array> An array of hashes, where each hash describes a milestone, and has the
following items: C<name>, C<sort_key> and C<is_active>.

=back

Note, that if the user tries to access a product that is not in the
list of accessible products for the user, or a product that does not
exist, that is silently ignored, and no information about that product
is returned.

=item B<Errors> (none)

=item B<History>

=over

=item In Bugzilla B<4.2>, C<names> was added as an input parameter.

=item In Bugzilla B<4.2>, C<classification>, C<components>, C<versions>,
C<milestones>, C<default_milestone> and C<has_unconfirmed> were added to
the fields returned by C<get> as a replacement for C<internals>, which has
been removed.

=back

=back

=head1 Product Creation

=head2 create

B<EXPERIMENTAL>

=over

=item B<Description>

This allows you to create a new product in Bugzilla.

=item B<Params> 

Some params must be set, or an error will be thrown. These params are 
marked B<Required>.

=over

=item C<name>

B<Required> C<string> The name of this product. Must be globally unique
within Bugzilla.

=item C<description>

B<Required> C<string> A description for this product. Allows some simple HTML.

=item C<version> 

B<Required> C<string> The default version for this product.

=item C<has_unconfirmed> 

C<boolean> Allow the UNCONFIRMED status to be set on bugs in this product.
Default: true.

=item C<classification>

C<string> The name of the Classification which contains this product.

=item C<default_milestone> 

C<string> The default milestone for this product. Default: '---'.

=item C<is_open> 

C<boolean> True if the product is currently allowing bugs to be entered
into it. Default: true.

=item C<create_series>

C<boolean> True if you want series for New Charts to be created for this
new product. Default: true.

=back

=item B<Returns>    

A hash with one element, id. This is the id of the newly-filed product.

=item B<Errors>

=over

=item 51 (Classification does not exist)

You must specify an existing classification name.

=item 700 (Product blank name)

You must specify a non-blank name for this product.

=item 701 (Product name too long)

The name specified for this product was longer than the maximum
allowed length.

=item 702 (Product name already exists)

You specified the name of a product that already exists.
(Product names must be globally unique in Bugzilla.)

=item 703 (Product must have description)

You must specify a description for this product.

=item 704 (Product must have version)

You must specify a version for this product.

=back

=back
