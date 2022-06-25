#!/bin/sh

set -e

if [[ "${DEPLOYMENT_LOCATION}" != "YES" ]]; then
    echo "error: $(basename $0) should only be run for deployment-style (\"production\" / \"non-engineering\") builds."
    exit 1
fi

if [[ "${USE_SYSTEM_CONTENT_PATH}" != "YES" || "${COPY_STAGED_FRAMEWORKS_TO_SECONDARY_PATH}" != "YES" ]]; then
    echo "Skipping copying frameworks to secondary content path."
    exit 0
fi

if [[ "${ACTION}" == "installapi" || "${ACTION}" == "installhdrs" ]]; then
    echo "Skipping copying frameworks to secondary content path for installapi or installhdrs action."
    exit 0
fi

if [[ -z "${FRAMEWORKS_TO_COPY_TO_SECONDARY_PATH}" && -z "${DYLIBS_TO_COPY_TO_SECONDARY_PATH}" ]]; then
    echo "warning: Neither FRAMEWORKS_TO_COPY_TO_SECONDARY_PATH or DYLIBS_TO_COPY_TO_SECONDARY_PATH are set."
fi

if [[ -z "${SECONDARY_STAGED_FRAMEWORK_DIRECTORY}" ]]; then
    echo "error: SECONDARY_STAGED_FRAMEWORK_DIRECTORY is not set."
    exit 1
fi

function copy_product_to_destination_framework_directory
{
    PRODUCT_COMPONENT=$1

    # In deployment builds, BUILT_PRODUCTS_DIR doesn't point to the actual location of any products,
    # but to a single folder of symlinks to all built products, regardless of whether they are actually
    # being installed. We need to copy the actual products to the secondary content path, *not* just a symlink.
    SOURCE_PRODUCT_PATH="$(readlink "${BUILT_PRODUCTS_DIR}/${PRODUCT_COMPONENT}")"

    echo "Copying ${SOURCE_PRODUCT_PATH} to ${DESTINATION_FRAMEWORK_DIRECTORY}"
    rsync -aE "${SOURCE_PRODUCT_PATH}" "${DESTINATION_FRAMEWORK_DIRECTORY}"
}

if [[ ! -z "${SECONDARY_STAGED_FRAMEWORK_DIRECTORY}" ]]; then
    DESTINATION_FRAMEWORK_DIRECTORY="${DSTROOT}${SECONDARY_STAGED_FRAMEWORK_DIRECTORY}"
    mkdir -p "${DESTINATION_FRAMEWORK_DIRECTORY}"

    for FRAMEWORK_NAME in $(echo ${FRAMEWORKS_TO_COPY_TO_SECONDARY_PATH} | tr ' ' '\n')
    do
        FRAMEWORK_BUNDLE_NAME="${FRAMEWORK_NAME}.framework"

        copy_product_to_destination_framework_directory ${FRAMEWORK_BUNDLE_NAME}
    done
fi

if [[ ! -z "${DYLIBS_TO_COPY_TO_SECONDARY_PATH}" ]]; then
    DESTINATION_FRAMEWORK_DIRECTORY="${DSTROOT}${SECONDARY_STAGED_FRAMEWORK_DIRECTORY}${STAGED_DYLIB_FRAMEWORK_PATH}"
    mkdir -p "${DESTINATION_FRAMEWORK_DIRECTORY}"

    for DYLIB in $(echo ${DYLIBS_TO_COPY_TO_SECONDARY_PATH} | tr ' ' '\n')
    do
        copy_product_to_destination_framework_directory ${DYLIB}
    done
fi
