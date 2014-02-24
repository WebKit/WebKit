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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import common
import glob
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../jhbuild"))
import jhbuildrc_common

resources_path = jhbuildrc_common.top_level_path() + "/Source/WebInspectorUI/"
inspector_files = \
    glob.glob(resources_path + 'UserInterface/*.html') + \
    glob.glob(resources_path + 'UserInterface/Base/*.js') + \
    glob.glob(resources_path + 'UserInterface/Controllers/*.css') + \
    glob.glob(resources_path + 'UserInterface/Controllers/*.js') + \
    glob.glob(resources_path + 'UserInterface/Models/*.js') + \
    glob.glob(resources_path + 'UserInterface/Protocol/*.js') + \
    glob.glob(resources_path + 'UserInterface/Views/*.css') + \
    glob.glob(resources_path + 'UserInterface/Views/*.js') + \
    glob.glob(resources_path + 'UserInterface/Images/*.png') + \
    glob.glob(resources_path + 'UserInterface/Images/*.svg') + \
    glob.glob(resources_path + 'UserInterface/External/CodeMirror/*') + \
    glob.glob(resources_path + 'Localizations/en.lproj/localizedStrings.js')

gresources_file_content = \
"""<?xml version=1.0 encoding=UTF-8?>
<gresources>
    <gresource prefix="/org/webkitgtk/inspector">
"""

for file in inspector_files:
    gresources_file_content += "        <file>" + file.replace(resources_path, '') + "</file>\n"

gresources_file_content += \
"""    </gresource>
</gresources>
"""

open(sys.argv[1], 'w').write(gresources_file_content)
