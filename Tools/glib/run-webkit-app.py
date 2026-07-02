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

from webkitpy.port.linux_container_sdk_utils import maybe_enter_webkit_container_sdk  # noqa: E402
maybe_enter_webkit_container_sdk()

from webkitpy.common.host import Host  # noqa: E402
from webkitpy.common.webkit_finder import WebKitFinder  # noqa: E402


def get_default_configuration(output_dir):
    try:
        with open(path.join(output_dir, 'Configuration')) as f:
            return f.read()
    except FileNotFoundError:
        return None


def prepend_path_var(env, name, value):
    if name in env:
        env[name] = f'{value}:{env[name]}'
    else:
        env[name] = value


def main():
    parser = argparse.ArgumentParser(description='run-webkit-app runs an executable with a specific build of WebKit')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--release', action='store_const', dest='configuration', const='Release')
    group.add_argument('--debug', action='store_const', dest='configuration', const='Debug')
    parser.add_argument('--wpe', action='store_true')
    parser.add_argument('executable', nargs=argparse.REMAINDER)
    options = parser.parse_args(sys.argv[1:])

    host = Host()
    finder = WebKitFinder(host.filesystem)
    output_dir = finder.path_from_webkit_outputdir()
    subdir = options.configuration or get_default_configuration(output_dir)

    if not options.executable:
        parser.print_help()
        print('')
        sys.exit('ERROR: An executable is required.')

    if not subdir:
        sys.exit('ERROR: You must specify either --debug/--release or set it with `set-webkit-configuration`.')

    build_dir = path.join(output_dir, 'WPE' if options.wpe else 'GTK', subdir)

    env = os.environ.copy()

    libdir = path.join(build_dir, 'lib')
    env['WEBKIT_EXEC_PATH'] = path.join(build_dir, 'bin')
    env['WEBKIT_INJECTED_BUNDLE_PATH'] = libdir
    prepend_path_var(env, 'LD_LIBRARY_PATH', libdir)
    prepend_path_var(env, 'GI_TYPELIB_PATH', build_dir)

    proc = subprocess.run(options.executable, env=env)
    sys.exit(proc.returncode)


if __name__ == '__main__':
    main()
