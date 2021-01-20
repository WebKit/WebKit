# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::FlagType;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::WebService);
use Bugzilla::Component;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::FlagType;
use Bugzilla::Product;
use Bugzilla::Util qw(trim);

use List::MoreUtils qw(uniq);

use constant PUBLIC_METHODS => qw(
    create
    get
    update
);

sub get {
    my ($self, $params) = @_;
    my $dbh  = Bugzilla->switch_to_shadow_db();
    my $user = Bugzilla->user;

    defined $params->{product}
        || ThrowCodeError('param_required',
                          { function => 'Bug.flag_types',
                            param   => 'product' });

    my $product   = delete $params->{product};
    my $component = delete $params->{component};

    $product = Bugzilla::Product->check({ name => $product, cache => 1 });
    $component = Bugzilla::Component->check(
        { name => $component, product => $product, cache => 1 }) if $component;

    my $flag_params = { product_id => $product->id };
    $flag_params->{component_id} = $component->id if $component;
    my $matched_flag_types = Bugzilla::FlagType::match($flag_params);

    my $flag_types = { bug => [], attachment => [] };
    foreach my $flag_type (@$matched_flag_types) {
        push(@{ $flag_types->{bug} }, $self->_flagtype_to_hash($flag_type, $product))
            if $flag_type->target_type eq 'bug';
        push(@{ $flag_types->{attachment} }, $self->_flagtype_to_hash($flag_type, $product))
            if $flag_type->target_type eq 'attachment';
    }

    return $flag_types;
}

sub create {
    my ($self, $params) = @_;
    my $user = Bugzilla->login(LOGIN_REQUIRED);

    $user->in_group('editcomponents')
        || scalar(@{$user->get_products_by_permission('editcomponents')})
        || ThrowUserError("auth_failure", { group => "editcomponents",
                                         action => "add",
                                         object => "flagtypes" });

    $params->{name} || ThrowCodeError('param_required', { param => 'name' });
    $params->{description} || ThrowCodeError('param_required', { param => 'description' });

    my %args = (
        sortkey => 1,
        name => undef,
        inclusions => ['0:0'],  # Default to __ALL__:__ALL__
        cc_list => '',
        description => undef,
        is_requestable => 'on',
        exclusions => [],
        is_multiplicable => 'on',
        request_group => '',
        is_active => 'on',
        is_specifically_requestable => 'on',
        target_type => 'bug',
        grant_group => '',
    );

    foreach my $key (keys %args) {
        $args{$key} = $params->{$key} if defined($params->{$key});
    }

    $args{name} = trim($params->{name});
    $args{description} = trim($params->{description});

    # Is specifically requestable is actually is_requesteeable
    if (exists $args{is_specifically_requestable}) {
        $args{is_requesteeble} = delete $args{is_specifically_requestable};
    }

    # Default is on for the tickbox flags.
    # If the user has set them to 'off' then undefine them so the flags are not ticked
    foreach my $arg_name (qw(is_requestable is_multiplicable is_active is_requesteeble)) {
        if (defined($args{$arg_name}) && ($args{$arg_name} eq '0')) {
            $args{$arg_name} = undef;
        }
    }

    # Process group inclusions and exclusions
    $args{inclusions} = _process_lists($params->{inclusions}) if defined $params->{inclusions};
    $args{exclusions} = _process_lists($params->{exclusions}) if defined $params->{exclusions};

    my $flagtype = Bugzilla::FlagType->create(\%args);

    return { id => $self->type('int', $flagtype->id)  };
}

