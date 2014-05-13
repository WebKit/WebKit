#!/usr/bin/env python
# Copyright (C) 2013 Igalia S.L.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

import common
import os
import sys

from ConfigParser import SafeConfigParser


def header_name_list_from_gtkdoc_config_file():
    config_file = common.build_path('gtkdoc-webkitdom.cfg')
    if not os.path.isfile(config_file):
        sys.stderr.write("Could not find config file at %s\n" % config_file)
        return sys.exit(1)

    config = SafeConfigParser()
    config.read(config_file)
    module_name = config.sections()[0]
    return [os.path.basename(f) for f in str(config.get(module_name, 'headers')).replace(';', ' ').split()]


def get_all_webkitdom_symbol_files():
    static_symbol_files_path = common.top_level_path('Source', 'WebCore', 'bindings', 'gobject')
    generated_symbol_files_path = common.build_path('DerivedSources', 'webkitdom')

    symbol_files = []
    for header_name in header_name_list_from_gtkdoc_config_file():
        # webkitdomdefines.h doesn't have a corresponding symbols file and webkitdom.symbols is a
        # file containing the expected symbols results.
        if header_name in ("webkitdom.h", "webkitdomdefines.h"):
            continue

        symbol_file = header_name.replace(".h", ".symbols")
        path = os.path.join(static_symbol_files_path, symbol_file)
        if os.path.exists(path):
            symbol_files.append(path)
            continue
        path = os.path.join(generated_symbol_files_path, symbol_file)
        if os.path.exists(path):
            symbol_files.append(path)
            continue
        sys.stderr.write("Could not find symbol file for header: %s\n" % header_name)
        sys.exit(1)

    return symbol_files
