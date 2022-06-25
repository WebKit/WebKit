#!/bin/sh
#
# Copyright (C) 2018 Apple Inc. All rights reserved.
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

WEB_CONTENT_RESOURCES_PATH="${BUILT_PRODUCTS_DIR}/WebKit.framework/PrivateHeaders/CustomWebContentResources"
mkdir -p "${WEB_CONTENT_RESOURCES_PATH}"

echo "Copying WebContent entitlements from ${WK_PROCESSED_XCENT_FILE} to ${WEB_CONTENT_RESOURCES_PATH}/WebContent.entitlements"
ditto "${WK_PROCESSED_XCENT_FILE}" "${WEB_CONTENT_RESOURCES_PATH}/WebContent.entitlements"

WEBCONTENT_XIB="${SRCROOT}/Resources/WebContentProcess.xib"
echo "Copying WebContentProcess.xib from ${WEBCONTENT_XIB} to ${WEB_CONTENT_RESOURCES_PATH}/WebContentProcess.xib"
ditto "${WEBCONTENT_XIB}" "${WEB_CONTENT_RESOURCES_PATH}/WebContentProcess.xib"

PROCESSED_INFOPLIST="${BUILT_PRODUCTS_DIR}/${INFOPLIST_PATH}"
UNPROCESSED_INFOPLIST="${INFOPLIST_FILE}"
COPIED_INFOPLIST="${WEB_CONTENT_RESOURCES_PATH}/Info-WebContent.plist"
echo "Copying Info.plist from ${UNPROCESSED_INFOPLIST} to ${COPIED_INFOPLIST}"
ditto "${UNPROCESSED_INFOPLIST}" "${COPIED_INFOPLIST}"

echo "Setting fixed entry values for ${COPIED_INFOPLIST}"
if [[ ${WK_PLATFORM_NAME} == "macosx" ]]; then
    FIXED_ENTRIES=( ":XPCService:RunLoopType" )
else
    FIXED_ENTRIES=()
fi

for ((i = 0; i < ${#FIXED_ENTRIES[@]}; ++i)); do
    ENTRY_VALUE=$(/usr/libexec/PlistBuddy -c "Print ${FIXED_ENTRIES[$i]}" "${PROCESSED_INFOPLIST}")
    /usr/libexec/PlistBuddy -c "Set ${FIXED_ENTRIES[$i]} ${ENTRY_VALUE}" "${COPIED_INFOPLIST}"
done
