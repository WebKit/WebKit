#!/usr/bin/env python

import glob
import os
import re
import sys


def file_modification_times(directory):
    for dirpath, dirnames, filenames in os.walk(directory):
        for filename in filenames:
            yield os.path.getmtime(os.path.join(dirpath, filename))


def main():
    vsprops_directory = os.path.join(os.environ['WEBKITLIBRARIESDIR'], 'tools', 'vsprops')
    newest_vsprops_time = max(file_modification_times(vsprops_directory))

    obj_directory = os.path.join(os.environ['WEBKITOUTPUTDIR'], 'obj')
    for manifest_file in glob.iglob(os.path.join(obj_directory, '*', '*', '*.manifest*')):
        manifest_time = os.path.getmtime(manifest_file)
        if manifest_time < newest_vsprops_time:
            os.remove(manifest_file)


if __name__ == '__main__':
    sys.exit(main())
