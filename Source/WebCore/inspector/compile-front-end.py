#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
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

import os

inspector_frontend_path = "Source/WebCore/inspector/front-end"
externs = ["externs.js", "protocol-externs.js"]
jsmodule_name_prefix = "jsmodule_"
modules = [
    {
        "target_name": "util",
        "dependencies": [],
        "sources": [
            "DOMExtension.js",
            "utilities.js",
            "treeoutline.js",
        ]
    },
    {
        "target_name": "common",
        "dependencies": ["util"],
        "sources": [
            "Object.js",
            "Settings.js",
            "UserMetrics.js",
            "HandlerRegistry.js",
            "InspectorFrontendHostStub.js",
        ]
    },
    {
        "target_name": "sdk",
        "dependencies": ["common"],
        "sources": [
            "InspectorBackend.js",
            "ApplicationCacheModel.js",
            "Color.js",
            "CompilerScriptMapping.js",
            "ConsoleModel.js",
            "ContentProvider.js",
            "ContentProviders.js",
            "CookieParser.js",
            "CSSCompletions.js",
            "CSSKeywordCompletions.js",
            "CSSStyleModel.js",
            "BreakpointManager.js",
            "Database.js",
            "DOMAgent.js",
            "DOMStorage.js",
            "DebuggerModel.js",
            "DebuggerResourceBinding.js",
            "DebuggerScriptMapping.js",
            "FileManager.js",
            "HAREntry.js",
            "IndexedDBModel.js",
            "Linkifier.js",
            "NetworkLog.js",
            "Placard.js",
            "Script.js",
            "ScriptFormatter.js",
            "ScriptMapping.js",
            "ScriptSnippetModel.js",
            "SnippetStorage.js",
            "TimelineManager.js",
            "TimelineModel.js",
            "RawSourceCode.js",
            "RemoteObject.js",
            "Resource.js",
            "ResourceScriptMapping.js",
            "ResourceTreeModel.js",
            "ResourceType.js",
            "ResourceUtils.js",
            "NetworkManager.js",
            "NetworkRequest.js",
            "UISourceCode.js",
            "UserAgentSupport.js",
        ]
    },
    {
        "target_name": "ui",
        "dependencies": ["common"],
        "sources": [
            "AdvancedSearchController.js",
            "Checkbox.js",
            "ContextMenu.js",
            "CookiesTable.js",
            "DOMSyntaxHighlighter.js",
            "DataGrid.js",
            "Dialog.js",
            "Drawer.js",
            "EmptyView.js",
            "HelpScreen.js",
            "InspectorView.js",
            "KeyboardShortcut.js",
            "Panel.js",
            "PanelEnablerView.js",
            "Popover.js",
            "PropertiesSection.js",
            "SearchController.js",
            "Section.js",
            "SidebarPane.js",
            "SidebarTreeElement.js",
            "ShortcutsScreen.js",
            "ShowMoreDataGridNode.js",
            "SidebarOverlay.js",
            "SoftContextMenu.js",
            "SourceTokenizer.js",
            "Spectrum.js",
            "SplitView.js",
            "StatusBarButton.js",
            "TabbedPane.js",
            "TextEditorModel.js",
            "TextEditorHighlighter.js",
            "TextPrompt.js",
            "TextViewer.js",
            "Toolbar.js",
            "UIUtils.js",
            "View.js",
        ]
    },
    {
        "target_name": "components",
        "dependencies": ["sdk", "ui"],
        "sources": [
            "ConsoleMessage.js",
            "BreakpointsSidebarPane.js",
            "DOMBreakpointsSidebarPane.js",
            "DOMPresentationUtils.js",
            "ElementsTreeOutline.js",
            "EventListenersSidebarPane.js",
            "FilteredItemSelectionDialog.js",
            "GoToLineDialog.js",
            "NavigatorOverlayController.js",
            "NavigatorView.js",
            "JavaScriptContextManager.js",
            "JavaScriptSource.js",
            "ObjectPopoverHelper.js",
            "ObjectPropertiesSection.js",
            "PropertiesSidebarPane.js",
            "SourceFrame.js",
            "TimelineGrid.js",
        ]
    },
    {
        "target_name": "elements",
        "dependencies": ["components"],
        "sources": [
            "StylesSidebarPane.js",
            "MetricsSidebarPane.js",
            "ElementsPanel.js",
        ]
    },
    {
        "target_name": "network",
        "dependencies": ["components"],
        "sources": [
            "FontView.js",
            "ImageView.js",
            "NetworkItemView.js",
            "RequestCookiesView.js",
            "RequestHeadersView.js",
            "RequestHTMLView.js",
            "RequestJSONView.js",
            "RequestPreviewView.js",
            "RequestResponseView.js",
            "RequestTimingView.js",
            "RequestView.js",
            "ResourceView.js",
            "ResourceWebSocketFrameView.js",
            "NetworkPanel.js",
        ]
    },
    {
        "target_name": "resources",
        "dependencies": ["components"],
        "sources": [
            "ApplicationCacheItemsView.js",
            "CookieItemsView.js",
            "DatabaseQueryView.js",
            "DatabaseTableView.js",
            "DOMStorageItemsView.js",
            "IndexedDBViews.js",
            "ResourcesPanel.js",
        ]
    },
    {
        "target_name": "scripts",
        "dependencies": ["components"],
        "sources": [
            "CallStackSidebarPane.js",
            "ScopeChainSidebarPane.js",
            "JavaScriptSourceFrame.js",
            "PresentationConsoleMessageHelper.js",
            "TabbedEditorContainer.js",
            "ScriptsNavigator.js",
            "ScriptsPanel.js",
            "ScriptsSearchScope.js",
            "WatchExpressionsSidebarPane.js",
            "WorkerManager.js",
            "WorkersSidebarPane.js",
        ]
    },
    {
        "target_name": "console",
        "dependencies": ["components"],
        "sources": [
            "ConsoleView.js",
            "ConsolePanel.js",
        ]
    },
    {
        "target_name": "timeline",
        "dependencies": ["components"],
        "sources": [
            "MemoryStatistics.js",
            "TimelineOverviewPane.js",
            "TimelinePanel.js",
            "TimelinePresentationModel.js",
            "TimelineFrameController.js"
        ]
    },
    {
        "target_name": "audits",
        "dependencies": ["components"],
        "sources": [
            "AuditCategories.js",
            "AuditFormatters.js",
            "AuditLauncherView.js",
            "AuditResultView.js",
            "AuditRules.js",
            "AuditsPanel.js",
        ]
    },
    {
        "target_name": "styles",
        "dependencies": ["components"],
        "sources": [
            "StylesPanel.js",
        ]
    },
    {
        "target_name": "extensions",
        "dependencies": ["components"],
        "sources": [
            "ExtensionAPI.js",
            "ExtensionAuditCategory.js",
            "ExtensionPanel.js",
            "ExtensionRegistryStub.js",
            "ExtensionServer.js",
        ]
    },
    {
        "target_name": "inspector",
        "dependencies": ["components", "extensions"],
        "sources": [
            "SettingsScreen.js",
        ]
    },
    {
        "target_name": "tests",
        "dependencies": ["components"],
        "sources": [
            "TestController.js",
        ]
    },
    {
        "target_name": "profiler",
        "dependencies": ["components"],
        "sources": [
            "BottomUpProfileDataGridTree.js",
            "CSSSelectorProfileView.js",
            "HeapSnapshot.js",
            "HeapSnapshotDataGrids.js",
            "HeapSnapshotGridNodes.js",
            "HeapSnapshotLoader.js",
            "HeapSnapshotProxy.js",
            "HeapSnapshotView.js",
            "HeapSnapshotWorker.js",
            "HeapSnapshotWorkerDispatcher.js",
            "ProfileDataGridTree.js",
            "ProfilesPanel.js",
            "ProfileLauncherView.js",
            "ProfileView.js",
            "TopDownProfileDataGridTree.js",
        ]
    },
#    {
#        "target_name": "tokenizers",
#        "dependencies": ["components"],
#        "sources": [
#            "SourceCSSTokenizer.js",
#            "SourceHTMLTokenizer.js",
#            "SourceJavaScriptTokenizer.js",
#        ]
#    },
]

# To be compiled...
#
# [Misc]
# inspector
# SettingsScreen
# JavaScriptFormatter
# ScriptFormatterWorker

command = "java -jar ~/closure/compiler.jar --summary_detail_level 3 --compilation_level SIMPLE_OPTIMIZATIONS --warning_level VERBOSE --language_in ECMASCRIPT5 --accept_const_keyword \\\n"
for extern in externs:
    command += "    --externs " + inspector_frontend_path + "/" + extern
    command += " \\\n"
for module in modules:
    command += "    --module " + jsmodule_name_prefix + module["target_name"] + ":"
    command += str(len(module["sources"]))
    firstDependency = True
    for dependency in module["dependencies"]:
        if firstDependency:
            command += ":"
        else:
            command += ","
        firstDependency = False
        command += jsmodule_name_prefix + dependency
    command += " \\\n"
    for script in module["sources"]:
        command += "        --js " + inspector_frontend_path + "/" + script
        command += " \\\n"
command += "\n"

os.system(command)
