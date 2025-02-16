#!/usr/bin/env python3
#
# Copyright (C) 2025 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

import argparse
import os
from os import path
import subprocess
import sys

sys.path.append(path.abspath(path.join(path.dirname(__file__), '..', 'Scripts')))

from webkitpy.common.host import Host
from webkitpy.common.webkit_finder import WebKitFinder


def get_default_configuration(output_dir):
    try:
        with open(path.join(output_dir, 'Configuration')) as f:
            return f.read()
    except FileNotFoundError:
        return None


def main():
    parser = argparse.ArgumentParser(description='run-webkit-app runs an executable with a specific build of WebKit')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--release', action='store_const', dest='configuration', const='Release')
    group.add_argument('--debug', action='store_const', dest='configuration', const='Debug')
    parser.add_argument('executable', nargs=argparse.REMAINDER)
    options = parser.parse_args(sys.argv[1:])

    host = Host()
    finder = WebKitFinder(host.filesystem)
    output_dir = finder.path_from_webkit_outputdir()
    subdir = options.configuration or get_default_configuration(output_dir)

    if not os.environ.get('WEBKIT_BUILD_USE_SYSTEM_LIBRARIES', 0):
        sys.exit('ERROR: This command is not supported when using flatpak.')

    if not options.executable:
        parser.print_help()
        print('')
        sys.exit('ERROR: An executable is required.')

    if not subdir:
        sys.exit('ERROR: You must specify either --debug/--release or set it with `set-webkit-configuration`.')

    build_dir = path.join(output_dir, 'GTK', subdir)

    env = os.environ.copy()

    libdir = path.join(build_dir, 'lib')
    env['WEBKIT_EXEC_PATH'] = path.join(build_dir, 'bin')
    env['WEBKIT_INJECTED_BUNDLE_PATH'] = libdir
    if 'LD_LIBRARY_PATH' in env:
        env['LD_LIBRARY_PATH'] = ':'.join((libdir, env['LD_LIBRARY_PATH']))
    else:
        env['LD_LIBRARY_PATH'] = libdir

    proc = subprocess.run(options.executable, env=env)
    sys.exit(proc.returncode)


if __name__ == '__main__':
    main()