sub update {
    my ($self, $params) = @_;
    my $dbh = Bugzilla->dbh;
    my $user = Bugzilla->login(LOGIN_REQUIRED);

    $user->in_group('editcomponents')
        || scalar(@{$user->get_products_by_permission('editcomponents')})
        || ThrowUserError("auth_failure", { group  => "editcomponents",
                                            action => "edit",
                                            object => "flagtypes" });

    defined($params->{names}) || defined($params->{ids})
        || ThrowCodeError('params_required',
               { function => 'FlagType.update', params => ['ids', 'names'] });

    # Get the list of unique flag type ids we are updating
    my @flag_type_ids = defined($params->{ids}) ? @{$params->{ids}} : ();
    if (defined $params->{names}) {
        push @flag_type_ids, map { $_->id }
            @{ Bugzilla::FlagType::match({ name => $params->{names} }) };
    }
    @flag_type_ids = uniq @flag_type_ids;

    # We delete names and ids to keep only new values to set.
    delete $params->{names};
    delete $params->{ids};

    # Process group inclusions and exclusions
    # We removed them from $params because these are handled differently
    my $inclusions = _process_lists(delete $params->{inclusions}) if defined $params->{inclusions};
    my $exclusions = _process_lists(delete $params->{exclusions}) if defined $params->{exclusions};

    $dbh->bz_start_transaction();
    my %changes = ();

    foreach my $flag_type_id (@flag_type_ids) {
        my ($flagtype, $can_fully_edit) = $user->check_can_admin_flagtype($flag_type_id);

        if ($can_fully_edit) {
            $flagtype->set_all($params);
        }
        elsif (scalar keys %$params) {
            ThrowUserError('flag_type_not_editable', { flagtype => $flagtype });
        }

        # Process the clusions
        foreach my $type ('inclusions', 'exclusions') {
            my $clusions = $type eq 'inclusions' ? $inclusions : $exclusions;
            next if not defined $clusions;

            my @extra_clusions = ();
            if (!$user->in_group('editcomponents')) {
                my $products = $user->get_products_by_permission('editcomponents');
                # Bring back the products the user cannot edit.
                foreach my $item (values %{$flagtype->$type}) {
                    my ($prod_id, $comp_id) = split(':', $item);
                    push(@extra_clusions, $item) unless grep { $_->id == $prod_id } @$products;
                }
            }

            $flagtype->set_clusions({
                $type => [@$clusions, @extra_clusions],
            });
        }

        my $returned_changes = $flagtype->update();
        $changes{$flagtype->id} = {
            name    => $flagtype->name,
            changes => $returned_changes,
        };
    }
    $dbh->bz_commit_transaction();

    my @result;
    foreach my $flag_type_id (keys %changes) {
        my %hash = (
            id      => $self->type('int',    $flag_type_id),
            name    => $self->type('string', $changes{$flag_type_id}{name}),
            changes => {},
        );

        foreach my $field (keys %{ $changes{$flag_type_id}{changes} }) {
            my $change = $changes{$flag_type_id}{changes}{$field};
            $hash{changes}{$field} = {
                removed => $self->type('string', $change->[0]),
                added   => $self->type('string', $change->[1])
            };
        }

        push(@result, \%hash);
    }

    return { flagtypes => \@result };
}

sub _flagtype_to_hash {
    my ($self, $flagtype, $product) = @_;
    my $user = Bugzilla->user;

    my @values = ('X');
    push(@values, '?') if ($flagtype->is_requestable && $user->can_request_flag($flagtype));
    push(@values, '+', '-') if $user->can_set_flag($flagtype);

    my $item = {
        id          => $self->type('int'    , $flagtype->id),
        name        => $self->type('string' , $flagtype->name),
        description => $self->type('string' , $flagtype->description),
        type        => $self->type('string' , $flagtype->target_type),
        values      => \@values,
        is_active   => $self->type('boolean', $flagtype->is_active),
        is_requesteeble  => $self->type('boolean', $flagtype->is_requesteeble),
        is_multiplicable => $self->type('boolean', $flagtype->is_multiplicable)
    };

    if ($product) {
        my $inclusions = $self->_flagtype_clusions_to_hash($flagtype->inclusions, $product->id);
        my $exclusions = $self->_flagtype_clusions_to_hash($flagtype->exclusions, $product->id);
        # if we have both inclusions and exclusions, the exclusions are redundant
        $exclusions = [] if @$inclusions && @$exclusions;
        # no need to return anything if there's just "any component"
        $item->{inclusions} = $inclusions if @$inclusions && $inclusions->[0] ne '';
        $item->{exclusions} = $exclusions if @$exclusions && $exclusions->[0] ne '';
    }

    return $item;
}

