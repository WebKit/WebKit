#!/bin/bash

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is run in a git repository. It lists all header files,
# sorted by the number of other files where the file name of the file
# occurs. It is intentionally not limited to only source files, and
# there may be some false hits because we search only for the file
# part (sans directory). It is quite slow.
#
# Headers close to the top of the list are candidates for removal.

# If the name includes at most one directory, keep name unchanged,
# otherwise strip directories. Needed to work with relative #includes
# which are used in some parts of the tree, while still avoiding,
# e.g., api/foo.h to match includes of pc/foo.h.
simplify_name () {
  if expr "$1" : '.*/.*/' > /dev/null ; then
    basename "$1"
  else
    echo "$1"
  fi
}

git ls-files '*.h' '*.hpp' | while read header ; do
  name="$(simplify_name "${header}")"
  count="$(git grep -l -F  "${name}" \
           | grep -v -e '\.gn' -e '\.gyp'  \
           | wc -l)"
  echo "${count}" "${header}"
done | sort -n
