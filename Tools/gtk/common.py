#!/usr/bin/env python
# Copyright (C) 2011 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import os
import subprocess
import sys

script_dir = None
build_dir = None


def script_path(*args):
    global script_dir
    if not script_dir:
        script_dir = os.path.join(os.path.dirname(__file__), '..', 'Scripts')
    return os.path.join(*(script_dir,) + args)


def top_level_path(*args):
    return os.path.join(*((script_path('..', '..'),) + args))


def get_build_path():
    global build_dir
    if build_dir:
        return build_dir

    def is_valid_build_directory(path):
        return os.path.exists(os.path.join(path, 'GNUmakefile'))

    build_types = ['Release', 'Debug']
    if '--debug' in sys.argv:
        build_types.reverse()

    for build_type in build_types:
        build_dir = top_level_path('WebKitBuild', build_type)
        if is_valid_build_directory(build_dir):
            return build_dir

    # distcheck builds in a directory named _build in the top-level path.
    build_dir = top_level_path("_build")
    if is_valid_build_directory(build_dir):
        return build_dir

    build_dir = top_level_path()
    if is_valid_build_directory(build_dir):
        return build_dir

    build_dir = top_level_path("WebKitBuild")
    if is_valid_build_directory(build_dir):
        return build_dir

    print 'Could not determine build directory.'
    sys.exit(1)


def build_path(*args):
    return os.path.join(*(get_build_path(),) + args)


def number_of_cpus():
    process = subprocess.Popen([script_path('num-cpus')], stdout=subprocess.PIPE)
    stdout = process.communicate()[0]
    return int(stdout)
