#!/bin/sh
set -e

program="$(dirname $(dirname $(dirname "${SRCROOT}")))/${WK_ADDITIONAL_SCRIPTS_DIR}/audit-spi"
[ -f "${program}" ] || program="$(dirname $(dirname "${SRCROOT}"))/Tools/Scripts/audit-spi"

# Xcode doesn't expose the name of the discovered dependency file, but by convention, it is
# the same basename as the timestamp output.
depfile="${SCRIPT_OUTPUT_FILE_0/%.timestamp/.d}"

eval allowlists=(${WK_AUDIT_SPI_ALLOWLISTS})

# For compatibility with offline build environments.
export DISABLE_WEBKITCOREPY_AUTOINSTALLER=1

if [[ "${WK_AUDIT_SPI}" == YES && -f "${program}" ]]; then
    mkdir -p "${OBJROOT}/WebKitSDKDBs"

    # WK_SDKDB_DIR is a directory of directories named according to SDK
    # versions. Pick the versioned directory closest to the active SDK, but not
    # greater. If all available directories are for newer SDKs, fall back to
    # the last one.
    for versioned_sdkdb_dir in $(printf '%s\n' ${WK_SDKDB_DIR}/${PLATFORM_NAME}* | sort -rV); do
        # Skip any empty directories that may have been left behind after being
        # removed from the tree due to .DS_Store, etc.
        if [ -n "$(ls "${versioned_sdkdb_dir}")" ] && \
            printf '%s\n' ${versioned_sdkdb_dir#${WK_SDKDB_DIR}/} ${SDK_NAME%.internal} | sort -CV; then
        break
        fi
    done

    for arch in ${ARCHS}; do
        (set -x && "${program}" \
         --sdkdb-dir "${versioned_sdkdb_dir}" \
         --sdkdb-cache "${OBJROOT}/WebKitSDKDBs/${SDK_NAME}.sqlite3" \
         --sdk-dir "${SDKROOT}" --arch-name "${arch}" \
         --depfile "${depfile}" \
         -F "${BUILT_PRODUCTS_DIR}" \
         -L "${BUILT_PRODUCTS_DIR}" \
         ${allowlists[@]/#/--allowlist } \
         ${WK_OTHER_AUDIT_SPI_FLAGS} \
         @"${BUILT_PRODUCTS_DIR}/DerivedSources/${PROJECT_NAME}/platform-enabled-swift-args.${arch}.resp" \
         $@)
     done
else
    [ -f "${program}" ] || echo "audit-spi not available, skipping" >&2
    echo "dependencies: " > "${depfile}"
fi
touch "${SCRIPT_OUTPUT_FILE_0}"
