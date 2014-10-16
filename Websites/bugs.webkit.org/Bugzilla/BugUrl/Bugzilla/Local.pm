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
# The Initial Developer of the Original Code is Tiago Mello.
# Portions created by Tiago Mello are Copyright (C) 2010
# Tiago Mello. All Rights Reserved.
#
# Contributor(s): Tiago Mello <timello@linux.vnet.ibm.com>

package Bugzilla::BugUrl::Bugzilla::Local;
use strict;
use base qw(Bugzilla::BugUrl::Bugzilla);

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
 
    my $product = $ref_bug->product_obj;
    if (!Bugzilla->user->can_edit_product($product->id)) {
        ThrowUserError("product_edit_denied",
                       { product => $product->name });
    }

    return $uri;
}

sub local_uri {
    my ($self, $bug_id) = @_;
    $bug_id ||= '';
    return correct_urlbase() . "show_bug.cgi?id=$bug_id";
}

1;
