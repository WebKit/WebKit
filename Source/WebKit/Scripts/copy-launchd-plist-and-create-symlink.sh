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

# Evaluate whether a symlink to the plist is needed, returning early if not.

if [[ "${USE_SYSTEM_CONTENT_PATH}" == "YES" ]]; then
    if [[ "${ENABLE_DAEMON_SYMLINKS}" != "YES" ]]; then
        echo "Not creating symlink to plist because build enables USE_SYSTEM_CONTENT_PATH but ENABLE_DAEMON_SYMLINKS is disabled."
        exit 0;
    fi
else
    if [[ ${PLATFORM_NAME} != "macosx" || "${USE_STAGING_INSTALL_PATH}" == "YES" ]]; then
        echo "Not creating symlink because current platform is not macOS or this isn't a standard install."
        exit 0;
    fi
fi

# If we've gotten this far, install a symlink in the standard LaunchDaemon/Agent directory
# pointing to the real install location of the plist.

DSTROOT_LAUNCHD_PLIST_SYMLINK_DIR="${DSTROOT}${LAUNCHD_PLIST_SYMLINK_PATH}"

# Convert eg. `/System/Library/LaunchAgents` to `../../..`
RELATIVE_PATH_FROM_SYMLINK_TO_ROOT=$(echo "${LAUNCHD_PLIST_SYMLINK_PATH}" | sed -E -e "s/\/[a-zA-Z0-9_]+/..\//g" -e "s/\/$//")

echo "Creating a symlink at ${DSTROOT_LAUNCHD_PLIST_SYMLINK_DIR} pointing to ${RELATIVE_PATH_FROM_SYMLINK_TO_ROOT}${LAUNCHD_PLIST_INSTALL_FILE}"

mkdir -p "${DSTROOT_LAUNCHD_PLIST_SYMLINK_DIR}"
ln -sf "${RELATIVE_PATH_FROM_SYMLINK_TO_ROOT}${LAUNCHD_PLIST_INSTALL_FILE}" "${DSTROOT_LAUNCHD_PLIST_SYMLINK_DIR}"

