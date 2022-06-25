#!/bin/sh

set -e

if [[ "${DEPLOYMENT_LOCATION}" != "YES" ]]; then
    echo "error: $(basename $0) should only be run for deployment-style (\"production\" / \"non-engineering\") builds."
    exit 1
fi

if [[ "${USE_SYSTEM_CONTENT_PATH}" != "YES" || "${COPY_STAGED_FRAMEWORKS_TO_SECONDARY_PATH}" != "YES" ]]; then
    echo "Skipping copying XPC services to secondary content path."
    exit 0
fi

if [[ "${ACTION}" == "installapi" || "${ACTION}" == "installhdrs" ]]; then
    echo "Skipping copying frameworks to secondary content path for installapi or installhdrs action."
    exit 0
fi

if [[ -z "${XPC_BUNDLES_TO_COPY_TO_SECONDARY_PATH}" ]]; then
    echo "warning: XPC_BUNDLES_TO_COPY_TO_SECONDARY_PATH is not set."
fi

if [[ -z "${XPC_BUNDLE_INSTALL_PATH_IN_SECONDARY_PATH}" ]]; then
    echo "error: XPC_BUNDLE_INSTALL_PATH_IN_SECONDARY_PATH is not set."
    exit 1
fi

if [[ -z "${SECONDARY_STAGED_FRAMEWORK_DIRECTORY}" ]]; then
    echo "error: SECONDARY_STAGED_FRAMEWORK_DIRECTORY is not set."
    exit 1
fi

UPDATE_DYLD_LOAD_COMMAND="$(dirname $0)/update-dyld-environment-load-command"

for XPC_BUNDLE_NAME in $(echo ${XPC_BUNDLES_TO_COPY_TO_SECONDARY_PATH} | tr ' ' '\n')
do
    # In deployment builds, BUILT_PRODUCTS_DIR doesn't point to the actual location of any products,
    # but to a single folder of symlinks to all built products, regardless of whether they are actually
    # being installed. We need to copy the actual products to the secondary path, *not* just a symlink.
    SOURCE_BUNDLE_PATH="$(readlink "${BUILT_PRODUCTS_DIR}/${XPC_BUNDLE_NAME}")"

    DESTINATION_XPC_BUNDLE_DIRECTORY="${DSTROOT}${XPC_BUNDLE_INSTALL_PATH_IN_SECONDARY_PATH}"

    mkdir -p "${DESTINATION_XPC_BUNDLE_DIRECTORY}"

    echo "Copying ${SOURCE_BUNDLE_PATH} to ${DESTINATION_XPC_BUNDLE_DIRECTORY}"
    rsync -aE "${SOURCE_BUNDLE_PATH}" "${DESTINATION_XPC_BUNDLE_DIRECTORY}"

    DESTINATION_XPC_BUNDLE_PATH="${DESTINATION_XPC_BUNDLE_DIRECTORY}/${XPC_BUNDLE_NAME}"
    XPC_EXECUTABLE_PATH="${DESTINATION_XPC_BUNDLE_PATH}/Contents/MacOS/$(basename "${DESTINATION_XPC_BUNDLE_PATH}" | sed -E "s/\\.[^\.]+$//")"

    echo "Updating DYLD environment variables in ${XPC_EXECUTABLE_PATH}"
    "${UPDATE_DYLD_LOAD_COMMAND}" "${XPC_EXECUTABLE_PATH}" "DYLD_VERSIONED_FRAMEWORK_PATH=${SECONDARY_STAGED_FRAMEWORK_DIRECTORY}"

    echo "Re-signing ${DESTINATION_XPC_BUNDLE_PATH}"
    codesign --sign "${CODE_SIGN_IDENTITY}" --force --preserve-metadata=entitlements,flags,identifier,requirements,runtime "${DESTINATION_XPC_BUNDLE_PATH}"

done
