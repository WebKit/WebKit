# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Product;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService);
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(validate filter filter_wants translate params_to_objects);

use constant READ_ONLY => qw(
    get
    get_accessible_products
    get_enterable_products
    get_selectable_products
);

use constant PUBLIC_METHODS => qw(
    create
    get
    get_accessible_products
    get_enterable_products
    get_selectable_products
    update
);

use constant MAPPED_FIELDS => {
    has_unconfirmed => 'allows_unconfirmed',
    is_open => 'is_active',
};

use constant MAPPED_RETURNS => {
    allows_unconfirmed => 'has_unconfirmed',
    defaultmilestone => 'default_milestone',
    isactive => 'is_open',
};

use constant FIELD_MAP => {
    has_unconfirmed => 'allows_unconfirmed',
    is_open         => 'isactive',
};

##################################################
# Add aliases here for method name compatibility #
##################################################

# Get the ids of the products the user can search
sub get_selectable_products {
    Bugzilla->switch_to_shadow_db();
    return {ids => [map {$_->id} @{Bugzilla->user->get_selectable_products}]};
}

# Get the ids of the products the user can enter bugs against
sub get_enterable_products {
    Bugzilla->switch_to_shadow_db();
    return {ids => [map {$_->id} @{Bugzilla->user->get_enterable_products}]};
}

# Get the union of the products the user can search and enter bugs against.
sub get_accessible_products {
    Bugzilla->switch_to_shadow_db();
    return {ids => [map {$_->id} @{Bugzilla->user->get_accessible_products}]};
}

# Get a list of actual products, based on list of ids or names
sub get {
    my ($self, $params) = validate(@_, 'ids', 'names', 'type');
    my $user = Bugzilla->user;

    defined $params->{ids} || defined $params->{names} || defined $params->{type}
        || ThrowCodeError("params_required", { function => "Product.get",
                                               params => ['ids', 'names', 'type'] });
    Bugzilla->switch_to_shadow_db();

    my $products = [];
    if (defined $params->{type}) {
        my %product_hash;
        foreach my $type (@{ $params->{type} }) {
            my $result = [];
            if ($type eq 'accessible') {
                $result = $user->get_accessible_products();
            }
            elsif ($type eq 'enterable') {
                $result = $user->get_enterable_products();
            }
            elsif ($type eq 'selectable') {
                $result = $user->get_selectable_products();
            }
            else {
                ThrowUserError('get_products_invalid_type',
                               { type => $type });
            }
            map { $product_hash{$_->id} = $_ } @$result;
        }
        $products = [ values %product_hash ];
    }
    else {
        $products = $user->get_accessible_products;
    }

    my @requested_products;

    if (defined $params->{ids}) {
        # Create a hash with the ids the user wants
        my %ids = map { $_ => 1 } @{$params->{ids}};

        # Return the intersection of this, by grepping the ids from $products.
        push(@requested_products,
            grep { $ids{$_->id} } @$products);
    }

    if (defined $params->{names}) {
        # Create a hash with the names the user wants
        my %names = map { lc($_) => 1 } @{$params->{names}};

        # Return the intersection of this, by grepping the names
        # from $products, union'ed with products found by ID to
        # avoid duplicates
        foreach my $product (grep { $names{lc $_->name} }
                                  @$products) {
            next if grep { $_->id == $product->id }
                         @requested_products;
            push @requested_products, $product;
        }
    }

    # If we just requested a specific type of products without
    # specifying ids or names, then return the entire list.
    if (!defined $params->{ids} && !defined $params->{names}) {
        @requested_products = @$products;
    }

    # Now create a result entry for each.
    my @products = map { $self->_product_to_hash($params, $_) }
                       @requested_products;
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

sub update {
    my ($self, $params) = @_;

    my $dbh = Bugzilla->dbh;

    Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->user->in_group('editcomponents')
        || ThrowUserError("auth_failure", { group  => "editcomponents",
                                            action => "edit",
                                            object => "products" });

    defined($params->{names}) || defined($params->{ids})
        || ThrowCodeError('params_required',
               { function => 'Product.update', params => ['ids', 'names'] });

    my $product_objects = params_to_objects($params, 'Bugzilla::Product');

    my $values = translate($params, MAPPED_FIELDS);

    # We delete names and ids to keep only new values to set.
    delete $values->{names};
    delete $values->{ids};

    $dbh->bz_start_transaction();
    foreach my $product (@$product_objects) {
        $product->set_all($values);
    }

    my %changes;
    foreach my $product (@$product_objects) {
        my $returned_changes = $product->update();
        $changes{$product->id} = translate($returned_changes, MAPPED_RETURNS);
    }
    $dbh->bz_commit_transaction();

    my @result;
    foreach my $product (@$product_objects) {
        my %hash = (
            id      => $product->id,
            changes => {},
        );

        foreach my $field (keys %{ $changes{$product->id} }) {
            my $change = $changes{$product->id}->{$field};
            $hash{changes}{$field} = {
                removed => $self->type('string', $change->[0]),
                added   => $self->type('string', $change->[1])
            };
        }

        push(@result, \%hash);
    }

    return { products => \@result };
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
            $self->_component_to_hash($_, $params)
        } @{$product->components}];
    }
    if (filter_wants($params, 'versions')) {
        $field_data->{versions} = [map {
            $self->_version_to_hash($_, $params)
        } @{$product->versions}];
    }
    if (filter_wants($params, 'milestones')) {
        $field_data->{milestones} = [map {
            $self->_milestone_to_hash($_, $params)
        } @{$product->milestones}];
    }
    return filter($params, $field_data);
}

