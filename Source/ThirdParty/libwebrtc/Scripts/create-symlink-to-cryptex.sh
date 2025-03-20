#!/bin/sh

set -e

if [[ "${SKIP_INSTALL}" == "YES" || "${USE_STAGING_INSTALL_PATH}" == "YES" ]]; then
    exit 0
fi

if [[ -z "${SYSTEM_PUBLIC_HEADER_PREFIX}" || -z "${HEADER_FOLDER_LIST}" || -z "${SYSTEM_PUBLIC_LIBRARY_PREFIX}" || -z "${LIBRARY_LIST}" ]]; then
    exit 0
fi

create_symlink_if_needed()
{
    INSTALL_FOLDER_PREFIX=$1
    PUBLIC_FOLDER_PREFIX=$2
    SYSTEM_FOLDER_PREFIX=$3
    TARGET=$4

    SYSTEM_HEADER_PATH="${SYSTEM_FOLDER_PREFIX}/${TARGET}"

    # Convert eg. `/System/Library/PrivateFrameworks` to `../../..`
    RELATIVE_PATH_FROM_SYMLINK_TO_ROOT=$(echo "${PUBLIC_FOLDER_PREFIX}" | sed -E -e "s/\/[a-zA-Z0-9_]+/..\//g" -e "s/\/$//")
    SYMLINK_VALUE="${RELATIVE_PATH_FROM_SYMLINK_TO_ROOT}${INSTALL_FOLDER_PREFIX}/${TARGET}"

    if [[ -L "${SYSTEM_HEADER_PATH}" ]]; then
        EXISTING_SYMLINK_VALUE=$(readlink "${SYSTEM_HEADER_PATH}")

        if [[ "${EXISTING_SYMLINK_VALUE}" == "${SYMLINK_VALUE}" ]]; then
            exit 0
        fi

        echo "warning: existing symlink is incorrect; expected ${SYMLINK_VALUE}, got ${EXISTING_SYMLINK_VALUE}"
    elif [[ -e "${SYSTEM_HEADER_PATH}" ]]; then
        echo "error: expected a symlink at ${SYSTEM_HEADER_PATH}"
        exit 1
    fi

    ln -sf "${SYMLINK_VALUE}" "${SYSTEM_HEADER_PATH}"
}

if [[ "${ACTION}" == "installhdrs" || "${ACTION}" == "install" ]]
then
    if [[ ! -d ${SYSTEM_PUBLIC_HEADER_PREFIX} ]]; then
        mkdir -p ${SYSTEM_PUBLIC_HEADER_PREFIX}
    fi

    for HEADER_FOLDER in `echo ${HEADER_FOLDER_LIST} | cut -d' ' -f1-`
    do
        create_symlink_if_needed ${INSTALL_PUBLIC_HEADER_PREFIX} ${PUBLIC_HEADERS_FOLDER_PREFIX} ${SYSTEM_PUBLIC_HEADER_PREFIX} ${HEADER_FOLDER}
    done
fi

if [[ "${ACTION}" == "install" ]]
then
    if [[ ! -d ${SYSTEM_PUBLIC_LIBRARY_PREFIX} ]]; then
        mkdir -p ${SYSTEM_PUBLIC_LIBRARY_PREFIX}
    fi

    for LIBRARY in `echo ${LIBRARY_LIST} | cut -d' ' -f1-`
    do
        create_symlink_if_needed ${INSTALL_PUBLIC_LIBRARY_PREFIX} ${PUBLIC_LIBRARY_FOLDER_PREFIX} ${SYSTEM_PUBLIC_LIBRARY_PREFIX} ${LIBRARY}
    done
fi
