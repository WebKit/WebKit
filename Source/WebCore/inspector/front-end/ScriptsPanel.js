/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

importScript("BreakpointsSidebarPane.js");
importScript("CallStackSidebarPane.js");
importScript("FilteredItemSelectionDialog.js");
importScript("JavaScriptSourceFrame.js");
importScript("RevisionHistoryView.js");
importScript("ScopeChainSidebarPane.js");
importScript("ScriptsNavigator.js");
importScript("ScriptsSearchScope.js");
importScript("SnippetJavaScriptSourceFrame.js");
importScript("StyleSheetOutlineDialog.js");
importScript("TabbedEditorContainer.js");
importScript("UISourceCodeFrame.js");
importScript("WatchExpressionsSidebarPane.js");
importScript("WorkersSidebarPane.js");

/**
 * @constructor
 * @implements {WebInspector.TabbedEditorContainerDelegate}
 * @implements {WebInspector.ContextMenu.Provider}
 * @extends {WebInspector.Panel}
 * @param {WebInspector.Workspace=} workspaceForTest
 */
WebInspector.ScriptsPanel = function(workspaceForTest)
{
    WebInspector.Panel.call(this, "scripts");
    this.registerRequiredCSS("scriptsPanel.css");

    WebInspector.settings.navigatorWasOnceHidden = WebInspector.settings.createSetting("navigatorWasOnceHidden", false);
    WebInspector.settings.debuggerSidebarHidden = WebInspector.settings.createSetting("debuggerSidebarHidden", false);

    this._workspace = workspaceForTest || WebInspector.workspace;

    function viewGetter()
    {
        return this.visibleView;
    }
    WebInspector.GoToLineDialog.install(this, viewGetter.bind(this));

    var helpSection = WebInspector.shortcutsScreen.section(WebInspector.UIString("Sources Panel"));
    this.debugToolbar = this._createDebugToolbar(helpSection);

    const initialDebugSidebarWidth = 225;
    const maximalDebugSidebarWidthPercent = 50;
    this.createSplitView(this.element, WebInspector.SplitView.SidebarPosition.Right, initialDebugSidebarWidth);
    this.splitView.element.id = "scripts-split-view";
    this.splitView.minimalSidebarWidth = Preferences.minScriptsSidebarWidth;
    this.splitView.minimalMainWidthPercent = 100 - maximalDebugSidebarWidthPercent;

    this.sidebarElement.appendChild(this.debugToolbar);

    this.debugSidebarResizeWidgetElement = document.createElement("div");
    this.debugSidebarResizeWidgetElement.id = "scripts-debug-sidebar-resizer-widget";
    this.splitView.installResizer(this.debugSidebarResizeWidgetElement);

    // Create scripts navigator
    const initialNavigatorWidth = 225;
    const minimalViewsContainerWidthPercent = 50;
    this.editorView = new WebInspector.SplitView(WebInspector.SplitView.SidebarPosition.Left, "scriptsPanelNavigatorSidebarWidth", initialNavigatorWidth);
    this.editorView.element.tabIndex = 0;

    this.editorView.minimalSidebarWidth = Preferences.minScriptsSidebarWidth;
    this.editorView.minimalMainWidthPercent = minimalViewsContainerWidthPercent;
    this.editorView.show(this.splitView.mainElement);

    this._navigator = new WebInspector.ScriptsNavigator();
    this._navigator.view.show(this.editorView.sidebarElement);

    this._editorContainer = new WebInspector.TabbedEditorContainer(this, "previouslyViewedFiles");
    this._editorContainer.show(this.editorView.mainElement);

    this._navigatorController = new WebInspector.NavigatorOverlayController(this.editorView, this._navigator.view, this._editorContainer.view);

    this._navigator.addEventListener(WebInspector.ScriptsNavigator.Events.ScriptSelected, this._scriptSelected, this);
    this._navigator.addEventListener(WebInspector.ScriptsNavigator.Events.SnippetCreationRequested, this._snippetCreationRequested, this);
    this._navigator.addEventListener(WebInspector.ScriptsNavigator.Events.ItemRenamingRequested, this._itemRenamingRequested, this);
    this._navigator.addEventListener(WebInspector.ScriptsNavigator.Events.FileRenamed, this._fileRenamed, this);

    this._editorContainer.addEventListener(WebInspector.TabbedEditorContainer.Events.EditorSelected, this._editorSelected, this);
    this._editorContainer.addEventListener(WebInspector.TabbedEditorContainer.Events.EditorClosed, this._editorClosed, this);

    this.splitView.mainElement.appendChild(this.debugSidebarResizeWidgetElement);

    this.sidebarPanes = {};
    this.sidebarPanes.watchExpressions = new WebInspector.WatchExpressionsSidebarPane();
    this.sidebarPanes.callstack = new WebInspector.CallStackSidebarPane();
    this.sidebarPanes.scopechain = new WebInspector.ScopeChainSidebarPane();
    this.sidebarPanes.jsBreakpoints = new WebInspector.JavaScriptBreakpointsSidebarPane(WebInspector.breakpointManager, this._showSourceLine.bind(this));
    this.sidebarPanes.domBreakpoints = WebInspector.domBreakpointsSidebarPane;
    this.sidebarPanes.xhrBreakpoints = new WebInspector.XHRBreakpointsSidebarPane();
    this.sidebarPanes.eventListenerBreakpoints = new WebInspector.EventListenerBreakpointsSidebarPane();

    if (InspectorFrontendHost.canInspectWorkers() && !WebInspector.WorkerManager.isWorkerFrontend()) {
        WorkerAgent.enable();
        this.sidebarPanes.workerList = new WebInspector.WorkersSidebarPane(WebInspector.workerManager);
    }

    this._debugSidebarContentsElement = document.createElement("div");
    this._debugSidebarContentsElement.id = "scripts-debug-sidebar-contents";
    this.sidebarElement.appendChild(this._debugSidebarContentsElement);

    for (var pane in this.sidebarPanes)
        this._debugSidebarContentsElement.appendChild(this.sidebarPanes[pane].element);

    this.sidebarPanes.callstack.expanded = true;

    this.sidebarPanes.scopechain.expanded = true;
    this.sidebarPanes.jsBreakpoints.expanded = true;

    this.sidebarPanes.callstack.registerShortcuts(helpSection, this.registerShortcut.bind(this));
    var evaluateInConsoleShortcut = WebInspector.KeyboardShortcut.makeDescriptor("e", WebInspector.KeyboardShortcut.Modifiers.Shift | WebInspector.KeyboardShortcut.Modifiers.Ctrl);
    helpSection.addKey(evaluateInConsoleShortcut.name, WebInspector.UIString("Evaluate selection in console"));
    this.registerShortcut(evaluateInConsoleShortcut.key, this._evaluateSelectionInConsole.bind(this));

    var outlineShortcut = WebInspector.KeyboardShortcut.makeDescriptor("o", WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta | WebInspector.KeyboardShortcut.Modifiers.Shift);
    helpSection.addKey(outlineShortcut.name, WebInspector.UIString("Go to member"));
    this.registerShortcut(outlineShortcut.key, this._showOutlineDialog.bind(this));

    var createBreakpointShortcut = WebInspector.KeyboardShortcut.makeDescriptor("b", WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta);
    helpSection.addKey(createBreakpointShortcut.name, WebInspector.UIString("Toggle breakpoint"));
    this.registerShortcut(createBreakpointShortcut.key, this._toggleBreakpoint.bind(this));

    var panelEnablerHeading = WebInspector.UIString("You need to enable debugging before you can use the Scripts panel.");
    var panelEnablerDisclaimer = WebInspector.UIString("Enabling debugging will make scripts run slower.");
    var panelEnablerButton = WebInspector.UIString("Enable Debugging");

    this.panelEnablerView = new WebInspector.PanelEnablerView("scripts", panelEnablerHeading, panelEnablerDisclaimer, panelEnablerButton);
    this.panelEnablerView.addEventListener("enable clicked", this._enableDebugging, this);

    this.enableToggleButton = new WebInspector.StatusBarButton("", "enable-toggle-status-bar-item");
    this.enableToggleButton.addEventListener("click", this._toggleDebugging, this);
    if (!Capabilities.debuggerCausesRecompilation)
        this.enableToggleButton.element.addStyleClass("hidden");

    this._pauseOnExceptionButton = new WebInspector.StatusBarButton("", "scripts-pause-on-exceptions-status-bar-item", 3);
    this._pauseOnExceptionButton.addEventListener("click", this._togglePauseOnExceptions, this);

    this._toggleFormatSourceButton = new WebInspector.StatusBarButton(WebInspector.UIString("Pretty print"), "scripts-toggle-pretty-print-status-bar-item");
    this._toggleFormatSourceButton.toggled = false;
    this._toggleFormatSourceButton.addEventListener("click", this._toggleFormatSource, this);

    this._scriptViewStatusBarItemsContainer = document.createElement("div");
    this._scriptViewStatusBarItemsContainer.style.display = "inline-block";

    this._installDebuggerSidebarController();

    this._sourceFramesByUISourceCode = new Map();
    this._updateDebuggerButtons();
    this._pauseOnExceptionStateChanged();
    if (WebInspector.debuggerModel.isPaused())
        this._debuggerPaused();

    WebInspector.settings.pauseOnExceptionStateString.addChangeListener(this._pauseOnExceptionStateChanged, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerWasEnabled, this._debuggerWasEnabled, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerWasDisabled, this._debuggerWasDisabled, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerPaused, this._debuggerPaused, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerResumed, this._debuggerResumed, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.CallFrameSelected, this._callFrameSelected, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ConsoleCommandEvaluatedInSelectedCallFrame, this._consoleCommandEvaluatedInSelectedCallFrame, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ExecutionLineChanged, this._executionLineChanged, this);
    WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointsActiveStateChanged, this._breakpointsActiveStateChanged, this);

    WebInspector.startBatchUpdate();
    var uiSourceCodes = this._workspace.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i)
        this._addUISourceCode(uiSourceCodes[i]);
    WebInspector.endBatchUpdate();

    this._workspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeAdded, this._uiSourceCodeAdded, this);
    this._workspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeReplaced, this._uiSourceCodeReplaced, this);
    this._workspace.addEventListener(WebInspector.UISourceCodeProvider.Events.UISourceCodeRemoved, this._uiSourceCodeRemoved, this);
    this._workspace.addEventListener(WebInspector.Workspace.Events.WorkspaceReset, this._reset.bind(this), this);

    WebInspector.advancedSearchController.registerSearchScope(new WebInspector.ScriptsSearchScope(this._workspace));
    WebInspector.ContextMenu.registerProvider(this);
}