sub _component_to_hash {
    my ($self, $component, $params) = @_;
    my $field_data = filter $params, {
        id =>
            $self->type('int', $component->id),
        name =>
            $self->type('string', $component->name),
        description =>
            $self->type('string' , $component->description),
        default_assigned_to =>
            $self->type('email', $component->default_assignee->login),
        default_qa_contact =>
            $self->type('email', $component->default_qa_contact ?
                                 $component->default_qa_contact->login : ""),
        sort_key =>  # sort_key is returned to match Bug.fields
            0,
        is_active =>
            $self->type('boolean', $component->is_active),
    }, undef, 'components';

    if (filter_wants($params, 'flag_types', undef, 'components')) {
        $field_data->{flag_types} = {
            bug =>
                [map {
                    $self->_flag_type_to_hash($_)
                } @{$component->flag_types->{'bug'}}],
            attachment =>
                [map {
                    $self->_flag_type_to_hash($_)
                } @{$component->flag_types->{'attachment'}}],
        };
    }

    return $field_data;
}

sub _flag_type_to_hash {
    my ($self, $flag_type, $params) = @_;
    return filter $params, {
        id =>
            $self->type('int', $flag_type->id),
        name =>
            $self->type('string', $flag_type->name),
        description =>
            $self->type('string', $flag_type->description),
        cc_list =>
            $self->type('string', $flag_type->cc_list),
        sort_key =>
            $self->type('int', $flag_type->sortkey),
        is_active =>
            $self->type('boolean', $flag_type->is_active),
        is_requestable =>
            $self->type('boolean', $flag_type->is_requestable),
        is_requesteeble =>
            $self->type('boolean', $flag_type->is_requesteeble),
        is_multiplicable =>
            $self->type('boolean', $flag_type->is_multiplicable),
        grant_group =>
            $self->type('int', $flag_type->grant_group_id),
        request_group =>
            $self->type('int', $flag_type->request_group_id),
    }, undef, 'flag_types';
}

sub _version_to_hash {
    my ($self, $version, $params) = @_;
    return filter $params, {
        id =>
            $self->type('int', $version->id),
        name =>
            $self->type('string', $version->name),
        sort_key =>  # sort_key is returened to match Bug.fields
            0,
        is_active =>
            $self->type('boolean', $version->is_active),
    }, undef, 'versions';
}

