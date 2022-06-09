#!/bin/sh
#
# Copyright (c) 2021 Apple Inc. All rights reserved.
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

set -e

if [[ ${WK_PLATFORM_NAME} == "appletvos" || ${WK_PLATFORM_NAME} == "appletvsimulator" ]]; then
    WEBKIT_PLIST_FILE_NAME="WebKit-appletvos.plist"
elif [[ ${WK_PLATFORM_NAME} == "iphoneos" || ${WK_PLATFORM_NAME} == "iphonesimulator" ]]; then
    WEBKIT_PLIST_FILE_NAME="WebKit-ios.plist"
elif [[ ${WK_PLATFORM_NAME} == "macosx" ]]; then
    WEBKIT_PLIST_FILE_NAME="WebKit-macos.plist"
elif [[ ${WK_PLATFORM_NAME} == "watchos" || ${WK_PLATFORM_NAME} == "watchsimulator" ]]; then
    WEBKIT_PLIST_FILE_NAME="WebKit-watchos.plist"
else
    exit 0;
fi

WEBKIT_PLIST_DIR="${SRCROOT}/FeatureFlags"
INTERNAL_WEBKIT_PLIST_DIR="${WK_WEBKITADDITIONS_HEADERS_FOLDER_PATH}/WebKit"
DEST_DIR="${INSTALL_ROOT}/${WK_INSTALL_PATH_PREFIX}/${SYSTEM_LIBRARY_DIR}/FeatureFlags/Domain"

if [[ ! -d "${DEST_DIR}" ]]; then
    mkdir -p "${DEST_DIR}"
fi

if [[ ! -f "${INTERNAL_WEBKIT_PLIST_DIR}/${WEBKIT_PLIST_FILE_NAME}" ]]; then
    echo "Copy ${WEBKIT_PLIST_DIR}/${WEBKIT_PLIST_FILE_NAME} to ${DEST_DIR}/WebKit.plist"
    ditto "${WEBKIT_PLIST_DIR}/${WEBKIT_PLIST_FILE_NAME}" "${DEST_DIR}/WebKit.plist"
else
    echo "Generate ${DEST_DIR}/WebKit.plist"
    python3 "${SRCROOT}/Scripts/combine-feature-flags-plist.py" "${WEBKIT_PLIST_DIR}/${WEBKIT_PLIST_FILE_NAME}" "${INTERNAL_WEBKIT_PLIST_DIR}/${WEBKIT_PLIST_FILE_NAME}" "${DEST_DIR}/WebKit.plist"
fi
