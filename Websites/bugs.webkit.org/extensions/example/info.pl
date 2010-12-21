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
# The Original Code is the Bugzilla Example WebService Plugin
#
# The Initial Developer of the Original Code is Everything Solved, Inc.
# Portions created by Everything Solved, Inc. are Copyright (C) 2007
# Everything Solved, Inc. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Colin Ogilvie <colin.ogilvie@gmail.com>

# This script does some code to return a hash about the Extension.
# You are required to return a hash containing the Extension version
# You can optionaally add any other values to the hash too, as long as
# they begin with an x_
#
# Eg:
# {
#   'version' => '0.1', # required
#   'x_name'  => 'Example Extension'
# }

use strict;
no warnings qw(void); # Avoid "useless use of a constant in void context"
use Bugzilla::Constants;

{
	'version' => BUGZILLA_VERSION,
	'x_blah'  => 'Hello....',

};