sub _flagtype_clusions_to_hash {
    my ($self, $clusions, $product_id) = @_;
    my $result = [];
    foreach my $key (keys %$clusions) {
        my ($prod_id, $comp_id) = split(/:/, $clusions->{$key}, 2);
        if ($prod_id == 0 || $prod_id == $product_id) {
            if ($comp_id) {
                my $component = Bugzilla::Component->new({ id => $comp_id, cache => 1 });
                push @$result, $component->name;
            }
            else {
                return [ '' ];
            }
        }
    }
    return $result;
}

sub _process_lists {
    my $list = shift;
    my $user = Bugzilla->user;

    my @products;
    if ($user->in_group('editcomponents')) {
        @products = Bugzilla::Product->get_all;
    }
    else {
        @products = @{$user->get_products_by_permission('editcomponents')};
    }

    my @component_list;

    foreach my $item (@$list) {
        # A hash with products as the key and component names as the values
        if(ref($item) eq 'HASH') {
            while (my ($product_name, $component_names) = each %$item) {
                my $product = Bugzilla::Product->check({name => $product_name});
                unless (grep { $product->name eq $_->name } @products) {
                    ThrowUserError('product_access_denied', { name => $product_name });
                }
                my @component_ids;

                foreach my $comp_name (@$component_names) {
                    my $component = Bugzilla::Component->check({product => $product, name => $comp_name});
                    ThrowCodeError('param_invalid', { param => $comp_name}) unless defined $component;
                    push @component_list, $product->id . ':' . $component->id;
                }
            }
        }
        elsif(!ref($item)) {
            # These are whole products
            my $product = Bugzilla::Product->check({name => $item});
            unless (grep { $product->name eq $_->name } @products) {
                ThrowUserError('product_access_denied', { name => $item });
            }
            push @component_list, $product->id . ':0';
        }
        else {
            # The user has passed something invalid
            ThrowCodeError('param_invalid', { param => $item });
        }
    }

    return \@component_list;
}

1;

__END__

=head1 NAME

Bugzilla::WebService::FlagType - API for creating flags.

=head1 DESCRIPTION

This part of the Bugzilla API allows you to create new flags

=head1 METHODS

See L<Bugzilla::WebService> for a description of what B<STABLE>, B<UNSTABLE>,
and B<EXPERIMENTAL> mean, and for more description about error codes.

=head2 Get Flag Types

=over

=item C<get> B<UNSTABLE>

=item B<Description>

Get information about valid flag types that can be set for bugs and attachments.

=item B<REST>

You have several options for retreiving information about flag types. The first
part is the request method and the rest is the related path needed.

To get information about all flag types for a product:

GET /rest/flag_type/<product>

To get information about flag_types for a product and component:

GET /rest/flag_type/<product>/<component>

The returned data format is the same as below.

=item B<Params>

You must pass a product name and an optional component name.

=over

=item C<product>   (string) - The name of a valid product.

=item C<component> (string) - An optional valid component name associated with the product.

=back

=item B<Returns>

A hash containing two keys, C<bug> and C<attachment>. Each key value is an array of hashes,
containing the following keys:

=over

=item C<id>

C<int> An integer id uniquely identifying this flag type.

=item C<name>

C<string> The name for the flag type.

=item C<type>

C<string> The target of the flag type which is either C<bug> or C<attachment>.

=item C<description>

C<string> The description of the flag type.

=item C<values>

C<array> An array of string values that the user can set on the flag type.

=item C<is_requesteeble>

C<boolean> Users can ask specific other users to set flags of this type.

=item C<is_multiplicable>

C<boolean> Multiple flags of this type can be set for the same bug or attachment.

