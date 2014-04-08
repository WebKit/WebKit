#!/usr/bin/bash

# Copyright (C) 2014 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple puter, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

XSRCROOT="`pwd`/.."
XSRCROOT=`realpath "$XSRCROOT"`
export XSRCROOT
export SRCROOT=$XSRCROOT

XDSTROOT="$1"
export XDSTROOT

export TARGET_BUILD_DIR="$XDSTROOT/bin${4}/WebKit.resources"
export JAVASCRIPTCORE_PRIVATE_HEADERS_DIR="$XDSTROOT/obj${4}/JavaScriptCore/DerivedSources"
export WEBCORE_PRIVATE_HEADERS_DIR="$XDSTROOT/obj${4}/WebCore/DerivedSources"
export DERIVED_SOURCES_DIR="$XDSTROOT/obj${4}/WebInspectorUI/DerivedSources"

export UNLOCALIZED_RESOURCES_FOLDER_PATH="WebInspectorUI"

if [[ ${TARGET_BUILD_DIR} =~ "Release" ]] || [[ ${TARGET_BUILD_DIR} =~ "Production" ]]; then
    export COMBINE_INSPECTOR_RESOURCES="YES";
fi

perl "${SRCROOT}/Scripts/copy-user-interface-resources.pl"
