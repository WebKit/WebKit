#!/usr/bin/env python
# Copyright (C) 2011 Igalia S.L.
# Copyright (C) 2012 Intel Corporation
# Copyright (C) 2012, 2013 Nokia Corporation and/or its subsidiary(-ies).
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

import multiprocessing
import sys
import os

script_dir = None


def script_path(*args):
    global script_dir
    if not script_dir:
        script_dir = os.path.join(os.path.dirname(__file__), '..', 'Scripts')
    return os.path.join(*(script_dir,) + args)


def top_level_path(*args):
    return os.path.join(*((os.path.join(os.path.dirname(__file__), '..', '..'),) + args))


def init(jhbuildrc_globals, platform):

    __tools_directory = os.path.join(os.path.dirname(__file__), "../", platform)
    sys.path.insert(0, __tools_directory)

    jhbuildrc_globals["build_policy"] = 'updated'

    __moduleset_file_uri = 'file://' + os.path.join(__tools_directory, 'jhbuild.modules')
    __extra_modulesets = os.environ.get("WEBKIT_EXTRA_MODULESETS", "").split(",")
    jhbuildrc_globals["moduleset"] = [__moduleset_file_uri, ]
    if __extra_modulesets != ['']:
        jhbuildrc_globals["moduleset"].extend(__extra_modulesets)

    __extra_modules = os.environ.get("WEBKIT_EXTRA_MODULES", "").split(",")
    jhbuildrc_globals["modules"] = ['webkit' + platform + '-testing-dependencies', ]
    if __extra_modules != ['']:
        jhbuildrc_globals["modules"].extend(__extra_modules)

    if 'WEBKIT_OUTPUTDIR' in os.environ:
        jhbuildrc_globals["checkoutroot"] = checkoutroot = os.path.abspath(os.path.join(os.environ['WEBKIT_OUTPUTDIR'], 'Dependencies', 'Source'))
        jhbuildrc_globals["prefix"] = os.path.abspath(os.path.join(os.environ['WEBKIT_OUTPUTDIR'], 'Dependencies', 'Root'))
    else:
        jhbuildrc_globals["checkoutroot"] = checkoutroot = os.path.abspath(top_level_path('WebKitBuild', 'Dependencies', 'Source'))
        jhbuildrc_globals["prefix"] = os.path.abspath(top_level_path('WebKitBuild', 'Dependencies', 'Root'))

    jhbuildrc_globals["nonotify"] = True
    jhbuildrc_globals["notrayicon"] = True