=back

=item B<Errors>

=over

=item 106 (Product Access Denied)

Either the product does not exist or you don't have access to it.

=item 51 (Invalid Component)

The component provided does not exist in the product.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back

=head2 Create Flag

=over

=item C<create> B<UNSTABLE>

=item B<Description>

Creates a new FlagType

=item B<REST>

POST /rest/flag_type

The params to include in the POST body as well as the returned data format,
are the same as below.

=item B<Params>

At a minimum the following two arguments must be supplied:

=over

=item C<name> (string) - The name of the new Flag Type.

=item C<description> (string) - A description for the Flag Type object.

=back

=item B<Returns>

C<int> flag_id

The ID of the new FlagType object is returned.

=item B<Params>

=over

=item name B<required>

C<string> A short name identifying this type.

=item description B<required>

C<string> A comprehensive description of this type.

=item inclusions B<optional>

An array of strings or a hash containing product names, and optionally
component names. If you provide a string, the flag type will be shown on
all bugs in that product. If you provide a hash, the key represents the
product name, and the value is the components of the product to be included.

For example:

 [ 'FooProduct',
   {
     BarProduct => [ 'C1', 'C3' ],
     BazProduct => [ 'C7' ]
   }
 ]

This flag will be added to B<All> components of I<FooProduct>,
components C1 and C3 of I<BarProduct>, and C7 of I<BazProduct>.

=item exclusions B<optional>

An array of strings or hashes containing product names. This uses the same
fromat as inclusions.

This will exclude the flag from all products and components specified.

=item sortkey B<optional>

C<int> A number between 1 and 32767 by which this type will be sorted when
displayed to users in a list; ignore if you don't care what order the types
appear in or if you want them to appear in alphabetical order.

=item is_active B<optional>

C<boolean> Flag of this type appear in the UI and can be set. Default is B<true>.

=item is_requestable B<optional>

C<boolean> Users can ask for flags of this type to be set. Default is B<true>.

=item cc_list B<optional>

C<array> An array of strings. If the flag type is requestable, who should
receive e-mail notification of requests. This is an array of e-mail addresses
which do not need to be Bugzilla logins.

=item is_specifically_requestable B<optional>

C<boolean> Users can ask specific other users to set flags of this type as
opposed to just asking the wind.  Default is B<true>.

=item is_multiplicable B<optional>

C<boolean> Multiple flags of this type can be set on the same bug. Default is B<true>.

=item grant_group B<optional>

C<string> The group allowed to grant/deny flags of this type (to allow all
users to grant/deny these flags, select no group). Default is B<no group>.

=item request_group B<optional>

C<string> If flags of this type are requestable, the group allowed to request
them (to allow all users to request these flags, select no group). Note that
the request group alone has no effect if the grant group is not defined!
Default is B<no group>.

=back

=item B<Errors>

=over

=item 51 (Group Does Not Exist)

The group name you entered does not exist, or you do not have access to it.

=item 105 (Unknown component)

The component does not exist for this product.

=item 106 (Product Access Denied)

Either the product does not exist or you don't have editcomponents privileges
to it.

=item 501 (Illegal Email Address)

One of the e-mail address in the CC list is invalid. An e-mail in the CC
list does NOT need to be a valid Bugzilla user.

=item 1101 (Flag Type Name invalid)

You must specify a non-blank name for this flag type. It must
no contain spaces or commas, and must be 50 characters or less.

=item 1102 (Flag type must have description)

You must specify a description for this flag type.

