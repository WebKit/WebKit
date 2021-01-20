#!/usr/bin/env python
# Copyright (C) 2011 Igalia S.L.
# Copyright (C) 2012 Intel Corporation
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

import multiprocessing
import sys
import os
import platform


script_dir = None


def script_path(*args):
    global script_dir
    if not script_dir:
        script_dir = os.path.join(os.path.dirname(__file__), '..', 'Scripts')
    return os.path.join(*(script_dir,) + args)


def top_level_path(*args):
    return os.path.join(*((os.path.join(os.path.dirname(__file__), '..', '..'),) + args))


def init(jhbuildrc_globals, jhbuild_platform):

    __tools_directory = os.path.join(os.path.dirname(__file__), "../", jhbuild_platform)
    sys.path.insert(0, __tools_directory)

    jhbuildrc_globals["build_policy"] = 'updated'

    __moduleset_file_name = 'jhbuild.modules'
    if 'WEBKIT_JHBUILD_MODULESET' in os.environ:
        __moduleset_file_name = 'jhbuild-%s.modules' % os.environ['WEBKIT_JHBUILD_MODULESET']
    __moduleset_file_path = os.path.join(__tools_directory, __moduleset_file_name)
    if not os.path.isfile(__moduleset_file_path):
        raise RuntimeError("Can't find the moduleset in path %s" % __moduleset_file_path)
    __moduleset_file_uri = 'file://' + __moduleset_file_path

    __extra_modulesets = os.environ.get("WEBKIT_EXTRA_MODULESETS", "").split(",")
    jhbuildrc_globals["moduleset"] = [__moduleset_file_uri, ]
    if __extra_modulesets != ['']:
        jhbuildrc_globals["moduleset"].extend(__extra_modulesets)

    __extra_modules = os.environ.get("WEBKIT_EXTRA_MODULES", "").split(",")

    base_dependency_suffix = 'testing'
    if 'WEBKIT_JHBUILD_MODULESET' in os.environ:
        base_dependency_suffix = os.environ['WEBKIT_JHBUILD_MODULESET']

    jhbuildrc_globals["modules"] = ['webkit' + jhbuild_platform + '-' + base_dependency_suffix + '-dependencies', ]
    if __extra_modules != ['']:
        jhbuildrc_globals["modules"].extend(__extra_modules)

    if 'WEBKIT_OUTPUTDIR' in os.environ:
        jhbuildrc_globals["checkoutroot"] = checkoutroot = os.path.abspath(os.path.join(os.environ['WEBKIT_OUTPUTDIR'], 'Dependencies' + jhbuild_platform.upper(), 'Source'))
        jhbuildrc_globals["prefix"] = os.path.abspath(os.path.join(os.environ['WEBKIT_OUTPUTDIR'], 'Dependencies' + jhbuild_platform.upper(), 'Root'))
    else:
        jhbuildrc_globals["checkoutroot"] = checkoutroot = os.path.abspath(top_level_path('WebKitBuild', 'Dependencies' + jhbuild_platform.upper(), 'Source'))
        jhbuildrc_globals["prefix"] = os.path.abspath(top_level_path('WebKitBuild', 'Dependencies' + jhbuild_platform.upper(), 'Root'))

    jhbuildrc_globals["nonotify"] = True
    jhbuildrc_globals["notrayicon"] = True

    if 'NUMBER_OF_PROCESSORS' in os.environ:
        jhbuildrc_globals['jobs'] = os.environ['NUMBER_OF_PROCESSORS']

    if os.environ.get("WEBKIT_JHBUILD_MODULESET") != "minimal":
        # Avoid runtime conflicts with GStreamer system-wide plugins. We want
        # to use only the plugins we build in JHBuild.
        os.environ['GST_PLUGIN_SYSTEM_PATH'] = ''

    addpath = jhbuildrc_globals['addpath']

    prefix = jhbuildrc_globals['prefix']
    addpath('CMAKE_PREFIX_PATH', prefix)
    addpath('CMAKE_LIBRARY_PATH', os.path.join(prefix, 'lib'))

    if 'JHBUILD_MIRROR' in os.environ:
        jhbuildrc_globals['dvcs_mirror_dir'] = os.environ['JHBUILD_MIRROR']
        jhbuildrc_globals['tarballdir'] = os.environ['JHBUILD_MIRROR']

    if 'x86_64' in platform.machine():
        jhbuildrc_globals['conditions'].add('x86_64')

    if 'JHBUILD_ENABLE_THUNDER' in os.environ:
        jhbuild_enable_thunder = os.environ['JHBUILD_ENABLE_THUNDER'].lower()
        if jhbuild_enable_thunder == 'yes' or jhbuild_enable_thunder == '1' or jhbuild_enable_thunder == 'true':
            jhbuildrc_globals['conditions'].add('Thunder')
