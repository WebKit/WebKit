#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

thisdir=`dirname "$0"`
buglist="$thisdir/buglist"
csvfile="$thisdir/buglist.csv"

$thisdir/buglist "$@" 2>&1 1>${csvfile}
if test "$?" != "0"; then cat "$csvfile" 1>&2; exit 1; fi

# 1. use 'awk' to select the first column (bug_id)
# 2. use 'grep -v' to remove the first line with the column headers
# 3. use backquotes & 'echo' to get all values in one line, space separated
echo `cat ${csvfile} | awk -F, '{printf $1 "\n"}' | grep -v bug_id`
