#!/bin/sh

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

if [ "$DEPLOYMENT_LOCATION" == "YES" ] ; then
    # Apple-internal build.
    output_dir=${DSTROOT}${PUBLIC_HEADERS_FOLDER_PATH}
else
    # External build.
    output_dir=${BUILT_PRODUCTS_DIR}${PUBLIC_HEADERS_FOLDER_PATH}
fi

if [ $(uname) == "Linux" ]; then
    inplace_opt=(-i)
else
    inplace_opt=(-i "")
fi

for i in $output_dir/*.h ; do
    if [ ! -f $output_dir/angle.timestamp ] || [ $i -nt $output_dir/angle.timestamp ] ; then
        sed -e '
s/^#include [<"]EGL\/\(.*\)[>"]/#include <ANGLE\/\1>/
s/^#include [<"]GLES\/\(.*\)[>"]/#include <ANGLE\/\1>/
s/^#include [<"]GLES2\/\(.*\)[>"]/#include <ANGLE\/\1>/
s/^#include [<"]GLES3\/\(.*\)[>"]/#include <ANGLE\/\1>/
s/^#include [<"]KHR\/\(.*\)[>"]/#include <ANGLE\/\1>/
s/^#include [<"]export.h[>"]/#include <ANGLE\/export.h>/
s/^#include "\(eglext_angle\|gl2ext_angle\|ShaderVars\).h"/#include <ANGLE\/\1.h>/
' "${inplace_opt[@]}" $i
        echo Postprocessed ANGLE header `basename $i`
    fi
done

date > $output_dir/angle.timestamp