=item 1103 (Flag type CC list is invalid

The CC list must be 200 characters or less.

=item 1104 (Flag Type Sort Key Not Valid)

The sort key is not a valid number.

=item 1105 (Flag Type Not Editable)

This flag type is not available for the products you can administer. Therefore
you can not edit attributes of the flag type, other than the inclusion and
exclusion list.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back

=head2 update

B<EXPERIMENTAL>

=over

=item B<Description>

This allows you to update a flag type in Bugzilla.

=item B<REST>

PUT /rest/flag_type/<product_id_or_name>

The params to include in the PUT body as well as the returned data format,
are the same as below. The C<ids> and C<names> params will be overridden as
it is pulled from the URL path.

=item B<Params>

B<Note:> The following parameters specify which products you are updating.
You must set one or both of these parameters.

=over

=item C<ids>

C<array> of C<int>s. Numeric ids of the flag types that you wish to update.

=item C<names>

C<array> of C<string>s. Names of the flag types that you wish to update. If
many flag types have the same name, this will change ALL of them.

=back

B<Note:> The following parameters specify the new values you want to set for
the products you are updating.

=over

=item name

C<string> A short name identifying this type.

=item description

C<string> A comprehensive description of this type.

=item inclusions B<optional>

An array of strings or a hash containing product names, and optionally
component names. If you provide a string, the flag type will be shown on
all bugs in that product. If you provide a hash, the key represents the
product name, and the value is the components of the product to be included.

for example

 [ 'FooProduct',
   {
     BarProduct => [ 'C1', 'C3' ],
     BazProduct => [ 'C7' ]
   }
 ]

This flag will be added to B<All> components of I<FooProduct>,
components C1 and C3 of I<BarProduct>, and C7 of I<BazProduct>.

=item exclusions B<optional>

An array of strings or hashes containing product names.
This uses the same fromat as inclusions.

This will exclude the flag from all products and components specified.

=item sortkey

C<int> A number between 1 and 32767 by which this type will be sorted when
displayed to users in a list; ignore if you don't care what order the types
appear in or if you want them to appear in alphabetical order.

=item is_active

C<boolean> Flag of this type appear in the UI and can be set.

=item is_requestable

C<boolean> Users can ask for flags of this type to be set.

=item cc_list

C<array> An array of strings. If the flag type is requestable, who should
receive e-mail notification of requests. This is an array of e-mail addresses
which do not need to be Bugzilla logins.

=item is_specifically_requestable

C<boolean> Users can ask specific other users to set flags of this type as
opposed to just asking the wind.

=item is_multiplicable

C<boolean> Multiple flags of this type can be set on the same bug.

=item grant_group

C<string> The group allowed to grant/deny flags of this type (to allow all
users to grant/deny these flags, select no group).

=item request_group

C<string> If flags of this type are requestable, the group allowed to request
them (to allow all users to request these flags, select no group). Note that
the request group alone has no effect if the grant group is not defined!

=back

=item B<Returns>

A C<hash> with a single field "flagtypes". This points to an array of hashes
with the following fields:

=over

=item C<id>

C<int> The id of the product that was updated.

=item C<name>

C<string> The name of the product that was updated.

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
           removed => 'FooFlagType',
           added   => 'BarFlagType'
         },
         is_requestable => {
           removed => '1',
           added   => '0',
         }
       }
     }
   ]
 }

=back

=item B<Errors>

=over

=item 51 (Group Does Not Exist)

The group name you entered does not exist, or you do not have access to it.

=item 105 (Unknown component)

The component does not exist for this product.

=item 106 (Product Access Denied)

Either the product does not exist or you don't have editcomponents privileges
to it.

=item 501 (Illegal Email Address)

One of the e-mail address in the CC list is invalid. An e-mail in the CC
list does NOT need to be a valid Bugzilla user.

=item 1101 (Flag Type Name invalid)

You must specify a non-blank name for this flag type. It must
no contain spaces or commas, and must be 50 characters or less.

=item 1102 (Flag type must have description)

You must specify a description for this flag type.

=item 1103 (Flag type CC list is invalid

The CC list must be 200 characters or less.

=item 1104 (Flag Type Sort Key Not Valid)

The sort key is not a valid number.

=item 1105 (Flag Type Not Editable)

This flag type is not available for the products you can administer. Therefore
you can not edit attributes of the flag type, other than the inclusion and
exclusion list.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back
