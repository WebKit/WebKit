#!/bin/sh
#
# Copyright (C) 2014 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#

if [[ "${WK_FRAMEWORK_HEADER_POSTPROCESSING_DISABLED}" == "YES" ]]; then
    exit 0;
fi

TIMESTAMP_PATH=${TARGET_TEMP_DIR}/${0##*/}

if [[ -e $TIMESTAMP_PATH && $0 -nt $TIMESTAMP_PATH ]]; then
    rm "${TIMESTAMP_PATH}";
fi

function process_definitions () {
    local DEFINITIONS_FILE=$1

    if [[ ! -f "${DEFINITIONS_FILE}" ]]; then
        return 1
    fi

    if [[ -e $TIMESTAMP_PATH && "${DEFINITIONS_FILE}" -nt $TIMESTAMP_PATH ]]; then
        rm "${TIMESTAMP_PATH}";
    fi

    source "${DEFINITIONS_FILE}"
}

function rewrite_headers () {
    if [[ "${WK_PLATFORM_NAME}" == "macosx" ]]; then
        [[ -n ${OSX_VERSION} ]] || OSX_VERSION=${MACOSX_DEPLOYMENT_TARGET}
        [[ -n ${IOS_VERSION} ]] || IOS_VERSION="NA"
    elif [[ "${WK_PLATFORM_NAME}" =~ "iphone" ]]; then
        [[ -n ${IOS_VERSION} ]] || IOS_VERSION=${IPHONEOS_DEPLOYMENT_TARGET}
        [[ -n ${OSX_VERSION} ]] || OSX_VERSION="NA"
    fi

    SED_OPTIONS=(
    )

    if [[ -n "$OSX_VERSION" && -n "$IOS_VERSION" ]]; then
        SED_OPTIONS+=(
            -e s/WK_MAC_TBA/${OSX_VERSION}/g
            -e s/WK_IOS_TBA/${IOS_VERSION}/g
            -e s/WK_API_AVAILABLE/API_AVAILABLE/
            -e s/WK_API_DEPRECATED/API_DEPRECATED/
            -e "s/^WK_CLASS_AVAILABLE/WK_EXTERN API_AVAILABLE/"
            -e "s/^WK_CLASS_DEPRECATED/WK_EXTERN API_DEPRECATED/"
        )
    else
        SED_OPTIONS+=(
            -e 's/WK_(API_|CLASS_)AVAILABLE\(.*\)\s*\)//g'
            -e 's/WK_(API_|CLASS_)DEPRECATED(_WITH_REPLACEMENT)?\(.*\)\s*\)//g'
        )
    fi

    SED_OPTIONS+=(${OTHER_SED_OPTIONS[*]})

    for HEADER_PATH in "${1}/"*.h; do
        if [[ "$HEADER_PATH" -nt $TIMESTAMP_PATH ]]; then
            ditto "${HEADER_PATH}" "${TARGET_TEMP_DIR}/${HEADER_PATH##*/}"
            sed -i .tmp -E "${SED_OPTIONS[@]}" "${TARGET_TEMP_DIR}/${HEADER_PATH##*/}" || exit $?
            mv "${TARGET_TEMP_DIR}/${HEADER_PATH##*/}" "$HEADER_PATH"
        fi
    done
}

DEFINITIONS_PATH=usr/local/include/WebKitAdditions/Scripts/postprocess-framework-headers-definitions

process_definitions "${BUILT_PRODUCTS_DIR}/${DEFINITIONS_PATH}" || process_definitions "${SDKROOT}/${DEFINITIONS_PATH}"

rewrite_headers "${TARGET_BUILD_DIR}/${PUBLIC_HEADERS_FOLDER_PATH}"
rewrite_headers "${TARGET_BUILD_DIR}/${PRIVATE_HEADERS_FOLDER_PATH}"

touch ${TIMESTAMP_PATH}
