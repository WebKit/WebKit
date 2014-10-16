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
# Contributor(s): 
#   Carole Pryfer <carole.pryfer@dgfip.finances.gouv.fr>

package Bugzilla::WebService::Group;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla::Constants;
use Bugzilla::Error;

sub create {
    my ($self, $params) = @_;

    Bugzilla->login(LOGIN_REQUIRED);
    Bugzilla->user->in_group('creategroups') 
        || ThrowUserError("auth_failure", { group  => "creategroups",
                                            action => "add",
                                            object => "group"});
    # Create group
    my $group = Bugzilla::Group->create({
        name               => $params->{name},
        description        => $params->{description},
        userregexp         => $params->{user_regexp},
        isactive           => $params->{is_active},
        isbuggroup         => 1,
        icon_url           => $params->{icon_url}
    });
    return { id => $self->type('int', $group->id) };
}

1;

__END__

=head1 NAME

Bugzilla::Webservice::Group - The API for creating, changing, and getting
information about Groups.

=head1 DESCRIPTION

This part of the Bugzilla API allows you to create Groups and
get information about them.

=head1 METHODS

See L<Bugzilla::WebService> for a description of how parameters are passed,
and what B<STABLE>, B<UNSTABLE>, and B<EXPERIMENTAL> mean.

=head1 Group Creation

=head2 create

B<UNSTABLE>

=over

=item B<Description>

This allows you to create a new group in Bugzilla.

=item B<Params> 

Some params must be set, or an error will be thrown. These params are 
marked B<Required>.

=over

=item C<name>

B<Required> C<string> A short name for this group. Must be unique. This
is not usually displayed in the user interface, except in a few places.

=item C<description>

B<Required> C<string> A human-readable name for this group. Should be
relatively short. This is what will normally appear in the UI as the
name of the group.

=item C<user_regexp>

C<string> A regular expression. Any user whose Bugzilla username matches
this regular expression will automatically be granted membership in this group.

=item C<is_active> 

C<boolean> C<True> if new group can be used for bugs, C<False> if this
is a group that will only contain users and no bugs will be restricted
to it.

=item C<icon_url>

C<string> A URL pointing to a small icon used to identify the group.
This icon will show up next to users' names in various parts of Bugzilla
if they are in this group.

=back

=item B<Returns>    

A hash with one element, C<id>. This is the id of the newly-created group.

=item B<Errors>

=over

=item 800 (Empty Group Name)

You must specify a value for the C<name> field.

=item 801 (Group Exists)

There is already another group with the same C<name>.

=item 802 (Group Missing Description)

You must specify a value for the C<description> field.

=item 803 (Group Regexp Invalid)

You specified an invalid regular expression in the C<user_regexp> field.

=back

=back 

=cut
