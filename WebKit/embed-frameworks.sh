#!/bin/sh

framework_name=WebKit.framework
framework_dir="${SYMROOT}/${framework_name}/Versions/A/Frameworks"


if [ -d "${BUILT_PRODUCTS_DIR}/JavaScriptCore.framework" ]; then
    ditto "${BUILT_PRODUCTS_DIR}/JavaScriptCore.framework" "${framework_dir}/JavaScriptCore.framework"
    ditto "${BUILT_PRODUCTS_DIR}/WebCore.framework" "${framework_dir}/WebCore.framework"
fi

# When we copy files from the BuildRoot into DSTROOT, we sometimes pick up
# huge turds from ditto.  For now, just strip these out after the fact.
find -X "${framework_dir}" -name '*.dittoKeptBinary.*' -print0 | xargs -0 rm -f