WebInspector.ScriptsPanel.prototype = {
    get statusBarItems()
    {
        return [this.enableToggleButton.element, this._pauseOnExceptionButton.element, this._toggleFormatSourceButton.element, this._scriptViewStatusBarItemsContainer];
    },

    defaultFocusedElement: function()
    {
        return this._navigator.view.defaultFocusedElement();
    },

    get paused()
    {
        return this._paused;
    },

    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
        this._debugSidebarContentsElement.insertBefore(this.sidebarPanes.domBreakpoints.element, this.sidebarPanes.xhrBreakpoints.element);
        this.sidebarPanes.watchExpressions.show();

        this._navigatorController.wasShown();
    },

    willHide: function()
    {
        WebInspector.Panel.prototype.willHide.call(this);
        WebInspector.closeViewInDrawer();
    },

    /**
     * @param {WebInspector.Event} event
     */
    _uiSourceCodeAdded: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;
        this._addUISourceCode(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _addUISourceCode: function(uiSourceCode)
    {
        if (this._toggleFormatSourceButton.toggled)
            uiSourceCode.setFormatted(true);

        this._navigator.addUISourceCode(uiSourceCode);
        this._editorContainer.addUISourceCode(uiSourceCode);
    },

    _uiSourceCodeRemoved: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;
        this._editorContainer.removeUISourceCode(uiSourceCode);
        this._navigator.removeUISourceCode(uiSourceCode);
        this._removeSourceFrame(uiSourceCode);
    },

    _consoleCommandEvaluatedInSelectedCallFrame: function(event)
    {
        this.sidebarPanes.scopechain.update(WebInspector.debuggerModel.selectedCallFrame());
    },

    _debuggerPaused: function()
    {
        var details = WebInspector.debuggerModel.debuggerPausedDetails();

        this._paused = true;
        this._waitingToPause = false;
        this._stepping = false;

        this._updateDebuggerButtons();

        WebInspector.inspectorView.setCurrentPanel(this);
        this.sidebarPanes.callstack.update(details.callFrames);

        if (details.reason === WebInspector.DebuggerModel.BreakReason.DOM) {
            this.sidebarPanes.domBreakpoints.highlightBreakpoint(details.auxData);
            function didCreateBreakpointHitStatusMessage(element)
            {
                this.sidebarPanes.callstack.setStatus(element);
            }
            this.sidebarPanes.domBreakpoints.createBreakpointHitStatusMessage(details.auxData, didCreateBreakpointHitStatusMessage.bind(this));
        } else if (details.reason === WebInspector.DebuggerModel.BreakReason.EventListener) {
            var eventName = details.auxData.eventName;
            this.sidebarPanes.eventListenerBreakpoints.highlightBreakpoint(details.auxData.eventName);
            var eventNameForUI = WebInspector.EventListenerBreakpointsSidebarPane.eventNameForUI(eventName);
            this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a \"%s\" Event Listener.", eventNameForUI));
        } else if (details.reason === WebInspector.DebuggerModel.BreakReason.XHR) {
            this.sidebarPanes.xhrBreakpoints.highlightBreakpoint(details.auxData["breakpointURL"]);
            this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a XMLHttpRequest."));
        } else if (details.reason === WebInspector.DebuggerModel.BreakReason.Exception) {
            this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on exception: '%s'.", details.auxData.description));
        } else {
            function didGetUILocation(uiLocation)
            {
                var breakpoint = WebInspector.breakpointManager.findBreakpoint(uiLocation.uiSourceCode, uiLocation.lineNumber);
                if (!breakpoint)
                    return;
                this.sidebarPanes.jsBreakpoints.highlightBreakpoint(breakpoint);
                this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a JavaScript breakpoint."));
            }
            details.callFrames[0].createLiveLocation(didGetUILocation.bind(this));
        }

        this._showDebuggerSidebar();
        this._toggleDebuggerSidebarButton.disabled = true;
        window.focus();
        InspectorFrontendHost.bringToFront();
    },

    _debuggerResumed: function()
    {
        this._paused = false;
        this._waitingToPause = false;
        this._stepping = false;

        this._clearInterface();
        this._toggleDebuggerSidebarButton.disabled = false;
    },

    _debuggerWasEnabled: function()
    {
        this._updateDebuggerButtons();
    },

    _debuggerWasDisabled: function()
    {
        this._reset();
    },

    _reset: function()
    {
        delete this.currentQuery;
        this.searchCanceled();

        this._debuggerResumed();

        delete this._currentUISourceCode;
        this._navigator.reset();
        this._editorContainer.reset();
        this._updateScriptViewStatusBarItems();
        this.sidebarPanes.jsBreakpoints.reset();
        this.sidebarPanes.watchExpressions.reset();

        var uiSourceCodes = this._workspace.uiSourceCodes();
        for (var i = 0; i < uiSourceCodes.length; ++i)
            this._removeSourceFrame(uiSourceCodes[i]);
    },

    get visibleView()
    {
        return this._editorContainer.visibleView;
    },

    _updateScriptViewStatusBarItems: function()
    {
        this._scriptViewStatusBarItemsContainer.removeChildren();

        var sourceFrame = this.visibleView;
        if (sourceFrame) {
            var statusBarItems = sourceFrame.statusBarItems() || [];
            for (var i = 0; i < statusBarItems.length; ++i)
                this._scriptViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
        }
    },

    canShowAnchorLocation: function(anchor)
    {
        if (WebInspector.debuggerModel.debuggerEnabled() && anchor.uiSourceCode)
            return true;
        var uiSourceCodes = this._workspace.uiSourceCodes();
        for (var i = 0; i < uiSourceCodes.length; ++i) {
            if (uiSourceCodes[i].url === anchor.href) {
                anchor.uiSourceCode = uiSourceCodes[i];
                return true;
            }
        }
        return false;
    },

    showAnchorLocation: function(anchor)
    {
        this._showSourceLine(anchor.uiSourceCode, anchor.lineNumber);
    },

    showFunctionDefinition: function(functionLocation)
    {
        WebInspector.showPanelForAnchorNavigation(this);
        var uiLocation = WebInspector.debuggerModel.rawLocationToUILocation(functionLocation);
        this._showSourceLine(uiLocation.uiSourceCode, uiLocation.lineNumber);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number} lineNumber
     */
    showUISourceCode: function(uiSourceCode, lineNumber)
    {
        this._showSourceLine(uiSourceCode, lineNumber);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {number=} lineNumber
     */
    _showSourceLine: function(uiSourceCode, lineNumber)
    {
        var sourceFrame = this._showFile(uiSourceCode);
        if (typeof lineNumber === "number")
            sourceFrame.highlightLine(lineNumber);
        sourceFrame.focus();
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {WebInspector.SourceFrame}
     */
    _showFile: function(uiSourceCode)
    {
        var sourceFrame = this._getOrCreateSourceFrame(uiSourceCode);
        if (this._currentUISourceCode === uiSourceCode)
            return sourceFrame;
        this._currentUISourceCode = uiSourceCode;

        if (this._navigator.isScriptSourceAdded(uiSourceCode))
            this._navigator.revealUISourceCode(uiSourceCode);
        this._editorContainer.showFile(uiSourceCode);
        this._updateScriptViewStatusBarItems();

        return sourceFrame;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {WebInspector.SourceFrame}
     */
    _createSourceFrame: function(uiSourceCode)
    {
        var sourceFrame;
        if (uiSourceCode instanceof WebInspector.SnippetJavaScriptSource) {
            var snippetJavaScriptSource = /** @type {WebInspector.SnippetJavaScriptSource} */ uiSourceCode;
            sourceFrame = new WebInspector.SnippetJavaScriptSourceFrame(this, snippetJavaScriptSource);
        } else if (uiSourceCode instanceof WebInspector.JavaScriptSource) {
                var javaScriptSource = /** @type {WebInspector.JavaScriptSource} */ uiSourceCode;
                sourceFrame = new WebInspector.JavaScriptSourceFrame(this, javaScriptSource);
        } else
            sourceFrame = new WebInspector.UISourceCodeFrame(uiSourceCode);
        this._sourceFramesByUISourceCode.put(uiSourceCode, sourceFrame);
        return sourceFrame;
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {WebInspector.SourceFrame}
     */
    _getOrCreateSourceFrame: function(uiSourceCode)
    {
        return this._sourceFramesByUISourceCode.get(uiSourceCode) || this._createSourceFrame(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @return {WebInspector.SourceFrame}
     */
    viewForFile: function(uiSourceCode)
    {
        return this._getOrCreateSourceFrame(uiSourceCode);
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _removeSourceFrame: function(uiSourceCode)
    {
        var sourceFrame = this._sourceFramesByUISourceCode.get(uiSourceCode);
        if (!sourceFrame)
            return;
        this._sourceFramesByUISourceCode.remove(uiSourceCode);
        sourceFrame.detach();
    },

    /**
     * @param {WebInspector.Event} event
     */
    _uiSourceCodeReplaced: function(event)
    {
        var oldUISourceCode = /** @type {WebInspector.UISourceCode} */ event.data.oldUISourceCode;
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data.uiSourceCode;

        this._navigator.replaceUISourceCode(oldUISourceCode, uiSourceCode);
        this._editorContainer.replaceFile(oldUISourceCode, uiSourceCode);
        this._removeSourceFrame(oldUISourceCode);
    },

    _clearCurrentExecutionLine: function()
    {
        if (this._executionSourceFrame)
            this._executionSourceFrame.clearExecutionLine();
        delete this._executionSourceFrame;
    },

    _executionLineChanged: function(event)
    {
        var uiLocation = event.data;

        this._clearCurrentExecutionLine();
        if (!uiLocation)
            return;
        var sourceFrame = this._getOrCreateSourceFrame(uiLocation.uiSourceCode);
        sourceFrame.setExecutionLine(uiLocation.lineNumber);
        this._executionSourceFrame = sourceFrame;
    },

    _revealExecutionLine: function(uiLocation)
    {
        // Some scripts (anonymous and snippets evaluations) are not added to files select by default.
        this._editorContainer.addUISourceCode(uiLocation.uiSourceCode);
        var sourceFrame = this._showFile(uiLocation.uiSourceCode);
        sourceFrame.revealLine(uiLocation.lineNumber);
        sourceFrame.focus();
    },

    _callFrameSelected: function(event)
    {
        var callFrame = event.data;

        if (!callFrame)
            return;

        this.sidebarPanes.scopechain.update(callFrame);
        this.sidebarPanes.watchExpressions.refreshExpressions();
        this.sidebarPanes.callstack.setSelectedCallFrame(callFrame);
        callFrame.createLiveLocation(this._revealExecutionLine.bind(this));
    },

    _editorClosed: function(event)
    {
        this._navigatorController.hideNavigatorOverlay();
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;

        if (this._currentUISourceCode === uiSourceCode)
            delete this._currentUISourceCode;

        // ScriptsNavigator does not need to update on EditorClosed.
        this._updateScriptViewStatusBarItems();
        WebInspector.searchController.resetSearch();
    },

    _editorSelected: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;
        var sourceFrame = this._showFile(uiSourceCode);
        this._navigatorController.hideNavigatorOverlay();
        sourceFrame.focus();
        WebInspector.searchController.resetSearch();
    },

    _scriptSelected: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data.uiSourceCode;
        var sourceFrame = this._showFile(uiSourceCode);
        this._navigatorController.hideNavigatorOverlay();
        if (sourceFrame && event.data.focusSource)
            sourceFrame.focus();
    },

    _pauseOnExceptionStateChanged: function()
    {
        var pauseOnExceptionsState = WebInspector.settings.pauseOnExceptionStateString.get();
        switch (pauseOnExceptionsState) {
        case WebInspector.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions:
            this._pauseOnExceptionButton.title = WebInspector.UIString("Don't pause on exceptions.\nClick to Pause on all exceptions.");
            break;
        case WebInspector.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions:
            this._pauseOnExceptionButton.title = WebInspector.UIString("Pause on all exceptions.\nClick to Pause on uncaught exceptions.");
            break;
        case WebInspector.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions:
            this._pauseOnExceptionButton.title = WebInspector.UIString("Pause on uncaught exceptions.\nClick to Not pause on exceptions.");
            break;
        }
        this._pauseOnExceptionButton.state = pauseOnExceptionsState;
    },

    _updateDebuggerButtons: function()
    {
        if (WebInspector.debuggerModel.debuggerEnabled()) {
            this.enableToggleButton.title = WebInspector.UIString("Debugging enabled. Click to disable.");
            this.enableToggleButton.toggled = true;
            this._pauseOnExceptionButton.visible = true;
            this.panelEnablerView.detach();
        } else {
            this.enableToggleButton.title = WebInspector.UIString("Debugging disabled. Click to enable.");
            this.enableToggleButton.toggled = false;
            this._pauseOnExceptionButton.visible = false;
            this.panelEnablerView.show(this.element);
        }

        if (this._paused) {
            this.pauseButton.addStyleClass("paused");

            this.pauseButton.disabled = false;
            this.stepOverButton.disabled = false;
            this.stepIntoButton.disabled = false;
            this.stepOutButton.disabled = false;

            this.debuggerStatusElement.textContent = WebInspector.UIString("Paused");
        } else {
            this.pauseButton.removeStyleClass("paused");

            this.pauseButton.disabled = this._waitingToPause;
            this.stepOverButton.disabled = true;
            this.stepIntoButton.disabled = true;
            this.stepOutButton.disabled = true;

            if (this._waitingToPause)
                this.debuggerStatusElement.textContent = WebInspector.UIString("Pausing");
            else if (this._stepping)
                this.debuggerStatusElement.textContent = WebInspector.UIString("Stepping");
            else
                this.debuggerStatusElement.textContent = "";
        }
    },

    _clearInterface: function()
    {
        this.sidebarPanes.callstack.update(null);
        this.sidebarPanes.scopechain.update(null);
        this.sidebarPanes.jsBreakpoints.clearBreakpointHighlight();
        this.sidebarPanes.domBreakpoints.clearBreakpointHighlight();
        this.sidebarPanes.eventListenerBreakpoints.clearBreakpointHighlight();
        this.sidebarPanes.xhrBreakpoints.clearBreakpointHighlight();

        this._clearCurrentExecutionLine();
        this._updateDebuggerButtons();
    },

    _enableDebugging: function()
    {
        this._toggleDebugging(this.panelEnablerView.alwaysEnabled);
    },

    _toggleDebugging: function(optionalAlways)
    {
        this._paused = false;
        this._waitingToPause = false;
        this._stepping = false;

        if (WebInspector.debuggerModel.debuggerEnabled()) {
            WebInspector.settings.debuggerEnabled.set(false);
            WebInspector.debuggerModel.disableDebugger();
        } else {
            WebInspector.settings.debuggerEnabled.set(!!optionalAlways);
            WebInspector.debuggerModel.enableDebugger();
        }
    },

    _togglePauseOnExceptions: function()
    {
        var nextStateMap = {};
        var stateEnum = WebInspector.DebuggerModel.PauseOnExceptionsState;
        nextStateMap[stateEnum.DontPauseOnExceptions] = stateEnum.PauseOnAllExceptions;
        nextStateMap[stateEnum.PauseOnAllExceptions] = stateEnum.PauseOnUncaughtExceptions;
        nextStateMap[stateEnum.PauseOnUncaughtExceptions] = stateEnum.DontPauseOnExceptions;
        WebInspector.settings.pauseOnExceptionStateString.set(nextStateMap[this._pauseOnExceptionButton.state]);
    },

    _togglePause: function()
    {
        if (this._paused) {
            this._paused = false;
            this._waitingToPause = false;
            DebuggerAgent.resume();
        } else {
            this._stepping = false;
            this._waitingToPause = true;
            DebuggerAgent.pause();
        }

        this._clearInterface();
    },

    _stepOverClicked: function()
    {
        if (!this._paused)
            return;

        this._paused = false;
        this._stepping = true;

        this._clearInterface();

        DebuggerAgent.stepOver();
    },

    _stepIntoClicked: function()
    {
        if (!this._paused)
            return;

        this._paused = false;
        this._stepping = true;

        this._clearInterface();

        DebuggerAgent.stepInto();
    },

    _stepOutClicked: function()
    {
        if (!this._paused)
            return;

        this._paused = false;
        this._stepping = true;

        this._clearInterface();

        DebuggerAgent.stepOut();
    },

    _toggleBreakpointsClicked: function(event)
    {
        WebInspector.debuggerModel.setBreakpointsActive(!WebInspector.debuggerModel.breakpointsActive());
    },

    _breakpointsActiveStateChanged: function(event)
    {
        var active = event.data;
        this._toggleBreakpointsButton.toggled = active;
        if (active) {
            this._toggleBreakpointsButton.title = WebInspector.UIString("Deactivate all breakpoints.");
            WebInspector.inspectorView.element.removeStyleClass("breakpoints-deactivated");
            this.sidebarPanes.jsBreakpoints.listElement.removeStyleClass("breakpoints-list-deactivated");
        } else {
            this._toggleBreakpointsButton.title = WebInspector.UIString("Activate all breakpoints.");
            WebInspector.inspectorView.element.addStyleClass("breakpoints-deactivated");
            this.sidebarPanes.jsBreakpoints.listElement.addStyleClass("breakpoints-list-deactivated");
        }
    },

    _evaluateSelectionInConsole: function()
    {
        var selection = window.getSelection();
        if (selection.type === "Range" && !selection.isCollapsed)
            WebInspector.evaluateInConsole(selection.toString());
    },

    _createDebugToolbar: function(section)
    {
        var debugToolbar = document.createElement("div");
        debugToolbar.className = "status-bar";
        debugToolbar.id = "scripts-debug-toolbar";

        var title, handler, shortcuts;
        var platformSpecificModifier = WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta;

        // Continue.
        title = WebInspector.UIString("Pause script execution (%s).");
        handler = this._togglePause.bind(this);
        shortcuts = [];
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.F8));
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.Slash, platformSpecificModifier));
        this.pauseButton = this._createButtonAndRegisterShortcuts(section, "scripts-pause", title, handler, shortcuts, WebInspector.UIString("Pause/Continue"));
        debugToolbar.appendChild(this.pauseButton);

        // Step over.
        title = WebInspector.UIString("Step over next function call (%s).");
        handler = this._stepOverClicked.bind(this);
        shortcuts = [];
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.F10));
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.SingleQuote, platformSpecificModifier));
        this.stepOverButton = this._createButtonAndRegisterShortcuts(section, "scripts-step-over", title, handler, shortcuts, WebInspector.UIString("Step over"));
        debugToolbar.appendChild(this.stepOverButton);

        // Step into.
        title = WebInspector.UIString("Step into next function call (%s).");
        handler = this._stepIntoClicked.bind(this);
        shortcuts = [];
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.F11));
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.Semicolon, platformSpecificModifier));
        this.stepIntoButton = this._createButtonAndRegisterShortcuts(section, "scripts-step-into", title, handler, shortcuts, WebInspector.UIString("Step into"));
        debugToolbar.appendChild(this.stepIntoButton);

        // Step out.
        title = WebInspector.UIString("Step out of current function (%s).");
        handler = this._stepOutClicked.bind(this);
        shortcuts = [];
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.F11, WebInspector.KeyboardShortcut.Modifiers.Shift));
        shortcuts.push(WebInspector.KeyboardShortcut.makeDescriptor(WebInspector.KeyboardShortcut.Keys.Semicolon, WebInspector.KeyboardShortcut.Modifiers.Shift | platformSpecificModifier));
        this.stepOutButton = this._createButtonAndRegisterShortcuts(section, "scripts-step-out", title, handler, shortcuts, WebInspector.UIString("Step out"));
        debugToolbar.appendChild(this.stepOutButton);

        this._toggleBreakpointsButton = new WebInspector.StatusBarButton(WebInspector.UIString("Deactivate all breakpoints."), "toggle-breakpoints");
        this._toggleBreakpointsButton.toggled = true;
        this._toggleBreakpointsButton.addEventListener("click", this._toggleBreakpointsClicked, this);
        debugToolbar.appendChild(this._toggleBreakpointsButton.element);

        this.debuggerStatusElement = document.createElement("div");
        this.debuggerStatusElement.id = "scripts-debugger-status";
        debugToolbar.appendChild(this.debuggerStatusElement);

        return debugToolbar;
    },

    _createButtonAndRegisterShortcuts: function(section, buttonId, buttonTitle, handler, shortcuts, shortcutDescription)
    {
        var button = document.createElement("button");
        button.className = "status-bar-item";
        button.id = buttonId;
        button.title = String.vsprintf(buttonTitle, [shortcuts[0].name]);
        button.disabled = true;
        button.appendChild(document.createElement("img"));
        button.addEventListener("click", handler, false);

        var shortcutNames = [];
        for (var i = 0; i < shortcuts.length; ++i) {
            this.registerShortcut(shortcuts[i].key, handler);
            shortcutNames.push(shortcuts[i].name);
        }
        section.addAlternateKeys(shortcutNames, shortcutDescription);

        return button;
    },

    searchCanceled: function()
    {
        if (this._searchView)
            this._searchView.searchCanceled();

        delete this._searchView;
        delete this._searchQuery;
    },

    /**
     * @param {string} query
     */
    performSearch: function(query)
    {
        WebInspector.searchController.updateSearchMatchesCount(0, this);

        if (!this.visibleView)
            return;

        // Call searchCanceled since it will reset everything we need before doing a new search.
        this.searchCanceled();

        this._searchView = this.visibleView;
        this._searchQuery = query;

        function finishedCallback(view, searchMatches)
        {
            if (!searchMatches)
                return;

            WebInspector.searchController.updateSearchMatchesCount(searchMatches, this);
            view.jumpToNextSearchResult();
            WebInspector.searchController.updateCurrentMatchIndex(view.currentSearchResultIndex, this);
        }

        this._searchView.performSearch(query, finishedCallback.bind(this));
    },

    jumpToNextSearchResult: function()
    {
        if (!this._searchView)
            return;

        if (this._searchView !== this.visibleView) {
            this.performSearch(this._searchQuery);
            return;
        }

        if (this._searchView.showingLastSearchResult())
            this._searchView.jumpToFirstSearchResult();
        else
            this._searchView.jumpToNextSearchResult();
        WebInspector.searchController.updateCurrentMatchIndex(this._searchView.currentSearchResultIndex, this);
        return true;
    },

    jumpToPreviousSearchResult: function()
    {
        if (!this._searchView)
            return false;

        if (this._searchView !== this.visibleView) {
            this.performSearch(this._searchQuery);
            if (this._searchView)
                this._searchView.jumpToLastSearchResult();
            return;
        }

        if (this._searchView.showingFirstSearchResult())
            this._searchView.jumpToLastSearchResult();
        else
            this._searchView.jumpToPreviousSearchResult();
        WebInspector.searchController.updateCurrentMatchIndex(this._searchView.currentSearchResultIndex, this);
    },

    /**
     * @return {boolean}
     */
    canSearchAndReplace: function()
    {
        var view = /** @type {WebInspector.SourceFrame} */ this.visibleView;
        return !!view && view.canEditSource();
    },

    /**
     * @param {string} text
     */
    replaceSelectionWith: function(text)
    {
        var view = /** @type {WebInspector.SourceFrame} */ this.visibleView;
        view.replaceSearchMatchWith(text);
    },

    /**
     * @param {string} query
     * @param {string} text
     */
    replaceAllWith: function(query, text)
    {
        var view = /** @type {WebInspector.SourceFrame} */ this.visibleView;
        view.replaceAllWith(query, text);
    },

    _toggleFormatSource: function()
    {
        this._toggleFormatSourceButton.toggled = !this._toggleFormatSourceButton.toggled;
        var uiSourceCodes = this._workspace.uiSourceCodes();
        for (var i = 0; i < uiSourceCodes.length; ++i)
            uiSourceCodes[i].setFormatted(this._toggleFormatSourceButton.toggled);
    },

    addToWatch: function(expression)
    {
        this.sidebarPanes.watchExpressions.addExpression(expression);
    },

    _toggleBreakpoint: function()
    {
        var sourceFrame = this.visibleView;
        if (!sourceFrame)
            return;

        if (sourceFrame instanceof WebInspector.JavaScriptSourceFrame) {
            var javaScriptSourceFrame = /** @type {WebInspector.JavaScriptSourceFrame} */ sourceFrame;
            javaScriptSourceFrame.toggleBreakpointOnCurrentLine();
        }            
    },

    _showOutlineDialog: function()
    {
        var uiSourceCode = this._editorContainer.currentFile();
        if (!uiSourceCode)
            return;

        if (uiSourceCode instanceof WebInspector.JavaScriptSource)
            WebInspector.JavaScriptOutlineDialog.show(this.visibleView, uiSourceCode);
        else if (uiSourceCode instanceof WebInspector.StyleSource)
            WebInspector.StyleSheetOutlineDialog.show(this.visibleView, /** @type {WebInspector.StyleSource} */ uiSourceCode);
    },

    _installDebuggerSidebarController: function()
    {
        this._toggleDebuggerSidebarButton = new WebInspector.StatusBarButton(WebInspector.UIString("Hide debugger"), "scripts-debugger-show-hide-button", 3);
        this._toggleDebuggerSidebarButton.state = "shown";
        this._toggleDebuggerSidebarButton.addEventListener("click", clickHandler, this);

        function clickHandler()
        {
            if (this._toggleDebuggerSidebarButton.state === "shown")
                this._hideDebuggerSidebar();
            else
                this._showDebuggerSidebar();
        }
        this.editorView.element.appendChild(this._toggleDebuggerSidebarButton.element);

        if (WebInspector.settings.debuggerSidebarHidden.get())
            this._hideDebuggerSidebar();

    },

    _showDebuggerSidebar: function()
    {
        if (this._toggleDebuggerSidebarButton.state === "shown")
            return;
        this._toggleDebuggerSidebarButton.state = "shown";
        this._toggleDebuggerSidebarButton.title = WebInspector.UIString("Hide debugger");
        this.splitView.showSidebarElement();
        WebInspector.settings.debuggerSidebarHidden.set(false);
    },

    _hideDebuggerSidebar: function()
    {
        if (this._toggleDebuggerSidebarButton.state === "hidden")
            return;
        this._toggleDebuggerSidebarButton.state = "hidden";
        this._toggleDebuggerSidebarButton.title = WebInspector.UIString("Show debugger");
        this.splitView.hideSidebarElement();
        WebInspector.settings.debuggerSidebarHidden.set(true);
    },

    _fileRenamed: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data.uiSourceCode;
        var name = /** @type {string} */ event.data.name;
        if (!uiSourceCode.isSnippet)
            return;
        var snippetJavaScriptSource = /** @type {WebInspector.SnippetJavaScriptSource} */ uiSourceCode;
        WebInspector.scriptSnippetModel.renameScriptSnippet(snippetJavaScriptSource, name);
    },
        
    /**
     * @param {WebInspector.Event} event
     */
    _snippetCreationRequested: function(event)
    {
        var snippetJavaScriptSource = WebInspector.scriptSnippetModel.createScriptSnippet();
        this._showSourceLine(snippetJavaScriptSource);
        
        var shouldHideNavigator = !this._navigatorController.isNavigatorPinned();
        if (this._navigatorController.isNavigatorHidden())
            this._navigatorController.showNavigatorOverlay();
        this._navigator.rename(snippetJavaScriptSource, callback.bind(this));
    
        /**
         * @param {boolean} committed
         */
        function callback(committed)
        {
            if (shouldHideNavigator)
                this._navigatorController.hideNavigatorOverlay();

            if (!committed) {
                WebInspector.scriptSnippetModel.deleteScriptSnippet(snippetJavaScriptSource);
                return;
            }

            this._showSourceLine(snippetJavaScriptSource);
        }
    },

    /**
     * @param {WebInspector.Event} event
     */
    _itemRenamingRequested: function(event)
    {
        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ event.data;
        
        var shouldHideNavigator = !this._navigatorController.isNavigatorPinned();
        if (this._navigatorController.isNavigatorHidden())
            this._navigatorController.showNavigatorOverlay();
        this._navigator.rename(uiSourceCode, callback.bind(this));
    
        /**
         * @param {boolean} committed
         */
        function callback(committed)
        {
            if (shouldHideNavigator && committed) {
                this._navigatorController.hideNavigatorOverlay();
                this._showSourceLine(uiSourceCode);
            }
        }
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     */
    _showLocalHistory: function(uiSourceCode)
    {
        WebInspector.RevisionHistoryView.showHistory(uiSourceCode);
    },

    /** 
     * @param {WebInspector.ContextMenu} contextMenu
     * @param {Object} target
     */
    appendApplicableItems: function(contextMenu, target)
    {
        if (!(target instanceof WebInspector.UISourceCode))
            return;

        var uiSourceCode = /** @type {WebInspector.UISourceCode} */ target;
        contextMenu.appendItem(WebInspector.UIString("Local modifications..."), this._showLocalHistory.bind(this, uiSourceCode));
        if (uiSourceCode.resource() && uiSourceCode.resource().request)
            contextMenu.appendApplicableItems(uiSourceCode.resource().request);
    },

    showGoToSourceDialog: function()
    {
        WebInspector.OpenResourceDialog.show(this, this._workspace, this.editorView.mainElement);
    }
}

WebInspector.ScriptsPanel.prototype.__proto__ = WebInspector.Panel.prototype;
