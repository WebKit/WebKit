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

java -jar ~/closure/compiler.jar --summary_detail_level 3 --compilation_level SIMPLE_OPTIMIZATIONS --warning_level VERBOSE --language_in ECMASCRIPT5 --accept_const_keyword \
    --externs Source/WebCore/inspector/front-end/externs.js \
    --externs Source/WebCore/inspector/front-end/protocol-externs.js \
    --module jsmodule_util:2 \
        --js Source/WebCore/inspector/front-end/utilities.js \
        --js Source/WebCore/inspector/front-end/treeoutline.js \
    --module jsmodule_common:7:jsmodule_util \
        --js Source/WebCore/inspector/front-end/BinarySearch.js \
        --js Source/WebCore/inspector/front-end/Object.js \
        --js Source/WebCore/inspector/front-end/PartialQuickSort.js \
        --js Source/WebCore/inspector/front-end/Settings.js \
        --js Source/WebCore/inspector/front-end/UserMetrics.js \
        --js Source/WebCore/inspector/front-end/HandlerRegistry.js \
        --js Source/WebCore/inspector/front-end/InspectorFrontendHostStub.js \
    --module jsmodule_sdk:29:jsmodule_common \
        --js Source/WebCore/inspector/front-end/InspectorBackend.js \
        --js Source/WebCore/inspector/front-end/ApplicationCacheModel.js \
        --js Source/WebCore/inspector/front-end/Color.js \
        --js Source/WebCore/inspector/front-end/CompilerSourceMapping.js \
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
        --js Source/WebCore/inspector/front-end/NetworkLog.js \
        --js Source/WebCore/inspector/front-end/Placard.js \
        --js Source/WebCore/inspector/front-end/Script.js \
        --js Source/WebCore/inspector/front-end/ScriptFormatter.js \
        --js Source/WebCore/inspector/front-end/RawSourceCode.js \
        --js Source/WebCore/inspector/front-end/RemoteObject.js \
        --js Source/WebCore/inspector/front-end/Resource.js \
        --js Source/WebCore/inspector/front-end/ResourceCategory.js \
        --js Source/WebCore/inspector/front-end/ResourceTreeModel.js \
        --js Source/WebCore/inspector/front-end/ResourceUtils.js \
        --js Source/WebCore/inspector/front-end/NetworkManager.js \
        --js Source/WebCore/inspector/front-end/UISourceCode.js \
    --module jsmodule_ui:34:jsmodule_common \
        --js Source/WebCore/inspector/front-end/AdvancedSearchController.js \
        --js Source/WebCore/inspector/front-end/Checkbox.js \
        --js Source/WebCore/inspector/front-end/ContextMenu.js \
        --js Source/WebCore/inspector/front-end/CookiesTable.js \
        --js Source/WebCore/inspector/front-end/DOMSyntaxHighlighter.js \
        --js Source/WebCore/inspector/front-end/DataGrid.js \
        --js Source/WebCore/inspector/front-end/Drawer.js \
        --js Source/WebCore/inspector/front-end/EmptyView.js \
        --js Source/WebCore/inspector/front-end/HelpScreen.js \
        --js Source/WebCore/inspector/front-end/InspectorView.js \
        --js Source/WebCore/inspector/front-end/KeyboardShortcut.js \
        --js Source/WebCore/inspector/front-end/Panel.js \
        --js Source/WebCore/inspector/front-end/PanelEnablerView.js \
        --js Source/WebCore/inspector/front-end/Popover.js \
        --js Source/WebCore/inspector/front-end/PropertiesSection.js \
        --js Source/WebCore/inspector/front-end/SearchController.js \
        --js Source/WebCore/inspector/front-end/Section.js \
        --js Source/WebCore/inspector/front-end/SidebarPane.js \
        --js Source/WebCore/inspector/front-end/SidebarTreeElement.js \
        --js Source/WebCore/inspector/front-end/ShortcutsScreen.js \
        --js Source/WebCore/inspector/front-end/ShowMoreDataGridNode.js \
        --js Source/WebCore/inspector/front-end/SoftContextMenu.js \
        --js Source/WebCore/inspector/front-end/SourceTokenizer.js \
        --js Source/WebCore/inspector/front-end/SplitView.js \
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
    --module jsmodule_components:16:jsmodule_sdk,jsmodule_ui \
        --js Source/WebCore/inspector/front-end/ConsoleMessage.js \
        --js Source/WebCore/inspector/front-end/BreakpointsSidebarPane.js \
        --js Source/WebCore/inspector/front-end/DOMBreakpointsSidebarPane.js \
        --js Source/WebCore/inspector/front-end/DOMPresentationUtils.js \
        --js Source/WebCore/inspector/front-end/ElementsTreeOutline.js \
        --js Source/WebCore/inspector/front-end/EventListenersSidebarPane.js \
        --js Source/WebCore/inspector/front-end/GoToLineDialog.js \
        --js Source/WebCore/inspector/front-end/JavaScriptContextManager.js \
        --js Source/WebCore/inspector/front-end/JavaScriptOutlineDialog.js \
        --js Source/WebCore/inspector/front-end/ObjectPopoverHelper.js \
        --js Source/WebCore/inspector/front-end/ObjectPropertiesSection.js \
        --js Source/WebCore/inspector/front-end/PropertiesSidebarPane.js \
        --js Source/WebCore/inspector/front-end/SourceFrame.js \
        --js Source/WebCore/inspector/front-end/TimelineAgent.js \
        --js Source/WebCore/inspector/front-end/TimelineGrid.js \
        --js Source/WebCore/inspector/front-end/TimelineManager.js \
    --module jsmodule_elements:3:jsmodule_components \
        --js Source/WebCore/inspector/front-end/StylesSidebarPane.js \
        --js Source/WebCore/inspector/front-end/MetricsSidebarPane.js \
        --js Source/WebCore/inspector/front-end/ElementsPanel.js \
    --module jsmodule_network:12:jsmodule_components \
        --js Source/WebCore/inspector/front-end/FontView.js \
        --js Source/WebCore/inspector/front-end/ImageView.js \
        --js Source/WebCore/inspector/front-end/NetworkItemView.js \
        --js Source/WebCore/inspector/front-end/ResourceCookiesView.js \
        --js Source/WebCore/inspector/front-end/ResourceHeadersView.js \
        --js Source/WebCore/inspector/front-end/ResourceHTMLView.js \
        --js Source/WebCore/inspector/front-end/ResourceJSONView.js \
        --js Source/WebCore/inspector/front-end/ResourcePreviewView.js \
        --js Source/WebCore/inspector/front-end/ResourceResponseView.js \
        --js Source/WebCore/inspector/front-end/ResourceTimingView.js \
        --js Source/WebCore/inspector/front-end/ResourceView.js \
        --js Source/WebCore/inspector/front-end/NetworkPanel.js \
    --module jsmodule_resources:6:jsmodule_components \
        --js Source/WebCore/inspector/front-end/ApplicationCacheItemsView.js \
        --js Source/WebCore/inspector/front-end/CookieItemsView.js \
        --js Source/WebCore/inspector/front-end/DatabaseQueryView.js \
        --js Source/WebCore/inspector/front-end/DatabaseTableView.js \
        --js Source/WebCore/inspector/front-end/DOMStorageItemsView.js \
        --js Source/WebCore/inspector/front-end/ResourcesPanel.js \
    --module jsmodule_scripts:10:jsmodule_components \
        --js Source/WebCore/inspector/front-end/CallStackSidebarPane.js \
        --js Source/WebCore/inspector/front-end/ScopeChainSidebarPane.js \
        --js Source/WebCore/inspector/front-end/JavaScriptSourceFrame.js \
        --js Source/WebCore/inspector/front-end/TabbedEditorContainer.js \
        --js Source/WebCore/inspector/front-end/ScriptsNavigator.js \
        --js Source/WebCore/inspector/front-end/ScriptsPanel.js \
        --js Source/WebCore/inspector/front-end/ScriptsSearchScope.js \
        --js Source/WebCore/inspector/front-end/WatchExpressionsSidebarPane.js \
        --js Source/WebCore/inspector/front-end/WorkerManager.js \
        --js Source/WebCore/inspector/front-end/WorkersSidebarPane.js \
    --module jsmodule_console:2:jsmodule_components \
        --js Source/WebCore/inspector/front-end/ConsoleView.js \
        --js Source/WebCore/inspector/front-end/ConsolePanel.js \
    --module jsmodule_timeline:2:jsmodule_components \
        --js Source/WebCore/inspector/front-end/TimelineOverviewPane.js \
        --js Source/WebCore/inspector/front-end/TimelinePanel.js \
    --module jsmodule_audits:6:jsmodule_components \
        --js Source/WebCore/inspector/front-end/AuditCategories.js \
        --js Source/WebCore/inspector/front-end/AuditFormatters.js \
        --js Source/WebCore/inspector/front-end/AuditLauncherView.js \
        --js Source/WebCore/inspector/front-end/AuditResultView.js \
        --js Source/WebCore/inspector/front-end/AuditRules.js \
        --js Source/WebCore/inspector/front-end/AuditsPanel.js \
    --module jsmodule_extensions:5:jsmodule_components \
        --js Source/WebCore/inspector/front-end/ExtensionAPI.js \
        --js Source/WebCore/inspector/front-end/ExtensionAuditCategory.js \
        --js Source/WebCore/inspector/front-end/ExtensionPanel.js \
        --js Source/WebCore/inspector/front-end/ExtensionRegistryStub.js \
        --js Source/WebCore/inspector/front-end/ExtensionServer.js \
    --module jsmodule_inspector:1:jsmodule_components,jsmodule_extensions \
        --js Source/WebCore/inspector/front-end/SettingsScreen.js \
    --module jsmodule_tests:1:jsmodule_components \
        --js Source/WebCore/inspector/front-end/TestController.js

#     --module jsmodule_tokenizers:3:jsmodule_components \
#        --js Source/WebCore/inspector/front-end/SourceCSSTokenizer.js \
#        --js Source/WebCore/inspector/front-end/SourceHTMLTokenizer.js \
#        --js Source/WebCore/inspector/front-end/SourceJavaScriptTokenizer.js

# To be compiled...
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
# [Misc]
# inspector
# SettingsScreen
# JavaScriptFormatter
# ScriptFormatterWorker
