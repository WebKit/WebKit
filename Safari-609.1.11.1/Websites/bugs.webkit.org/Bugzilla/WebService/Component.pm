# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::WebService::Component;

use 5.10.1;
use strict;
use warnings;

use base qw(Bugzilla::WebService);

use Bugzilla::Component;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::WebService::Constants;
use Bugzilla::WebService::Util qw(translate params_to_objects validate);

use constant PUBLIC_METHODS => qw(                                                                                                                                                                                                            
    create
);

use constant MAPPED_FIELDS => {
    default_assignee   => 'initialowner',
    default_qa_contact => 'initialqacontact',
    default_cc         => 'initial_cc',
    is_open            => 'isactive',
};

sub create {
    my ($self, $params) = @_;

    my $user = Bugzilla->login(LOGIN_REQUIRED);

    $user->in_group('editcomponents')
        || scalar @{ $user->get_products_by_permission('editcomponents') }
        || ThrowUserError('auth_failure', { group  => 'editcomponents',
                                            action => 'edit',
                                            object => 'components' });

    my $product = $user->check_can_admin_product($params->{product});

    # Translate the fields
    my $values = translate($params, MAPPED_FIELDS);
    $values->{product} = $product;

    # Create the component and return the newly created id.
    my $component = Bugzilla::Component->create($values);
    return { id => $self->type('int', $component->id) };
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Component - The Component API

=head1 DESCRIPTION

This part of the Bugzilla API allows you to deal with the available product components.
You will be able to get information about them as well as manipulate them.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

=head1 Component Creation and Modification

=head2 create

B<EXPERIMENTAL>

=over

=item B<Description>

This allows you to create a new component in Bugzilla.

=item B<Params>

Some params must be set, or an error will be thrown. These params are
marked B<Required>.

=over

=item C<name>

B<Required> C<string> The name of the new component.

=item C<product>

B<Required> C<string> The name of the product that the component must be
added to. This product must already exist, and the user have the necessary
permissions to edit components for it.

=item C<description>

B<Required> C<string> The description of the new component.

=item C<default_assignee>

B<Required> C<string> The login name of the default assignee of the component.

=item C<default_cc>

C<array> An array of strings with each element representing one login name of the default CC list.

=item C<default_qa_contact>

C<string> The login name of the default QA contact for the component.

=item C<is_open>

C<boolean> 1 if you want to enable the component for bug creations. 0 otherwise. Default is 1.

=back

=item B<Returns>

A hash with one key: C<id>. This will represent the ID of the newly-added
component.

=item B<Errors>

=over

=item 304 (Authorization Failure)

You are not authorized to create a new component.

=item 1200 (Component already exists)

The name that you specified for the new component already exists in the
specified product.

=back

=item B<History>

=over

=item Added in Bugzilla B<5.0>.

=back

=back

