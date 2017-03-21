#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


thisdir=`dirname "$0"`
bugids=`$thisdir/bugids "$@"` || echo "$bugids" 1>&2 && exit 1

bugids=`echo "$bugids" | sed -e 's/ /\,/g'`
echo "https://bugzilla.mozilla.org/buglist.cgi?ctype=html&bug_id=$bugids"
