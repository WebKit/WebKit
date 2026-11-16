# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::BugUrl::Bugzilla::Local;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::BugUrl::Bugzilla);

use Bugzilla::Error;
use Bugzilla::Util;

###############################
####    Initialization     ####
###############################

use constant VALIDATOR_DEPENDENCIES => {
    value => ['bug_id'],
};

###############################
####        Methods        ####
###############################

sub ref_bug_url {
    my $self = shift;

    if (!exists $self->{ref_bug_url}) {
        my $ref_bug_id = new URI($self->name)->query_param('id');
        my $ref_bug = Bugzilla::Bug->check($ref_bug_id);
        my $ref_value = $self->local_uri($self->bug_id);
        $self->{ref_bug_url} =
            new Bugzilla::BugUrl::Bugzilla::Local({ bug_id => $ref_bug->id,
                                                    value => $ref_value });
    }
    return $self->{ref_bug_url};
}

sub should_handle {
    my ($class, $uri) = @_;

    # Check if it is either a bug id number or an alias.
    return 1 if $uri->as_string =~ m/^\w+$/;

    # Check if it is a local Bugzilla uri and call
    # Bugzilla::BugUrl::Bugzilla to check if it's a valid Bugzilla
    # see also url.
    my $canonical_local = URI->new($class->local_uri)->canonical;
    if ($canonical_local->authority eq $uri->canonical->authority
        and $canonical_local->path eq $uri->canonical->path)
    {
        return $class->SUPER::should_handle($uri);
    }

    return 0;
}

sub _check_value {
    my ($class, $uri, undef, $params) = @_;

    # At this point we are going to treat any word as a
    # bug id/alias to the local Bugzilla.
    my $value = $uri->as_string;
    if ($value =~ m/^\w+$/) {
        $uri = new URI($class->local_uri($value));
    } else {
        # It's not a word, then we have to check
        # if it's a valid Bugzilla url.
        $uri = $class->SUPER::_check_value($uri);
    }

    my $ref_bug_id  = $uri->query_param('id');
    my $ref_bug     = Bugzilla::Bug->check($ref_bug_id);
    my $self_bug_id = $params->{bug_id};
    $params->{ref_bug} = $ref_bug;

    if ($ref_bug->id == $self_bug_id) {
        ThrowUserError('see_also_self_reference');
    }
 
    return $uri;
}

sub local_uri {
    my ($self, $bug_id) = @_;
    $bug_id ||= '';
    return correct_urlbase() . "show_bug.cgi?id=$bug_id";
}

1;
