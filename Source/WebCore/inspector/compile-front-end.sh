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
    --module jsmodule_common:6:jsmodule_util,jsmodule_host \
        --js Source/WebCore/inspector/front-end/BinarySearch.js \
        --js Source/WebCore/inspector/front-end/Object.js \
        --js Source/WebCore/inspector/front-end/PartialQuickSort.js \
        --js Source/WebCore/inspector/front-end/Settings.js \
        --js Source/WebCore/inspector/front-end/UserMetrics.js \
        --js Source/WebCore/inspector/front-end/HandlerRegistry.js \
    --module jsmodule_sdk:24:jsmodule_common \
        --js Source/WebCore/inspector/front-end/CompilerSourceMapping.js \
        --js Source/WebCore/inspector/front-end/CompilerSourceMappingProvider.js \
        --js Source/WebCore/inspector/front-end/ConsoleModel.js \
        --js Source/WebCore/inspector/front-end/ContentProviders.js \
        --js Source/WebCore/inspector/front-end/CookieParser.js \
        --js Source/WebCore/inspector/front-end/CSSCompletions.js \
        --js Source/WebCore/inspector/front-end/CSSKeywordCompletions.js \
        --js Source/WebCore/inspector/front-end/CSSStyleModel.js \
        --js Source/WebCore/inspector/front-end/BreakpointManager.js \
        --js Source/WebCore/inspector/front-end/Database.js \
        --js Source/WebCore/inspector/front-end/DOMAgent.js \
        --js Source/WebCore/inspector/front-end/DOMStorage.js \
        --js Source/WebCore/inspector/front-end/DebuggerModel.js \
        --js Source/WebCore/inspector/front-end/DebuggerPresentationModel.js \
        --js Source/WebCore/inspector/front-end/HAREntry.js \
        --js Source/WebCore/inspector/front-end/Script.js \
        --js Source/WebCore/inspector/front-end/ScriptFormatter.js \
        --js Source/WebCore/inspector/front-end/RawSourceCode.js \
        --js Source/WebCore/inspector/front-end/RemoteObject.js \
        --js Source/WebCore/inspector/front-end/ResourceCategory.js \
        --js Source/WebCore/inspector/front-end/ResourceTreeModel.js \
        --js Source/WebCore/inspector/front-end/Resource.js \
        --js Source/WebCore/inspector/front-end/NetworkManager.js \
        --js Source/WebCore/inspector/front-end/UISourceCode.js \
    --module jsmodule_ui:37:jsmodule_common \
        --js Source/WebCore/inspector/front-end/Checkbox.js \
        --js Source/WebCore/inspector/front-end/Color.js \
        --js Source/WebCore/inspector/front-end/ContextMenu.js \
        --js Source/WebCore/inspector/front-end/CookiesTable.js \
        --js Source/WebCore/inspector/front-end/DOMSyntaxHighlighter.js \
        --js Source/WebCore/inspector/front-end/DataGrid.js \
        --js Source/WebCore/inspector/front-end/Drawer.js \
        --js Source/WebCore/inspector/front-end/EmptyView.js \
        --js Source/WebCore/inspector/front-end/HelpScreen.js \
        --js Source/WebCore/inspector/front-end/IFrameView.js \
        --js Source/WebCore/inspector/front-end/KeyboardShortcut.js \
        --js Source/WebCore/inspector/front-end/Panel.js \
        --js Source/WebCore/inspector/front-end/PanelEnablerView.js \
        --js Source/WebCore/inspector/front-end/Placard.js \
        --js Source/WebCore/inspector/front-end/Popover.js \
        --js Source/WebCore/inspector/front-end/PropertiesSection.js \
        --js Source/WebCore/inspector/front-end/PropertiesSidebarPane.js \
        --js Source/WebCore/inspector/front-end/SearchController.js \
        --js Source/WebCore/inspector/front-end/Section.js \
        --js Source/WebCore/inspector/front-end/SidebarPane.js \
        --js Source/WebCore/inspector/front-end/SidebarTreeElement.js \
        --js Source/WebCore/inspector/front-end/ShortcutsScreen.js \
        --js Source/WebCore/inspector/front-end/ShowMoreDataGridNode.js \
        --js Source/WebCore/inspector/front-end/SoftContextMenu.js \
        --js Source/WebCore/inspector/front-end/SourceTokenizer.js \
        --js Source/WebCore/inspector/front-end/StatusBarButton.js \
        --js Source/WebCore/inspector/front-end/TabbedPane.js \
        --js Source/WebCore/inspector/front-end/TextEditorModel.js \
        --js Source/WebCore/inspector/front-end/TextEditorHighlighter.js \
        --js Source/WebCore/inspector/front-end/TextPrompt.js \
        --js Source/WebCore/inspector/front-end/TextViewer.js \
        --js Source/WebCore/inspector/front-end/Toolbar.js \
        --js Source/WebCore/inspector/front-end/UIUtils.js \
        --js Source/WebCore/inspector/front-end/View.js \
        --js Source/WebCore/inspector/front-end/WelcomeView.js \
    --module jsmodule_inspector:26:jsmodule_sdk,jsmodule_ui \
        --js Source/WebCore/inspector/front-end/ConsoleMessage.js \
        --js Source/WebCore/inspector/front-end/ConsoleView.js \
        --js Source/WebCore/inspector/front-end/CookieItemsView.js \
        --js Source/WebCore/inspector/front-end/DatabaseQueryView.js \
        --js Source/WebCore/inspector/front-end/DatabaseTableView.js \
        --js Source/WebCore/inspector/front-end/DOMPresentationUtils.js \
        --js Source/WebCore/inspector/front-end/DOMStorageItemsView.js \
        --js Source/WebCore/inspector/front-end/ElementsTreeOutline.js \
        --js Source/WebCore/inspector/front-end/EventListenersSidebarPane.js \
        --js Source/WebCore/inspector/front-end/FontView.js \
        --js Source/WebCore/inspector/front-end/ImageView.js \
        --js Source/WebCore/inspector/front-end/JavaScriptContextManager.js \
        --js Source/WebCore/inspector/front-end/MetricsSidebarPane.js \
        --js Source/WebCore/inspector/front-end/NetworkItemView.js \
        --js Source/WebCore/inspector/front-end/ObjectPopoverHelper.js \
        --js Source/WebCore/inspector/front-end/ObjectPropertiesSection.js \
        --js Source/WebCore/inspector/front-end/ResourceCookiesView.js \
        --js Source/WebCore/inspector/front-end/ResourceHeadersView.js \
        --js Source/WebCore/inspector/front-end/ResourceHTMLView.js \
        --js Source/WebCore/inspector/front-end/ResourceJSONView.js \
        --js Source/WebCore/inspector/front-end/ResourcePreviewView.js \
        --js Source/WebCore/inspector/front-end/ResourceResponseView.js \
        --js Source/WebCore/inspector/front-end/ResourceTimingView.js \
        --js Source/WebCore/inspector/front-end/ResourceView.js \
        --js Source/WebCore/inspector/front-end/SourceFrame.js \
        --js Source/WebCore/inspector/front-end/StylesSidebarPane.js \
        --js Source/WebCore/inspector/front-end/TimelineAgent.js \
        --js Source/WebCore/inspector/front-end/TimelineManager.js

