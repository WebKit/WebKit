#!/usr/bin/python3

# WebKit builds ANGLE as a static library, and exports some of the
# internal header files as "public headers" in the Xcode project for
# consumption by other build targets - e.g. WebCore.
#
# The build phase which copies these headers also flattens the
# directory structure (so that "ANGLE" is the top-level directory
# containing all of them - e.g., "#include <ANGLE/gl2.h>").
#
# It isn't practical to override the include paths so drastically for
# the other build targets (like WebCore) that we could make the
# original include paths, like <GLES2/gl2.h> work. Changing them so
# their namespace is "ANGLE", which implicitly occurs during the "copy
# headers" phase, is a reasonable solution.
#
# This script processes the header files after they're copied during
# the Copy Header Files build phase, and adjusts their #includes so
# that they refer to each other. This avoids modifying the ANGLE
# sources, and allows WebCore to more easily call ANGLE APIs directly.

import os
import re

if os.getenv('DEPLOYMENT_LOCATION') == 'YES':
    # Apple-internal build
    output_dir = os.getenv('DSTROOT') + os.getenv('PUBLIC_HEADERS_FOLDER_PATH')
else:
    # External build
    output_dir = os.getenv('BUILT_PRODUCTS_DIR') + os.getenv('PUBLIC_HEADERS_FOLDER_PATH')


def replace(line):
    match = re.match('#include [<"](.*)[>"]', line)
    if match:
        header = match.group(1)
        match = re.match(r'(?:\.\./)*(EGL|GLES[23]?|KHR)/(.*)', header)
        if match:
            return "#include <ANGLE/" + match.group(2) + ">\n"
        match = re.match(r'(eglext_angle|gl2ext_angle|ShaderVars)\.h', header)
        if match:
            return "#include <ANGLE/" + match.group(1) + ".h>\n"
        if header == 'export.h':
            return "#include <ANGLE/export.h>\n"

    return line


os.chdir(output_dir)
for filename in os.listdir('.'):
    if not os.path.isfile(filename):
        continue
    if not (filename.endswith('.h') or filename.endswith('.inc')):
        continue
    lines = open(filename, 'r').readlines()
    newLines = [replace(line) for line in lines]
    if lines != newLines:
        open(filename, 'w').writelines(newLines)
