# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Comment::TagWeights;

use 5.10.1;
use strict;
use warnings;

use parent qw(Bugzilla::Object);

use Bugzilla::Constants;

# No auditing required
use constant AUDIT_CREATES => 0;
use constant AUDIT_UPDATES => 0;
use constant AUDIT_REMOVES => 0;

use constant DB_COLUMNS => qw(
    id
    tag
    weight
);

use constant UPDATE_COLUMNS => qw(
    weight
);

use constant DB_TABLE   => 'longdescs_tags_weights';
use constant ID_FIELD   => 'id';
use constant NAME_FIELD => 'tag';
use constant LIST_ORDER => 'weight DESC';
use constant VALIDATORS => { };

# There's no gain to caching these objects
use constant USE_MEMCACHED => 0;

sub tag    { return $_[0]->{'tag'} }
sub weight { return $_[0]->{'weight'} }

sub set_weight { $_[0]->set('weight', $_[1]); }

1;

=head1 NAME

Comment::TagWeights - Bugzilla comment weighting class.

=head1 DESCRIPTION

TagWeights.pm represents a Comment::TagWeight object. It is an implementation
of L<Bugzilla::Object>, and thus provides all methods that L<Bugzilla::Object>
provides.

TagWeights is used to quickly find tags and order by their usage count.

=head1 PROPERTIES

=over

=item C<tag>

C<getter string> The tag

=item C<weight>

C<getter int> The tag's weight. The value returned corresponds to the number of
comments with this tag attached.

=item C<set_weight>

C<setter int> Set the tag's weight.

=back