# To be compiled...
#
# [Elements]
#
# ElementsPanel
# BreakpointsSidebarPane
# DOMBreakpointsSidebarPane
#
# [Resources]
# ResourcesPanel
# 
# [Network]
# NetworkLog
# NetworkPanel
#
# [Scripts]
# JavaScriptFormatter
# JavaScriptSourceFrame
# ScopeChainSidebarPane
# ScriptFormatterWorker
# ScriptsPanel
# WatchExpressionsSidebarPane
# CallStackSidebarPane
#
# [Timeline]
# TimelineGrid
# TimelineOverviewPane
# TimelinePanel
#
# [Profiler]
# BottomUpProfileDataGridTree
# DetailedHeapshotGridNodes
# DetailedHeapshotView
# HeapSnapshot
# HeapSnapshotProxy
# HeapSnapshotWorker
# HeapSnapshotWorkerDispatcher
# ProfileDataGridTree
# ProfilesPanel
# ProfileView
# TopDownProfileDataGridTree
#
# [Audits]
# AuditCategories
# AuditFormatters
# AuditLauncherView
# AuditResultView
# AuditRules
# AuditsPanel
#
# [Console]
# ConsolePanel
#
# [Extensions]
# ExtensionAPI
# ExtensionAuditCategory
# ExtensionPanel
# ExtensionRegistryStub
# ExtensionServer
#
# [Misc]
# GoToLineDialog
# TestController
#
# [Misc]
# inspector
# SettingsScreen
# SourceCSSTokenizer
# SourceHTMLTokenizer
# SourceJavaScriptTokenizer
#
# [Workers]
# InjectedFakeWorker
# WorkerManager
# WorkersSidebarPane
