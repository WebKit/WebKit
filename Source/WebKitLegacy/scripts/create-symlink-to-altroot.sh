#!/bin/sh

set -e

echo "ATTEMPTING SYMLINK for WebKitLegacy"
echo "ALTERNATE_ROOT_PATH = ${ALTERNATE_ROOT_PATH}"
echo "OUTPUT_ALTERNATE_ROOT_PATH = ${OUTPUT_ALTERNATE_ROOT_PATH}"
echo "DSTROOT = ${DSTROOT}"
echo "INSTALL_PATH = ${INSTALL_PATH}"

if [[ -z "${OUTPUT_ALTERNATE_ROOT_PATH}" ]]; then
    exit 0
fi

if [[ "${SKIP_INSTALL}" = "YES" ]]; then
    exit 0
fi

# Convert eg. `/System/Library/PrivateFrameworks` to `../../..`
RELATIVE_PATH_FROM_SYMLINK_TO_ROOT=$(echo "${ALTERNATE_ROOT_PATH}" | sed -E -e "s/\/[a-zA-Z0-9_]+/..\//g" -e "s/\/$//")
SYMLINK_VALUE="${RELATIVE_PATH_FROM_SYMLINK_TO_ROOT}${INSTALL_PATH}/${FULL_PRODUCT_NAME}"

if [[ -L "${OUTPUT_ALTERNATE_ROOT_PATH}" ]]; then
    EXISTING_SYMLINK_VALUE=$(readlink "${OUTPUT_ALTERNATE_ROOT_PATH}")

    if [[ "${EXISTING_SYMLINK_VALUE}" == "${SYMLINK_VALUE}" ]]; then
        exit 0
    fi

    echo "warning: existing symlink is incorrect; expected ${SYMLINK_VALUE}, got ${EXISTING_SYMLINK_VALUE}"
elif [[ -e "${OUTPUT_ALTERNATE_ROOT_PATH}" ]]; then
    echo "error: expected a symlink at ${OUTPUT_ALTERNATE_ROOT_PATH}"
    exit 1
fi

echo "RELATIVE_PATH_FROM_SYMLINK_TO_ROOT = ${RELATIVE_PATH_FROM_SYMLINK_TO_ROOT}"
echo "SYMLINK_VALUE = ${SYMLINK_VALUE}"
echo

echo ln -sf "${SYMLINK_VALUE}" "${OUTPUT_ALTERNATE_ROOT_PATH}"
ln -sf "${SYMLINK_VALUE}" "${OUTPUT_ALTERNATE_ROOT_PATH}"
