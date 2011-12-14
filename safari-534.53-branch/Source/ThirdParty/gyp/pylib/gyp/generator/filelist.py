#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""filelist output module

Just dumps the list of files to a text file for import into another build system.
"""

import gyp.common
import errno
import os


# FIXME: These defaults are wrong, but serve to allow us to generate something for now.
generator_default_variables = {
  'OS': 'linux',
  'PRODUCT_DIR': '$(builddir)',
}


def GenerateOutput(target_list, target_dicts, data, params):
  options = params['options']

  for build_file, build_file_dict in data.iteritems():
    (build_file_root, build_file_ext) = os.path.splitext(build_file)
    if build_file_ext != '.gyp':
      continue

    filelist_suffix = ".am" # FIXME: This should be configurable in the .gyp file
    filelist_path = build_file_root + options.suffix + filelist_suffix

    with open(filelist_path, 'w') as output:

      for qualified_target in target_list:
        [input_file, target] = gyp.common.ParseQualifiedTarget(qualified_target)[0:2]

        lowered_target = target.lower() # FIXME: This is a hack for now to matche gtk automake style.

        output.write("%s_sources += \\\n" % lowered_target)

        for source in target_dicts[qualified_target]['sources']:
            output.write("\t%s \\\n" % source)
