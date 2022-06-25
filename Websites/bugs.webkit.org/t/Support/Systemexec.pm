# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Support::Systemexec;

use 5.10.1;
use strict;
use warnings;

use parent qw(Exporter);
@Support::Systemexec::EXPORT = qw(system exec);

sub system($$@) {
  1;
}

sub exec($$@) {
  1;
}

1;
