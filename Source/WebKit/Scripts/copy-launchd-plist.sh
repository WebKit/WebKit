#!/bin/sh

if [[ "${SKIP_INSTALL}" == "YES" || -z "${LAUNCHD_PLIST_OUTPUT_FILE}" ]]; then
    exit 0;
fi

if [[ -z "${LAUNCHD_PLIST_FILE_NAME}" ]]; then
    echo "Missing definition for LAUNCHD_PLIST_FILE_NAME. e.g. 'com.apple.webkit.thedaemon.plist'"
    exit 1
fi

if [[ -z "${LAUNCHD_PLIST_INPUT_FILE}" ]]; then
    echo "Missing definition for LAUNCHD_PLIST_INPUT_FILE. e.g. 'TheDaemon/Resources/com.apple.webkit.thedaemon.plist'"
    exit 1
fi

# Update the launchd plist to point to the install location of the binary in its actual
# location.  When using the system content path, we need to install there and create a
# symlink if required.  Since the location can change based on the build configuration,
# the launchd plists should contain placeholders for build settings INSTALL_PATH and
# PRODUCT_NAME which we replace with the actual values from the current build configuration.

GENERATED_LAUNCHD_PLIST_DIR="${TEMP_DIR}"
mkdir -p "${GENERATED_LAUNCHD_PLIST_DIR}"

GENERATED_LAUNCHD_PLIST_PATH="${GENERATED_LAUNCHD_PLIST_DIR}/${LAUNCHD_PLIST_FILE_NAME}"

echo "Replacing placeholders in ${SRCROOT}/${LAUNCHD_PLIST_INPUT_FILE} and saving to ${GENERATED_LAUNCHD_PLIST_PATH}"

sed \
    -e "s|\${INSTALL_PATH}|${INSTALL_PATH}|" \
    -e "s|\${PRODUCT_NAME}|${PRODUCT_NAME}|" \
    "${SRCROOT}/${LAUNCHD_PLIST_INPUT_FILE}" > "${GENERATED_LAUNCHD_PLIST_PATH}"

# Install the plist.

LAUNCHD_PLIST_INSTALL_FILE="${LAUNCHD_PLIST_INSTALL_PATH}/${LAUNCHD_PLIST_FILE_NAME}"

mkdir -p "${DSTROOT}${LAUNCHD_PLIST_INSTALL_PATH}"

echo "Converting ${GENERATED_LAUNCHD_PLIST_PATH} to binary plist at ${LAUNCHD_PLIST_OUTPUT_FILE}"
plutil -convert binary1 -o "${LAUNCHD_PLIST_OUTPUT_FILE}" "${GENERATED_LAUNCHD_PLIST_PATH}"
