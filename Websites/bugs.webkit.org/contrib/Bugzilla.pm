# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package Bugzilla;

use 5.10.1;
use strict;
use warnings;

#######################################################################
# The goal of this tiny module is to let Bugzilla packagers of        #
# various Linux distributions easily specify the path to the Bugzilla #
# modules without having to edit each *.cgi script manually to fix    #
# the | use lib qw(. lib) | line.                                     #
#                                                                     #
# If the real Bugzilla.pm module and the Bugzilla/ directory are in   #
# the same directory as *.cgi scripts (which is the default), then    #
# this module is useless and has no effect.                           #
# If the real Bugzilla.pm module and other Bugzilla/*.pm modules are  #
# moved elsewhere, then the *.cgi scripts will call this module which #
# will give them the location of the Bugzilla modules.                #
#######################################################################
#                                                                     #
# IMPORTANT NOTE: This module must be in a place where Perl will find #
# it by default, e.g. in /usr/lib/perl5/vendor_perl/5.x.y/.           #
#                                                                     #
#######################################################################
#                                                                     #
# Specify the path to the real Bugzilla.pm file in BZ_ROOT_DIR.       #
# Some Linux distros may set it to '/usr/share/bugzilla/lib'.         #
#                                                                     #
# Specify the path to the Bugzilla lib/ directory in BZ_LIB_DIR.      #
# If your Linux distro doesn't use it, make it point to BZ_ROOT_DIR.  #
#                                                                     #
#######################################################################

use constant BZ_ROOT_DIR => '/usr/share/bugzilla/lib';
use constant BZ_LIB_DIR => BZ_ROOT_DIR . '/lib';

#######################################################################
#   DO NOT EDIT THE CODE BELOW, UNLESS YOU KNOW WHAT YOU ARE DOING!!  #
#######################################################################

use lib (BZ_ROOT_DIR, BZ_LIB_DIR);

# Now load the real Bugzilla.pm file.
BEGIN { my $bugzilla = BZ_ROOT_DIR . '/Bugzilla.pm'; require $bugzilla; }

1;
