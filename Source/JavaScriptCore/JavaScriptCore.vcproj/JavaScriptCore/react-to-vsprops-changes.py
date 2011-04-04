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

    obj_directory = os.path.join(os.environ['CONFIGURATIONBUILDDIR'], 'obj')

    # Visual Studio isn't smart enough to figure out it needs to rebuild these file types when
    # .vsprops files change (even if we touch wtf/Platform.h below), so we delete them to force them
    # to be rebuilt.
    for extension in ('dep', 'manifest', 'pch', 'res'):
        for filepath in glob.iglob(os.path.join(obj_directory, '*', '*.%s' % extension)):
            delete_if_older_than(filepath, newest_vsprops_time)

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
