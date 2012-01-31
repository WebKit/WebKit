#!/bin/sh

# This script is needed because adding entitlements through Xcode configuration doesn't get dependencies right in some cases, <rdar://problem/10783446>.

app_path="${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}"
app_binary_path="${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"
entitlement_file=PluginProcess/mac/PluginProcess.entitlements

if [[ ${CONFIGURATION} == "Production" ]]; then
    exit
fi

osx_version=$(sw_vers -productVersion | cut -d. -f 2)
if (( ${osx_version} <= 6 )); then
    exit
fi

needs_signing=0

# If the signature is invalid (e.g. due to updated resources), we need to re-sign it.
codesign --verify "${app_path}" 2> /dev/null
if [[ $? != 0 ]]; then
    needs_signing=1
else
    # If the entitlements file is newer than the binary, we need to re-sign the app.
    if [[ "${entitlement_file}" -nt "${app_binary_path}" ]]; then
        needs_signing=1
    fi
fi

if [[ $needs_signing == 0 ]]; then
    exit
fi

codesign -f -s - --entitlements "${entitlement_file}" "${app_path}"