sub _milestone_to_hash {
    my ($self, $milestone, $params) = @_;
    return filter $params, {
        id =>
            $self->type('int', $milestone->id),
        name =>
            $self->type('string', $milestone->name),
        sort_key =>
            $self->type('int', $milestone->sortkey),
        is_active =>
            $self->type('boolean', $milestone->is_active),
    }, undef, 'milestones';
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

Although the data input and output is the same for JSONRPC, XMLRPC and REST,
the directions for how to access the data via REST is noted in each method
where applicable.

=head1 List Products

=head2 get_selectable_products

B<EXPERIMENTAL>

=over

=item B<Description>

Returns a list of the ids of the products the user can search on.

=item B<REST>

GET /rest/product_selectable

the returned data format is same as below.

=item B<Params> (none)

=item B<Returns>

A hash containing one item, C<ids>, that contains an array of product
ids.

=item B<Errors> (none)

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head2 get_enterable_products

B<EXPERIMENTAL>

=over

=item B<Description>

Returns a list of the ids of the products the user can enter bugs
against.

=item B<REST>

GET /rest/product_enterable

the returned data format is same as below.

=item B<Params> (none)

=item B<Returns>

A hash containing one item, C<ids>, that contains an array of product
ids.

=item B<Errors> (none)

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head2 get_accessible_products

B<UNSTABLE>

=over

=item B<Description>

Returns a list of the ids of the products the user can search or enter
bugs against.

=item B<REST>

GET /rest/product_accessible

the returned data format is same as below.

=item B<Params> (none)

=item B<Returns>

A hash containing one item, C<ids>, that contains an array of product
ids.

=item B<Errors> (none)

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head2 get

B<EXPERIMENTAL>

=over

=item B<Description>

Returns a list of information about the products passed to it.

B<Note>: You must at least specify one of C<ids> or C<names>.

=item B<REST>

To return information about a specific groups of products such as
C<accessible>, C<selectable>, or C<enterable>:

GET /rest/product?type=accessible

To return information about a specific product by C<id> or C<name>:

GET /rest/product/<product_id_or_name>

You can also return information about more than one specific product
by using the following in your query string:

GET /rest/product?ids=1&ids=2&ids=3 or GET /product?names=ProductOne&names=Product2

the returned data format is same as below.

=item B<Params>

In addition to the parameters below, this method also accepts the
standard L<include_fields|Bugzilla::WebService/include_fields> and
L<exclude_fields|Bugzilla::WebService/exclude_fields> arguments.

This RPC call supports sub field restrictions.

=over

=item C<ids>

An array of product ids

=item C<names>

An array of product names

=item C<type>

The group of products to return. Valid values are: C<accessible> (default),
C<selectable>, and C<enterable>. C<type> can be a single value or an array
of values if more than one group is needed with duplicates removed.

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
new bugs by default. Empty string if the QA contact is not defined.

=item C<sort_key>

C<int> Components, when displayed in a list, are sorted first by this integer
and then secondly by their name.

=item C<is_active>

C<boolean> A boolean indicating if the component is active.  Inactive
components are not enabled for new bugs.

=item C<flag_types>

A hash containing the two items C<bug> and C<attachment> that each contains an
array of hashes, where each hash describes a flagtype, and has the
following items:

=over

=item C<id>

C<int> Returns the ID of the flagtype.

=item C<name>

C<string> Returns the name of the flagtype.

=item C<description>

C<string> Returns the description of the flagtype.

=item C<cc_list>

C<string> Returns the concatenated CC list for the flagtype, as a single string.

=item C<sort_key>

C<int> Returns the sortkey of the flagtype.

=item C<is_active>

C<boolean> Returns whether the flagtype is active or disabled. Flags being
in a disabled flagtype are not deleted. It only prevents you from
adding new flags to it.

=item C<is_requestable>

C<boolean> Returns whether you can request for the given flagtype
(i.e. whether the '?' flag is available or not).

=item C<is_requesteeble>

C<boolean> Returns whether you can ask someone specifically or not.

=item C<is_multiplicable>

C<boolean> Returns whether you can have more than one flag for the given
flagtype in a given bug/attachment.

=item C<grant_group>

C<int> the group id that is allowed to grant/deny flags of this type.
If the item is not included all users are allowed to grant/deny this
flagtype.

=item C<request_group>

C<int> the group id that is allowed to request the flag if the flag
is of the type requestable. If the item is not included all users
are allowed request this flagtype.

=back

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

=item In Bugzilla B<4.4>, C<flag_types> was added to the fields returned
by C<get>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head1 Product Creation and Modification

=head2 create

B<EXPERIMENTAL>

=over

=item B<Description>

This allows you to create a new product in Bugzilla.

=item B<REST>

POST /rest/product

The params to include in the POST body as well as the returned data format,
are the same as below.

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

C<string> The default milestone for this product. Default '---'.

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

=item B<History>

=over

=item REST API call added in Bugzilla B<5.0>.

=back

=back

=head2 update

B<EXPERIMENTAL>

=over

=item B<Description>

This allows you to update a product in Bugzilla.

=item B<REST>

PUT /rest/product/<product_id_or_name>

The params to include in the PUT body as well as the returned data format,
are the same as below. The C<ids> and C<names> params will be overridden as
it is pulled from the URL path.

=item B<Params>

B<Note:> The following parameters specify which products you are updating.
You must set one or both of these parameters.

=over

=item C<ids>

C<array> of C<int>s. Numeric ids of the products that you wish to update.

=item C<names>

C<array> or C<string>s. Names of the products that you wish to update.

=back

B<Note:> The following parameters specify the new values you want to set for
the products you are updating.

=over

=item C<name>

C<string> A new name for this product. If you try to set this while updating more
than one product, an error will occur, as product names must be unique.

=item C<default_milestone>

C<string> When a new bug is filed, what milestone does it get by default if the
user does not choose one? Must represent a milestone that is valid for this product.

=item C<description>

C<string> Update the long description for these products to this value.

=item C<has_unconfirmed>

C<boolean> Allow the UNCONFIRMED status to be set on bugs in products.

=item C<is_open>

C<boolean> True if the product is currently allowing bugs to be entered
into it, False otherwise.

=back

=item B<Returns>

A C<hash> with a single field "products". This points to an array of hashes
with the following fields:

=over

=item C<id>

C<int> The id of the product that was updated.

=item C<changes>

C<hash> The changes that were actually done on this product. The keys are
the names of the fields that were changed, and the values are a hash
with two keys:

=over

=item C<added>

C<string> The value that this field was changed to.

=item C<removed>

C<string> The value that was previously set in this field.

=back

Note that booleans will be represented with the strings '1' and '0'.

Here's an example of what a return value might look like:

 {
   products => [
     {
       id => 123,
       changes => {
         name => {
           removed => 'FooName',
           added   => 'BarName'
         },
         has_unconfirmed => {
           removed => '1',
           added   => '0',
         }
       }
     }
   ]
 }

=item B<Errors>

=over

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

=item 705 (Product must define a default milestone)

You must define a default milestone.

=back

=back

=item B<History>

=over

=item Added in Bugzilla B<4.4>.

=item REST API call added in Bugzilla B<5.0>.

=back

=back
