#!/usr/bin/env vpython3

# Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Remove extra namespace qualifications

Looks for names that don't need to be qualified by namespace, and deletes
the qualifier.

Depends on namespace names being properly formatted
"""
import os
import glob
import sys
import re
import argparse


def remove_extra_namespace_from_file(namespace, filename):
    print('Processing namespace', namespace, 'file', filename)
    with open(filename) as file:
        newfile = open(filename + '.NEW', 'w')
        namespaces = []
        changes = 0
        for line in file:
            match = re.match(r'namespace (\S+) {', line)
            if match is not None:
                namespaces.insert(0, match.group(1))
                newfile.write(line)
                continue
            match = re.match(r'}\s+// namespace (\S+)$', line)
            if match is not None:
                if match.group(1) != namespaces[0]:
                    print('Namespace mismatch')
                    raise RuntimeError('Namespace mismatch')
                del namespaces[0]
                newfile.write(line)
                continue
            # Remove namespace usage. Only replacing when target
            # namespace is the innermost namespace.
            if len(namespaces) > 0 and namespaces[0] == namespace:
                # Note that in namespace foo, we match neither ::foo::name
                # nor morefoo::name
                # Neither do we match foo:: when it is not followed by
                # an identifier character.
                usage_re = r'(?<=[^a-z:]){}::(?=[a-zA-Z])'.format(
                    namespaces[0])
                if re.search(usage_re, line):
                    line = re.sub(usage_re, '', line)
                    changes += 1
            newfile.write(line)
    if changes > 0:
        print('Made', changes, 'changes to', filename)
        os.remove(filename)
        os.rename(filename + '.NEW', filename)
    else:
        os.remove(filename + '.NEW')


def remove_extra_namespace_from_files(namespace, files):
    for file in files:
        if os.path.isfile(file):
            if re.search(r'\.(h|cc)$', file):
                remove_extra_namespace_from_file(namespace, file)
        elif os.path.isdir(file):
            if file in ('third_party', 'out'):
                continue
            subfiles = glob.glob(file + '/*')
            remove_extra_namespace_from_files(namespace, subfiles)
        else:
            print(file, 'is not a file or directory, ignoring')


def main():
    parser = argparse.ArgumentParser(
        prog='remove_extra_namespace.py',
        description=__doc__.strip().splitlines()[0],
        epilog=''.join(__doc__.splitlines(True)[1:]),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('--namespace')
    parser.add_argument('files', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    return remove_extra_namespace_from_files(args.namespace, args.files)


if __name__ == '__main__':
    sys.exit(main())
