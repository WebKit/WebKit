#!/usr/bin/env python

import glob
import os
import re
import sys


def main():
    # It's fragile to rely on the location of this script to find the top-level
    # source directory.
    top_level_directory = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))

    vsprops_directory = os.path.join(top_level_directory, 'WebKitLibraries', 'win', 'tools', 'vsprops')
    vsprops_files = glob.glob(os.path.join(vsprops_directory, '*.vsprops'))
    assert len(vsprops_files), "Couldn't find any .vsprops files in %s" % vsprops_directory
    newest_vsprops_time = max(map(os.path.getmtime, vsprops_files))

    # Delete any manifest-related files because Visual Studio isn't smart
    # enough to figure out that it might need to rebuild them.
    obj_directory = os.path.join(os.environ['CONFIGURATIONBUILDDIR'], 'obj')
    for manifest_file in glob.iglob(os.path.join(obj_directory, '*', '*.manifest*')):
        delete_if_older_than(manifest_file, newest_vsprops_time)

    # Delete any precompiled headers because Visual Studio isn't smart enough
    # to figure out that it might need to rebuild them, even if we touch
    # wtf/Platform.h below.
    for precompiled_header in glob.iglob(os.path.join(obj_directory, '*', '*.pch')):
        delete_if_older_than(precompiled_header, newest_vsprops_time)

    # Touch wtf/Platform.h so all files will be recompiled. This is necessary
    # to pick up changes to preprocessor macros (e.g., ENABLE_*).
    wtf_platform_h = os.path.join(top_level_directory, 'Source', 'JavaScriptCore', 'wtf', 'Platform.h')
    if os.path.getmtime(wtf_platform_h) < newest_vsprops_time:
        print 'Touching wtf/Platform.h'
        os.utime(wtf_platform_h, None)


def delete_if_older_than(path, reference_time):
    if os.path.getmtime(path) < reference_time:
        print 'Deleting %s' % path
        os.remove(path)


if __name__ == '__main__':
    sys.exit(main())
