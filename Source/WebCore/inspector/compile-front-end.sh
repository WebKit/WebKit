#!/bin/sh
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Compiles WebKit Web Inspector front-end.

python Source/WebCore/inspector/generate-protocol-externs -o Source/WebCore/inspector/front-end/protocol-externs.js Source/WebCore/inspector/Inspector.json

java -jar ~/closure/compiler.jar --compilation_level SIMPLE_OPTIMIZATIONS --warning_level VERBOSE --language_in ECMASCRIPT5 --accept_const_keyword \
    --externs Source/WebCore/inspector/front-end/externs.js \
    --externs Source/WebCore/inspector/front-end/protocol-externs.js \
    --module jsmodule_core:2 \
        --js Source/WebCore/inspector/front-end/utilities.js \
        --js Source/WebCore/inspector/front-end/treeoutline.js \
    --module jsmodule_env:3 \
        --js Source/WebCore/inspector/front-end/Settings.js \
        --js Source/WebCore/inspector/front-end/UserMetrics.js \
        --js Source/WebCore/inspector/front-end/InspectorFrontendHostStub.js \
    --module jsmodule_sdk:4:jsmodule_core,jsmodule_env \
        --js Source/WebCore/inspector/front-end/Object.js \
        --js Source/WebCore/inspector/front-end/DOMAgent.js \
        --js Source/WebCore/inspector/front-end/Script.js \
        --js Source/WebCore/inspector/front-end/DebuggerModel.js \
    --module jsmodule_ui:5:jsmodule_sdk \
        --js Source/WebCore/inspector/front-end/View.js \
        --js Source/WebCore/inspector/front-end/Placard.js \
        --js Source/WebCore/inspector/front-end/Popover.js \
        --js Source/WebCore/inspector/front-end/ContextMenu.js \
        --js Source/WebCore/inspector/front-end/SoftContextMenu.js
