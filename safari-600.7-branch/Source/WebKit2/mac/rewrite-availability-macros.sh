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

if [[ ${WK_API_AVAILABILITY_ENABLED} == "NO" ]]; then
    exit 0;
fi

TIMESTAMP_PATH=${TARGET_TEMP_DIR}/${0##*/}

if [[ -e $TIMESTAMP_PATH && $0 -nt $TIMESTAMP_PATH ]]; then
    rm "${TIMESTAMP_PATH}";
fi

function rewrite_headers () {
    for HEADER_PATH in $1/*.h; do
        if [[ $HEADER_PATH -nt $TIMESTAMP_PATH ]]; then
            sed -e s/^WK_CLASS_AVAILABLE/NS_CLASS_AVAILABLE/ -e s/WK_AVAILABLE/NS_AVAILABLE/ -e s/WK_DESIGNATED_INITIALIZER/NS_DESIGNATED_INITIALIZER/ -e s/WK_ENUM_AVAILABLE/NS_ENUM_AVAILABLE/ -e s/WK_UNAVAILABLE/NS_UNAVAILABLE/ ${HEADER_PATH} > ${TARGET_TEMP_DIR}/${HEADER_PATH##*/} || exit $_;
            mv ${TARGET_TEMP_DIR}/${HEADER_PATH##*/} $HEADER_PATH;
        fi
    done
}

rewrite_headers ${TARGET_BUILD_DIR}/${PUBLIC_HEADERS_FOLDER_PATH}
rewrite_headers ${TARGET_BUILD_DIR}/${PRIVATE_HEADERS_FOLDER_PATH}

touch ${TIMESTAMP_PATH}
