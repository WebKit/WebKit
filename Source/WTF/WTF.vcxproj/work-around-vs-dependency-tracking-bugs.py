#!/usr/bin/env python

import glob
import os
import re
import sys


# It's fragile to rely on the location of this script to find the top-level
# source directory.
TOP_LEVEL_DIRECTORY = os.environ['WEBKIT_SOURCE'];
WEBKIT_LIBRARIES = os.environ['WEBKIT_LIBRARIES'];
WEBKIT_SOURCE = os.environ['WEBKIT_SOURCE'];

def main():
    react_to_vsprops_changes()
    react_to_webkit1_interface_changes()


def react_to_vsprops_changes():
    vsprops_directory = os.path.join(WEBKIT_SOURCE, 'WebKit', 'WebKit.vcxproj')
    newest_vsprops_time = mtime_of_newest_file_matching_glob(os.path.join(vsprops_directory, '*.props'))

    obj_directory = os.path.join(os.environ['CONFIGURATIONBUILDDIR'], 'obj')

    # Visual Studio isn't smart enough to figure out it needs to rebuild these file types when
    # .vsprops files change (even if we touch wtf/Platform.h below), so we delete them to force them
    # to be rebuilt.
    for extension in ('dep', 'manifest', 'pch', 'res'):
        for filepath in glob.iglob(os.path.join(obj_directory, '*', '*.%s' % extension)):
            delete_if_older_than(filepath, newest_vsprops_time)

    # Touch wtf/Platform.h so all files will be recompiled. This is necessary
    # to pick up changes to preprocessor macros (e.g., ENABLE_*).
    wtf_platform_h = os.path.join(TOP_LEVEL_DIRECTORY, 'WTF', 'wtf', 'Platform.h')
    touch_if_older_than(wtf_platform_h, newest_vsprops_time)


def react_to_webkit1_interface_changes():
    interfaces_directory = os.path.join(TOP_LEVEL_DIRECTORY, 'WebKit', 'win', 'Interfaces')
    newest_idl_time = mtime_of_newest_file_matching_glob(os.path.join(interfaces_directory, '*.idl'))
    # WebKit.idl includes all the other IDL files, so needs to be rebuilt if any IDL file changes.
    # But Visual Studio isn't smart enough to figure this out, so we touch WebKit.idl to ensure that
    # it gets rebuilt.
    touch_if_older_than(os.path.join(interfaces_directory, 'WebKit.idl'), newest_idl_time)


def mtime_of_newest_file_matching_glob(glob_pattern):
    files = glob.glob(glob_pattern)
    assert len(files), "Couldn't find any files matching glob %s" % glob_pattern
    return max(map(os.path.getmtime, files))


def delete_if_older_than(path, reference_time):
    if os.path.getmtime(path) < reference_time:
        print 'Deleting %s' % path
        os.remove(path)


def touch_if_older_than(path, reference_time):
    if os.path.getmtime(path) < reference_time:
        print 'Touching %s' % path
        os.utime(path, None)


if __name__ == '__main__':
    sys.exit(main())
