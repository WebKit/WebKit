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
    --module jsmodule_util:2 \
        --js Source/WebCore/inspector/front-end/utilities.js \
        --js Source/WebCore/inspector/front-end/treeoutline.js \
    --module jsmodule_host:1 \
        --js Source/WebCore/inspector/front-end/InspectorFrontendHostStub.js \
    --module jsmodule_common:4:jsmodule_util,jsmodule_host \
        --js Source/WebCore/inspector/front-end/BinarySearch.js \
        --js Source/WebCore/inspector/front-end/Object.js \
        --js Source/WebCore/inspector/front-end/Settings.js \
        --js Source/WebCore/inspector/front-end/UserMetrics.js \
    --module jsmodule_sdk:16:jsmodule_common,jsmodule_host \
        --js Source/WebCore/inspector/front-end/ConsoleModel.js \
        --js Source/WebCore/inspector/front-end/ContentProviders.js \
        --js Source/WebCore/inspector/front-end/CookieParser.js \
        --js Source/WebCore/inspector/front-end/BreakpointManager.js \
        --js Source/WebCore/inspector/front-end/DOMAgent.js \
        --js Source/WebCore/inspector/front-end/DebuggerModel.js \
        --js Source/WebCore/inspector/front-end/DebuggerPresentationModel.js \
        --js Source/WebCore/inspector/front-end/Script.js \
        --js Source/WebCore/inspector/front-end/ScriptFormatter.js \
        --js Source/WebCore/inspector/front-end/RawSourceCode.js \
        --js Source/WebCore/inspector/front-end/RemoteObject.js \
        --js Source/WebCore/inspector/front-end/ResourceCategory.js \
        --js Source/WebCore/inspector/front-end/ResourceTreeModel.js \
        --js Source/WebCore/inspector/front-end/Resource.js \
        --js Source/WebCore/inspector/front-end/NetworkManager.js \
        --js Source/WebCore/inspector/front-end/UISourceCode.js \
    --module jsmodule_misc:16:jsmodule_sdk \
        --js Source/WebCore/inspector/front-end/Checkbox.js \
        --js Source/WebCore/inspector/front-end/ContextMenu.js \
        --js Source/WebCore/inspector/front-end/ConsoleMessage.js \
        --js Source/WebCore/inspector/front-end/ConsoleView.js \
        --js Source/WebCore/inspector/front-end/HelpScreen.js \
        --js Source/WebCore/inspector/front-end/KeyboardShortcut.js \
        --js Source/WebCore/inspector/front-end/JavaScriptContextManager.js \
        --js Source/WebCore/inspector/front-end/View.js \
        --js Source/WebCore/inspector/front-end/Placard.js \
        --js Source/WebCore/inspector/front-end/Popover.js \
        --js Source/WebCore/inspector/front-end/SoftContextMenu.js \
        --js Source/WebCore/inspector/front-end/ShortcutsScreen.js \
        --js Source/WebCore/inspector/front-end/StatusBarButton.js \
        --js Source/WebCore/inspector/front-end/TabbedPane.js \
        --js Source/WebCore/inspector/front-end/TextPrompt.js \
        --js Source/WebCore/inspector/front-end/TimelineManager.js
