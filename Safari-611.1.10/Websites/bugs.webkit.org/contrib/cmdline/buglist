#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

defaultcolumnlist="severity priority platform status resolution target_milestone status_whiteboard keywords summaryfull"

thisdir=`dirname "$0"`
query=`$thisdir/makequery "$@"` 
if test "$?" != "0"; then exit 1; fi

outputfile="/dev/stdout"
#outputfile="buglist.html"
#\rm -f ${outputfile}
wget -q -O ${outputfile} --header="Cookie: COLUMNLIST=${COLUMNLIST-${defaultcolumnlist}}" "${query}"
