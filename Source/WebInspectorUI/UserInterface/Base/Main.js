/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ContentViewCookieType = {
    ApplicationCache: "application-cache",
    CookieStorage: "cookie-storage",
    Database: "database",
    DatabaseTable: "database-table",
    DOMStorage: "dom-storage",
    Resource: "resource", // includes Frame too.
    Timelines: "timelines"
};

WebInspector.DebuggableType = {
    Web: "web",
    JavaScript: "javascript"
};

WebInspector.SelectedSidebarPanelCookieKey = "selected-sidebar-panel";
WebInspector.TypeIdentifierCookieKey = "represented-object-type";

WebInspector.StateRestorationType = {
    Load: "state-restoration-load",
    Navigation: "state-restoration-navigation",
    Delayed: "state-restoration-delayed",
};

WebInspector.LayoutDirection = {
    System: "system",
    LTR: "ltr",
    RTL: "rtl",
};

WebInspector.loaded = function()
{
    // Initialize WebSocket to communication.
    this._initializeWebSocketIfNeeded();

    this.debuggableType = InspectorFrontendHost.debuggableType() === "web" ? WebInspector.DebuggableType.Web : WebInspector.DebuggableType.JavaScript;
    this.hasExtraDomains = false;

    // Register observers for events from the InspectorBackend.
    if (InspectorBackend.registerInspectorDispatcher)
        InspectorBackend.registerInspectorDispatcher(new WebInspector.InspectorObserver);
    if (InspectorBackend.registerPageDispatcher)
        InspectorBackend.registerPageDispatcher(new WebInspector.PageObserver);
    if (InspectorBackend.registerConsoleDispatcher)
        InspectorBackend.registerConsoleDispatcher(new WebInspector.ConsoleObserver);
    if (InspectorBackend.registerNetworkDispatcher)
        InspectorBackend.registerNetworkDispatcher(new WebInspector.NetworkObserver);
    if (InspectorBackend.registerDOMDispatcher)
        InspectorBackend.registerDOMDispatcher(new WebInspector.DOMObserver);
    if (InspectorBackend.registerDebuggerDispatcher)
        InspectorBackend.registerDebuggerDispatcher(new WebInspector.DebuggerObserver);
    if (InspectorBackend.registerHeapDispatcher)
        InspectorBackend.registerHeapDispatcher(new WebInspector.HeapObserver);
    if (InspectorBackend.registerMemoryDispatcher)
        InspectorBackend.registerMemoryDispatcher(new WebInspector.MemoryObserver);
    if (InspectorBackend.registerDatabaseDispatcher)
        InspectorBackend.registerDatabaseDispatcher(new WebInspector.DatabaseObserver);
    if (InspectorBackend.registerDOMStorageDispatcher)
        InspectorBackend.registerDOMStorageDispatcher(new WebInspector.DOMStorageObserver);
    if (InspectorBackend.registerApplicationCacheDispatcher)
        InspectorBackend.registerApplicationCacheDispatcher(new WebInspector.ApplicationCacheObserver);
    if (InspectorBackend.registerScriptProfilerDispatcher)
        InspectorBackend.registerScriptProfilerDispatcher(new WebInspector.ScriptProfilerObserver);
    if (InspectorBackend.registerTimelineDispatcher)
        InspectorBackend.registerTimelineDispatcher(new WebInspector.TimelineObserver);
    if (InspectorBackend.registerCSSDispatcher)
        InspectorBackend.registerCSSDispatcher(new WebInspector.CSSObserver);
    if (InspectorBackend.registerLayerTreeDispatcher)
        InspectorBackend.registerLayerTreeDispatcher(new WebInspector.LayerTreeObserver);
    if (InspectorBackend.registerRuntimeDispatcher)
        InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeObserver);
    if (InspectorBackend.registerWorkerDispatcher)
        InspectorBackend.registerWorkerDispatcher(new WebInspector.WorkerObserver);
    if (InspectorBackend.registerReplayDispatcher)
        InspectorBackend.registerReplayDispatcher(new WebInspector.ReplayObserver);

    // Main backend target.
    WebInspector.mainTarget = new WebInspector.MainTarget;

    // Enable agents.
    InspectorAgent.enable();

    // Perform one-time tasks.
    WebInspector.CSSCompletions.requestCSSCompletions();

    // Listen for the ProvisionalLoadStarted event before registering for events so our code gets called before any managers or sidebars.
    // This lets us save a state cookie before any managers or sidebars do any resets that would affect state (namely TimelineManager).
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ProvisionalLoadStarted, this._provisionalLoadStarted, this);

    // Create the singleton managers next, before the user interface elements, so the user interface can register
    // as event listeners on these managers.
    this.targetManager = new WebInspector.TargetManager;
    this.branchManager = new WebInspector.BranchManager;
    this.frameResourceManager = new WebInspector.FrameResourceManager;
    this.storageManager = new WebInspector.StorageManager;
    this.domTreeManager = new WebInspector.DOMTreeManager;
    this.cssStyleManager = new WebInspector.CSSStyleManager;
    this.logManager = new WebInspector.LogManager;
    this.issueManager = new WebInspector.IssueManager;
    this.analyzerManager = new WebInspector.AnalyzerManager;
    this.runtimeManager = new WebInspector.RuntimeManager;
    this.heapManager = new WebInspector.HeapManager;
    this.memoryManager = new WebInspector.MemoryManager;
    this.applicationCacheManager = new WebInspector.ApplicationCacheManager;
    this.timelineManager = new WebInspector.TimelineManager;
    this.debuggerManager = new WebInspector.DebuggerManager;
    this.sourceMapManager = new WebInspector.SourceMapManager;
    this.layerTreeManager = new WebInspector.LayerTreeManager;
    this.dashboardManager = new WebInspector.DashboardManager;
    this.probeManager = new WebInspector.ProbeManager;
    this.workerManager = new WebInspector.WorkerManager;
    this.replayManager = new WebInspector.ReplayManager;

    // Enable the Console Agent after creating the singleton managers.
    ConsoleAgent.enable();

    // Tell the backend we are initialized after all our initialization messages have been sent.
    setTimeout(function() {
        // COMPATIBILITY (iOS 8): Inspector.initialized did not exist yet.
        if (InspectorAgent.initialized)
            InspectorAgent.initialized();
    }, 0);

    // Register for events.
    this.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStarted, this._captureDidStart, this);
    this.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
    this.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
    this.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.InspectModeStateChanged, this._inspectModeStateChanged, this);
    this.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.DOMNodeWasInspected, this._domNodeWasInspected, this);
    this.storageManager.addEventListener(WebInspector.StorageManager.Event.DOMStorageObjectWasInspected, this._storageWasInspected, this);
    this.storageManager.addEventListener(WebInspector.StorageManager.Event.DatabaseWasInspected, this._storageWasInspected, this);
    this.frameResourceManager.addEventListener(WebInspector.FrameResourceManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);
    this.frameResourceManager.addEventListener(WebInspector.FrameResourceManager.Event.FrameWasAdded, this._frameWasAdded, this);

    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

    document.addEventListener("DOMContentLoaded", this.contentLoaded.bind(this));

    // Create settings.
    this._showingSplitConsoleSetting = new WebInspector.Setting("showing-split-console", false);
    this._splitConsoleHeightSetting = new WebInspector.Setting("split-console-height", 150);

    this._openTabsSetting = new WebInspector.Setting("open-tab-types", ["elements", "network", "resources", "timeline", "debugger", "storage", "console"]);
    this._selectedTabIndexSetting = new WebInspector.Setting("selected-tab-index", 0);

    this.showShadowDOMSetting = new WebInspector.Setting("show-shadow-dom", false);
    this.showReplayInterfaceSetting = new WebInspector.Setting("show-web-replay", false);

    // COMPATIBILITY (iOS 8): Page.enableTypeProfiler did not exist.
    this.showJavaScriptTypeInformationSetting = new WebInspector.Setting("show-javascript-type-information", false);
    this.showJavaScriptTypeInformationSetting.addEventListener(WebInspector.Setting.Event.Changed, this._showJavaScriptTypeInformationSettingChanged, this);
    if (this.showJavaScriptTypeInformationSetting.value && window.RuntimeAgent && RuntimeAgent.enableTypeProfiler)
        RuntimeAgent.enableTypeProfiler();

    this.enableControlFlowProfilerSetting = new WebInspector.Setting("enable-control-flow-profiler", false);
    this.enableControlFlowProfilerSetting.addEventListener(WebInspector.Setting.Event.Changed, this._enableControlFlowProfilerSettingChanged, this);
    if (this.enableControlFlowProfilerSetting.value && window.RuntimeAgent && RuntimeAgent.enableControlFlowProfiler)
        RuntimeAgent.enableControlFlowProfiler();

    // COMPATIBILITY (iOS 8): Page.setShowPaintRects did not exist.
    this.showPaintRectsSetting = new WebInspector.Setting("show-paint-rects", false);
    if (this.showPaintRectsSetting.value && window.PageAgent && PageAgent.setShowPaintRects)
        PageAgent.setShowPaintRects(true);

    this.showPrintStylesSetting = new WebInspector.Setting("show-print-styles", false);
    if (this.showPrintStylesSetting.value && window.PageAgent)
        PageAgent.setEmulatedMedia("print");

    this.setZoomFactor(WebInspector.settings.zoomFactor.value);

    this.mouseCoords = {
        x: 0,
        y: 0
    };

    this.visible = false;

    this._windowKeydownListeners = [];
};

WebInspector.contentLoaded = function()
{
    // If there was an uncaught exception earlier during loading, then
    // abort loading more content. We could be in an inconsistent state.
    if (window.__uncaughtExceptions)
        return;

    // Register for global events.
    document.addEventListener("beforecopy", this._beforecopy.bind(this));
    document.addEventListener("copy", this._copy.bind(this));

    document.addEventListener("click", this._mouseWasClicked.bind(this));
    document.addEventListener("dragover", this._dragOver.bind(this));
    document.addEventListener("focus", WebInspector._focusChanged.bind(this), true);

    window.addEventListener("focus", this._windowFocused.bind(this));
    window.addEventListener("blur", this._windowBlurred.bind(this));
    window.addEventListener("resize", this._windowResized.bind(this));
    window.addEventListener("keydown", this._windowKeyDown.bind(this));
    window.addEventListener("keyup", this._windowKeyUp.bind(this));
    window.addEventListener("mousedown", this._mouseDown.bind(this), true);
    window.addEventListener("mousemove", this._mouseMoved.bind(this), true);
    window.addEventListener("pagehide", this._pageHidden.bind(this));
    window.addEventListener("contextmenu", this._contextMenuRequested.bind(this));

    // Add platform style classes so the UI can be tweaked per-platform.
    document.body.classList.add(WebInspector.Platform.name + "-platform");
    if (WebInspector.Platform.isNightlyBuild)
        document.body.classList.add("nightly-build");

    if (WebInspector.Platform.name === "mac") {
        document.body.classList.add(WebInspector.Platform.version.name);

        if (WebInspector.Platform.version.release >= 11)
            document.body.classList.add("latest-mac");
        else
            document.body.classList.add("legacy-mac");
    }

    document.body.classList.add(this.debuggableType);
    document.body.setAttribute("dir", this.resolvedLayoutDirection());

    function setTabSize() {
        document.body.style.tabSize = WebInspector.settings.tabSize.value;
    }
    WebInspector.settings.tabSize.addEventListener(WebInspector.Setting.Event.Changed, setTabSize);
    setTabSize();

    function setInvalidCharacterClassName() {
        document.body.classList.toggle("show-invalid-characters", WebInspector.settings.showInvalidCharacters.value);
    }
    WebInspector.settings.showInvalidCharacters.addEventListener(WebInspector.Setting.Event.Changed, setInvalidCharacterClassName);
    setInvalidCharacterClassName();

    function setWhitespaceCharacterClassName() {
        document.body.classList.toggle("show-whitespace-characters", WebInspector.settings.showWhitespaceCharacters.value);
    }
    WebInspector.settings.showWhitespaceCharacters.addEventListener(WebInspector.Setting.Event.Changed, setWhitespaceCharacterClassName);
    setWhitespaceCharacterClassName();

    this.settingsTabContentView = new WebInspector.SettingsTabContentView;

    this._settingsKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Comma, this._showSettingsTab.bind(this));

    // Create the user interface elements.
    this.toolbar = new WebInspector.Toolbar(document.getElementById("toolbar"), null, true);
    this.toolbar.displayMode = WebInspector.Toolbar.DisplayMode.IconOnly;
    this.toolbar.sizeMode = WebInspector.Toolbar.SizeMode.Small;

    this.tabBar = new WebInspector.TabBar(document.getElementById("tab-bar"));
    this.tabBar.addEventListener(WebInspector.TabBar.Event.OpenDefaultTab, this._openDefaultTab, this);

    this._contentElement = document.getElementById("content");
    this._contentElement.setAttribute("role", "main");
    this._contentElement.setAttribute("aria-label", WebInspector.UIString("Content"));

    const disableBackForward = true;
    const disableFindBanner = false;
    this.splitContentBrowser = new WebInspector.ContentBrowser(document.getElementById("split-content-browser"), this, disableBackForward, disableFindBanner);
    this.splitContentBrowser.navigationBar.element.addEventListener("mousedown", this._consoleResizerMouseDown.bind(this));

    this.clearKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "K", this._clear.bind(this));

    this.quickConsole = new WebInspector.QuickConsole(document.getElementById("quick-console"));
    this.quickConsole.addEventListener(WebInspector.QuickConsole.Event.DidResize, this._quickConsoleDidResize, this);

    this._consoleRepresentedObject = new WebInspector.LogObject;
    this._consoleTreeElement = new WebInspector.LogTreeElement(this._consoleRepresentedObject);
    this.consoleContentView = WebInspector.splitContentBrowser.contentViewForRepresentedObject(this._consoleRepresentedObject);
    this.consoleLogViewController = this.consoleContentView.logViewController;
    this.breakpointPopoverController = new WebInspector.BreakpointPopoverController;

    // FIXME: The sidebars should be flipped in RTL languages.
    this.navigationSidebar = new WebInspector.Sidebar(document.getElementById("navigation-sidebar"), WebInspector.Sidebar.Sides.Left);
    this.navigationSidebar.addEventListener(WebInspector.Sidebar.Event.WidthDidChange, this._sidebarWidthDidChange, this);

    this.detailsSidebar = new WebInspector.Sidebar(document.getElementById("details-sidebar"), WebInspector.Sidebar.Sides.Right, null, null, WebInspector.UIString("Details"), true);
    this.detailsSidebar.addEventListener(WebInspector.Sidebar.Event.WidthDidChange, this._sidebarWidthDidChange, this);

    this.searchKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "F", this._focusSearchField.bind(this));
    this._findKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "F", this._find.bind(this));
    this._saveKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "S", this._save.bind(this));
    this._saveAsKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Shift | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "S", this._saveAs.bind(this));

    this.openResourceKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "O", this._showOpenResourceDialog.bind(this));
    new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "P", this._showOpenResourceDialog.bind(this));

    this.navigationSidebarKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "0", this.toggleNavigationSidebar.bind(this));
    this.detailsSidebarKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Option, "0", this.toggleDetailsSidebar.bind(this));

    let boundIncreaseZoom = this._increaseZoom.bind(this);
    let boundDecreaseZoom = this._decreaseZoom.bind(this);
    this._increaseZoomKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Plus, boundIncreaseZoom);
    this._decreaseZoomKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Minus, boundDecreaseZoom);
    this._increaseZoomKeyboardShortcut2 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Plus, boundIncreaseZoom);
    this._decreaseZoomKeyboardShortcut2 = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, WebInspector.KeyboardShortcut.Key.Minus, boundDecreaseZoom);
    this._resetZoomKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "0", this._resetZoom.bind(this));

    this._showTabAtIndexKeyboardShortcuts = [1, 2, 3, 4, 5, 6, 7, 8, 9].map((i) => new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Option, `${i}`, this._showTabAtIndex.bind(this, i)));
    this._openNewTabKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Option, "T", this.showNewTabTab.bind(this));

    this.tabBrowser = new WebInspector.TabBrowser(document.getElementById("tab-browser"), this.tabBar, this.navigationSidebar, this.detailsSidebar);
    this.tabBrowser.addEventListener(WebInspector.TabBrowser.Event.SelectedTabContentViewDidChange, this._tabBrowserSelectedTabContentViewDidChange, this);

    this._reloadPageKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "R", this._reloadPage.bind(this));
    this._reloadPageIgnoringCacheKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "R", this._reloadPageIgnoringCache.bind(this));
    this._reloadPageKeyboardShortcut.implicitlyPreventsDefault = this._reloadPageIgnoringCacheKeyboardShortcut.implicitlyPreventsDefault = false;

    this._consoleTabKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Option | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "C", this._showConsoleTab.bind(this));
    this._quickConsoleKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control, WebInspector.KeyboardShortcut.Key.Apostrophe, this._focusConsolePrompt.bind(this));

    this._inspectModeKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "C", this._toggleInspectMode.bind(this));

    this._undoKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "Z", this._undoKeyboardShortcut.bind(this));
    this._redoKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "Z", this._redoKeyboardShortcut.bind(this));
    this._undoKeyboardShortcut.implicitlyPreventsDefault = this._redoKeyboardShortcut.implicitlyPreventsDefault = false;

    this.toggleBreakpointsKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "Y", this.debuggerToggleBreakpoints.bind(this));
    this.pauseOrResumeKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "Y", this.debuggerPauseResumeToggle.bind(this));
    this.stepOverKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.F6, this.debuggerStepOver.bind(this));
    this.stepIntoKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.F7, this.debuggerStepInto.bind(this));
    this.stepOutKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.F8, this.debuggerStepOut.bind(this));

    this.pauseOrResumeAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Backslash, this.debuggerPauseResumeToggle.bind(this));
    this.stepOverAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.SingleQuote, this.debuggerStepOver.bind(this));
    this.stepIntoAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Semicolon, this.debuggerStepInto.bind(this));
    this.stepOutAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Shift | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Semicolon, this.debuggerStepOut.bind(this));

    this._closeToolbarButton = new WebInspector.ControlToolbarItem("dock-close", WebInspector.UIString("Close"), "Images/Close.svg", 16, 14);
    this._closeToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this.close, this);

    this._undockToolbarButton = new WebInspector.ButtonToolbarItem("undock", WebInspector.UIString("Detach into separate window"), null, "Images/Undock.svg");
    this._undockToolbarButton.element.classList.add(WebInspector.Popover.IgnoreAutoDismissClassName);
    this._undockToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._undock, this);

    let dockImage = WebInspector.resolvedLayoutDirection() === WebInspector.LayoutDirection.RTL ? "Images/DockLeft.svg" : "Images/DockRight.svg";
    this._dockToSideToolbarButton = new WebInspector.ButtonToolbarItem("dock-right", WebInspector.UIString("Dock to side of window"), null, dockImage);
    this._dockToSideToolbarButton.element.classList.add(WebInspector.Popover.IgnoreAutoDismissClassName);

    let dockToSideCallback = WebInspector.resolvedLayoutDirection() === WebInspector.LayoutDirection.RTL ? this._dockLeft : this._dockRight;
    this._dockToSideToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, dockToSideCallback, this);

    this._dockBottomToolbarButton = new WebInspector.ButtonToolbarItem("dock-bottom", WebInspector.UIString("Dock to bottom of window"), null, "Images/DockBottom.svg");
    this._dockBottomToolbarButton.element.classList.add(WebInspector.Popover.IgnoreAutoDismissClassName);
    this._dockBottomToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._dockBottom, this);

    this._togglePreviousDockConfigurationKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "D", this._togglePreviousDockConfiguration.bind(this));

    var toolTip;
    if (WebInspector.debuggableType === WebInspector.DebuggableType.JavaScript)
        toolTip = WebInspector.UIString("Restart (%s)").format(this._reloadPageKeyboardShortcut.displayName);
    else
        toolTip = WebInspector.UIString("Reload page (%s)\nReload ignoring cache (%s)").format(this._reloadPageKeyboardShortcut.displayName, this._reloadPageIgnoringCacheKeyboardShortcut.displayName);

    this._reloadToolbarButton = new WebInspector.ButtonToolbarItem("reload", toolTip, null, "Images/ReloadToolbar.svg");
    this._reloadToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._reloadPageClicked, this);

    this._downloadToolbarButton = new WebInspector.ButtonToolbarItem("download", WebInspector.UIString("Download Web Archive"), null, "Images/DownloadArrow.svg");
    this._downloadToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._downloadWebArchive, this);

    this._updateReloadToolbarButton();
    this._updateDownloadToolbarButton();

    // The toolbar button for node inspection.
    if (this.debuggableType === WebInspector.DebuggableType.Web) {
        var toolTip = WebInspector.UIString("Start element selection (%s)").format(WebInspector._inspectModeKeyboardShortcut.displayName);
        var activatedToolTip = WebInspector.UIString("Stop element selection (%s)").format(WebInspector._inspectModeKeyboardShortcut.displayName);
        this._inspectModeToolbarButton = new WebInspector.ActivateButtonToolbarItem("inspect", toolTip, activatedToolTip, null, "Images/Crosshair.svg");
        this._inspectModeToolbarButton.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._toggleInspectMode, this);
    }

    this._dashboardContainer = new WebInspector.DashboardContainerView;
    this._dashboardContainer.showDashboardViewForRepresentedObject(this.dashboardManager.dashboards.default);

    this._searchToolbarItem = new WebInspector.SearchBar("inspector-search", WebInspector.UIString("Search"), null, true);
    this._searchToolbarItem.addEventListener(WebInspector.SearchBar.Event.TextChanged, this._searchTextDidChange, this);

    this.toolbar.addToolbarItem(this._closeToolbarButton, WebInspector.Toolbar.Section.Control);

    this.toolbar.addToolbarItem(this._undockToolbarButton, WebInspector.Toolbar.Section.Left);
    this.toolbar.addToolbarItem(this._dockToSideToolbarButton, WebInspector.Toolbar.Section.Left);
    this.toolbar.addToolbarItem(this._dockBottomToolbarButton, WebInspector.Toolbar.Section.Left);

    this.toolbar.addToolbarItem(this._reloadToolbarButton, WebInspector.Toolbar.Section.CenterLeft);
    this.toolbar.addToolbarItem(this._downloadToolbarButton, WebInspector.Toolbar.Section.CenterLeft);

    this.toolbar.addToolbarItem(this._dashboardContainer.toolbarItem, WebInspector.Toolbar.Section.Center);

    if (this._inspectModeToolbarButton)
        this.toolbar.addToolbarItem(this._inspectModeToolbarButton, WebInspector.Toolbar.Section.CenterRight);

    this.toolbar.addToolbarItem(this._searchToolbarItem, WebInspector.Toolbar.Section.Right);

    this.resourceDetailsSidebarPanel = new WebInspector.ResourceDetailsSidebarPanel;
    this.domNodeDetailsSidebarPanel = new WebInspector.DOMNodeDetailsSidebarPanel;
    this.cssStyleDetailsSidebarPanel = new WebInspector.CSSStyleDetailsSidebarPanel;
    this.applicationCacheDetailsSidebarPanel = new WebInspector.ApplicationCacheDetailsSidebarPanel;
    this.indexedDatabaseDetailsSidebarPanel = new WebInspector.IndexedDatabaseDetailsSidebarPanel;
    this.scopeChainDetailsSidebarPanel = new WebInspector.ScopeChainDetailsSidebarPanel;
    this.probeDetailsSidebarPanel = new WebInspector.ProbeDetailsSidebarPanel;

    if (window.LayerTreeAgent)
        this.layerTreeDetailsSidebarPanel = new WebInspector.LayerTreeDetailsSidebarPanel;

    this.modifierKeys = {altKey: false, metaKey: false, shiftKey: false};

    let dockedResizerElement = document.getElementById("docked-resizer");
    dockedResizerElement.classList.add(WebInspector.Popover.IgnoreAutoDismissClassName);
    dockedResizerElement.addEventListener("mousedown", this._dockedResizerMouseDown.bind(this));

    this._dockingAvailable = false;

    this._updateDockNavigationItems();
    this._setupViewHierarchy();

    // These tabs are always available for selecting, modulo isTabAllowed().
    // Other tabs may be engineering-only or toggled at runtime if incomplete.
    let productionTabClasses = [
        WebInspector.ConsoleTabContentView,
        WebInspector.DebuggerTabContentView,
        WebInspector.ElementsTabContentView,
        WebInspector.NetworkTabContentView,
        WebInspector.NewTabContentView,
        WebInspector.ResourcesTabContentView,
        WebInspector.SearchTabContentView,
        WebInspector.SettingsTabContentView,
        WebInspector.StorageTabContentView,
        WebInspector.TimelineTabContentView,
    ];

    this._knownTabClassesByType = new Map;
    // Set tab classes directly. The public API triggers other updates and
    // notifications that won't work or have no listeners at this point.
    for (let tabClass of productionTabClasses)
        this._knownTabClassesByType.set(tabClass.Type, tabClass);

    this._pendingOpenTabs = [];

    // Previously we may have stored duplicates in this setting. Avoid creating duplicate tabs.
    let openTabTypes = this._openTabsSetting.value;
    let seenTabTypes = new Set;

    for (let i = 0; i < openTabTypes.length; ++i) {
        let tabType = openTabTypes[i];

        if (seenTabTypes.has(tabType))
            continue;
        seenTabTypes.add(tabType);

        if (!this.isTabTypeAllowed(tabType)) {
            this._pendingOpenTabs.push({tabType, index: i});
            continue;
        }

        let tabContentView = this._createTabContentViewForType(tabType);
        if (!tabContentView)
            continue;
        this.tabBrowser.addTabForContentView(tabContentView, true);
    }

    this._restoreCookieForOpenTabs(WebInspector.StateRestorationType.Load);

    this.tabBar.selectedTabBarItem = this._selectedTabIndexSetting.value;

    if (!this.tabBar.selectedTabBarItem)
        this.tabBar.selectedTabBarItem = 0;

    if (!this.tabBar.normalTabCount)
        this.showNewTabTab();

    // Listen to the events after restoring the saved tabs to avoid recursion.
    this.tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemAdded, this._rememberOpenTabs, this);
    this.tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemRemoved, this._rememberOpenTabs, this);
    this.tabBar.addEventListener(WebInspector.TabBar.Event.TabBarItemsReordered, this._rememberOpenTabs, this);

    // Signal that the frontend is now ready to receive messages.
    InspectorFrontendAPI.loadCompleted();

    // Tell the InspectorFrontendHost we loaded, which causes the window to display
    // and pending InspectorFrontendAPI commands to be sent.
    InspectorFrontendHost.loaded();

    this._updateSplitConsoleHeight(this._splitConsoleHeightSetting.value);

    if (this._showingSplitConsoleSetting.value)
        this.showSplitConsole();

    // Store this on the window in case the WebInspector global gets corrupted.
    window.__frontendCompletedLoad = true;

    if (this.runBootstrapOperations)
        this.runBootstrapOperations();
};

WebInspector.isTabTypeAllowed = function(tabType)
{
    let tabClass = this._knownTabClassesByType.get(tabType);
    if (!tabClass)
        return false;

    return tabClass.isTabAllowed();
};

WebInspector.knownTabClasses = function()
{
    return new Set(this._knownTabClassesByType.values());
};

WebInspector._showOpenResourceDialog = function()
{
    if (!this._openResourceDialog)
        this._openResourceDialog = new WebInspector.OpenResourceDialog(this);

    if (this._openResourceDialog.visible)
        return;

    this._openResourceDialog.present(this._contentElement);
};

WebInspector._createTabContentViewForType = function(tabType)
{
    let tabClass = this._knownTabClassesByType.get(tabType);
    if (!tabClass) {
        console.error("Unknown tab type", tabType);
        return null;
    }

    console.assert(WebInspector.TabContentView.isPrototypeOf(tabClass));
    return new tabClass;
};

WebInspector._rememberOpenTabs = function()
{
    let seenTabTypes = new Set;
    let openTabs = [];

    for (let tabBarItem of this.tabBar.tabBarItems) {
        let tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WebInspector.TabContentView))
            continue;
        if (!tabContentView.constructor.shouldSaveTab())
            continue;
        console.assert(tabContentView.type, "Tab type can't be null, undefined, or empty string", tabContentView.type, tabContentView);
        openTabs.push(tabContentView.type);
        seenTabTypes.add(tabContentView.type);
    }

    // Keep currently unsupported tabs in the setting at their previous index.
    for (let {tabType, index} of this._pendingOpenTabs) {
        if (seenTabTypes.has(tabType))
            continue;
        openTabs.insertAtIndex(tabType, index);
        seenTabTypes.add(tabType);
    }

    this._openTabsSetting.value = openTabs;
};

WebInspector._openDefaultTab = function(event)
{
    this.showNewTabTab();
};

WebInspector._showSettingsTab = function(event)
{
    this.tabBrowser.showTabForContentView(this.settingsTabContentView);
};

WebInspector._tryToRestorePendingTabs = function()
{
    let stillPendingOpenTabs = [];
    for (let {tabType, index} of this._pendingOpenTabs) {
        if (!this.isTabTypeAllowed(tabType)) {
            stillPendingOpenTabs.push({tabType, index});
            continue;
        }

        let tabContentView = this._createTabContentViewForType(tabType);
        if (!tabContentView)
            continue;

        this.tabBrowser.addTabForContentView(tabContentView, true, index);

        tabContentView.restoreStateFromCookie(WebInspector.StateRestorationType.Load);
    }

    this._pendingOpenTabs = stillPendingOpenTabs;

    this.tabBrowser.tabBar.updateNewTabTabBarItemState();
};

WebInspector.showNewTabTab = function(shouldAnimate)
{
    if (!this.isNewTabWithTypeAllowed(WebInspector.NewTabContentView.Type))
        return;

    let tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.NewTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.NewTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView, !shouldAnimate);
};

WebInspector.isNewTabWithTypeAllowed = function(tabType)
{
    let tabClass = this._knownTabClassesByType.get(tabType);
    if (!tabClass || !tabClass.isTabAllowed())
        return false;

    // Only allow one tab per class for now.
    for (let tabBarItem of this.tabBar.tabBarItems) {
        let tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WebInspector.TabContentView))
            continue;
        if (tabContentView.constructor === tabClass)
            return false;
    }

    if (tabClass === WebInspector.NewTabContentView) {
        let allTabs = Array.from(this.knownTabClasses());
        let addableTabs = allTabs.filter((tabClass) => !tabClass.isEphemeral());
        let canMakeNewTab = addableTabs.some((tabClass) => WebInspector.isNewTabWithTypeAllowed(tabClass.Type));
        return canMakeNewTab;
    }

    return true;
};

WebInspector.createNewTabWithType = function(tabType, options = {})
{
    console.assert(this.isNewTabWithTypeAllowed(tabType));

    let {referencedView, shouldReplaceTab, shouldShowNewTab} = options;
    console.assert(!referencedView || referencedView instanceof WebInspector.TabContentView, referencedView);
    console.assert(!shouldReplaceTab || referencedView, "Must provide a reference view to replace a tab.");

    let tabContentView = this._createTabContentViewForType(tabType);
    const suppressAnimations = true;
    let insertionIndex = referencedView ? this.tabBar.tabBarItems.indexOf(referencedView.tabBarItem) : undefined;
    this.tabBrowser.addTabForContentView(tabContentView, suppressAnimations, insertionIndex);

    if (shouldReplaceTab)
        this.tabBrowser.closeTabForContentView(referencedView, suppressAnimations);

    if (shouldShowNewTab)
        this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.registerTabClass = function(tabClass)
{
    console.assert(WebInspector.TabContentView.isPrototypeOf(tabClass));
    if (!WebInspector.TabContentView.isPrototypeOf(tabClass))
        return;

    if (this._knownTabClassesByType.has(tabClass.Type))
        return;

    this._knownTabClassesByType.set(tabClass.Type, tabClass);

    this._tryToRestorePendingTabs();
    this.notifications.dispatchEventToListeners(WebInspector.Notification.TabTypesChanged);
};

WebInspector.activateExtraDomains = function(domains)
{
    this.hasExtraDomains = true;

    for (var domain of domains) {
        var agent = InspectorBackend.activateDomain(domain);
        if (agent.enable)
            agent.enable();
    }

    this.notifications.dispatchEventToListeners(WebInspector.Notification.ExtraDomainsActivated, {"domains": domains});

    WebInspector.CSSCompletions.requestCSSCompletions();

    this._updateReloadToolbarButton();
    this._updateDownloadToolbarButton();
    this._tryToRestorePendingTabs();
};

WebInspector.contentBrowserTreeElementForRepresentedObject = function(contentBrowser, representedObject)
{
    // The console does not have a sidebar, so return a tree element here so something is shown.
    if (representedObject === this._consoleRepresentedObject)
        return this._consoleTreeElement;
    return null;
};

WebInspector.updateWindowTitle = function()
{
    var mainFrame = this.frameResourceManager.mainFrame;
    if (!mainFrame)
        return;

    var urlComponents = mainFrame.mainResource.urlComponents;

    var lastPathComponent;
    try {
        lastPathComponent = decodeURIComponent(urlComponents.lastPathComponent || "");
    } catch (e) {
        lastPathComponent = urlComponents.lastPathComponent;
    }

    // Build a title based on the URL components.
    if (urlComponents.host && lastPathComponent)
        var title = this.displayNameForHost(urlComponents.host) + " \u2014 " + lastPathComponent;
    else if (urlComponents.host)
        var title = this.displayNameForHost(urlComponents.host);
    else if (lastPathComponent)
        var title = lastPathComponent;
    else
        var title = mainFrame.url;

    // The name "inspectedURLChanged" sounds like the whole URL is required, however this is only
    // used for updating the window title and it can be any string.
    InspectorFrontendHost.inspectedURLChanged(title);
};

WebInspector.updateDockingAvailability = function(available)
{
    this._dockingAvailable = available;

    this._updateDockNavigationItems();
};

WebInspector.updateDockedState = function(side)
{
    if (this._dockConfiguration === side)
        return;

    this._previousDockConfiguration = this._dockConfiguration;

    if (!this._previousDockConfiguration) {
        if (side === WebInspector.DockConfiguration.Right || side === WebInspector.DockConfiguration.Left)
            this._previousDockConfiguration = WebInspector.DockConfiguration.Bottom;
        else
            this._previousDockConfiguration = WebInspector.resolvedLayoutDirection() === WebInspector.LayoutDirection.RTL ? WebInspector.DockConfiguration.Left : WebInspector.DockConfiguration.Right;
    }

    this._dockConfiguration = side;

    this.docked = side !== WebInspector.DockConfiguration.Undocked;

    this._ignoreToolbarModeDidChangeEvents = true;

    if (side === WebInspector.DockConfiguration.Bottom) {
        document.body.classList.add("docked", WebInspector.DockConfiguration.Bottom);
        document.body.classList.remove("window-inactive", WebInspector.DockConfiguration.Right, WebInspector.DockConfiguration.Left);
    } else if (side === WebInspector.DockConfiguration.Right) {
        document.body.classList.add("docked", WebInspector.DockConfiguration.Right);
        document.body.classList.remove("window-inactive", WebInspector.DockConfiguration.Bottom, WebInspector.DockConfiguration.Left);
    } else if (side === WebInspector.DockConfiguration.Left) {
        document.body.classList.add("docked", WebInspector.DockConfiguration.Left);
        document.body.classList.remove("window-inactive", WebInspector.DockConfiguration.Bottom, WebInspector.DockConfiguration.Right);
    } else
        document.body.classList.remove("docked", WebInspector.DockConfiguration.Right, WebInspector.DockConfiguration.Left, WebInspector.DockConfiguration.Bottom);

    this._ignoreToolbarModeDidChangeEvents = false;

    this._updateDockNavigationItems();

    if (!this.dockedConfigurationSupportsSplitContentBrowser() && !this.doesCurrentTabSupportSplitContentBrowser())
        this.hideSplitConsole();
};

WebInspector.updateVisibilityState = function(visible)
{
    this.visible = visible;
    this.notifications.dispatchEventToListeners(WebInspector.Notification.VisibilityStateDidChange);
};

WebInspector.handlePossibleLinkClick = function(event, frame, alwaysOpenExternally)
{
    var anchorElement = event.target.enclosingNodeOrSelfWithNodeName("a");
    if (!anchorElement || !anchorElement.href)
        return false;

    if (WebInspector.isBeingEdited(anchorElement)) {
        // Don't follow the link when it is being edited.
        return false;
    }

    // Prevent the link from navigating, since we don't do any navigation by following links normally.
    event.preventDefault();
    event.stopPropagation();

    this.openURL(anchorElement.href, frame, false, anchorElement.lineNumber);

    return true;
};

WebInspector.openURL = function(url, frame, alwaysOpenExternally, lineNumber)
{
    console.assert(url);
    if (!url)
        return;

    console.assert(typeof lineNumber === "undefined" || typeof lineNumber === "number", "lineNumber should be a number.");

    // If alwaysOpenExternally is not defined, base it off the command/meta key for the current event.
    if (alwaysOpenExternally === undefined || alwaysOpenExternally === null)
        alwaysOpenExternally = window.event ? window.event.metaKey : false;

    if (alwaysOpenExternally) {
        InspectorFrontendHost.openInNewTab(url);
        return;
    }

    var searchChildFrames = false;
    if (!frame) {
        frame = this.frameResourceManager.mainFrame;
        searchChildFrames = true;
    }

    console.assert(frame);

    // WebInspector.Frame.resourceForURL does not check the main resource, only sub-resources. So check both.
    let simplifiedURL = removeURLFragment(url);
    var resource = frame.url === simplifiedURL ? frame.mainResource : frame.resourceForURL(simplifiedURL, searchChildFrames);
    if (resource) {
        var position = new WebInspector.SourceCodePosition(lineNumber, 0);
        this.showSourceCode(resource, position);
        return;
    }

    InspectorFrontendHost.openInNewTab(url);
};

WebInspector.close = function()
{
    if (this._isClosing)
        return;

    this._isClosing = true;

    InspectorFrontendHost.closeWindow();
};

WebInspector.saveDataToFile = function(saveData, forceSaveAs)
{
    console.assert(saveData);
    if (!saveData)
        return;

    if (typeof saveData.customSaveHandler === "function") {
        saveData.customSaveHandler(forceSaveAs);
        return;
    }

    console.assert(saveData.url);
    console.assert(saveData.content);
    if (!saveData.url || !saveData.content)
        return;

    let suggestedName = parseURL(saveData.url).lastPathComponent;
    if (!suggestedName) {
        suggestedName = WebInspector.UIString("Untitled");
        let dataURLTypeMatch = /^data:([^;]+)/.exec(saveData.url);
        if (dataURLTypeMatch)
            suggestedName += WebInspector.fileExtensionForMIMEType(dataURLTypeMatch[1]) || "";
    }

    if (typeof saveData.content === "string") {
        const base64Encoded = false;
        InspectorFrontendHost.save(suggestedName, saveData.content, base64Encoded, forceSaveAs || saveData.forceSaveAs);
        return;
    }

    let fileReader = new FileReader;
    fileReader.readAsDataURL(saveData.content);
    fileReader.addEventListener("loadend", () => {
        let dataURLComponents = parseDataURL(fileReader.result);

        const base64Encoded = true;
        InspectorFrontendHost.save(suggestedName, dataURLComponents.data, base64Encoded, forceSaveAs || saveData.forceSaveAs);
    });
};

WebInspector.isConsoleFocused = function()
{
    return this.quickConsole.prompt.focused;
};

WebInspector.isShowingSplitConsole = function()
{
    return !this.splitContentBrowser.element.classList.contains("hidden");
};

WebInspector.dockedConfigurationSupportsSplitContentBrowser = function()
{
    return this._dockConfiguration !== WebInspector.DockConfiguration.Bottom;
};

WebInspector.doesCurrentTabSupportSplitContentBrowser = function()
{
    var currentContentView = this.tabBrowser.selectedTabContentView;
    return !currentContentView || currentContentView.supportsSplitContentBrowser;
};

WebInspector.toggleSplitConsole = function()
{
    if (!this.doesCurrentTabSupportSplitContentBrowser()) {
        this.showConsoleTab();
        return;
    }

    if (this.isShowingSplitConsole())
        this.hideSplitConsole();
    else
        this.showSplitConsole();
};

WebInspector.showSplitConsole = function()
{
    if (!this.doesCurrentTabSupportSplitContentBrowser()) {
        this.showConsoleTab();
        return;
    }

    this.splitContentBrowser.element.classList.remove("hidden");

    this._showingSplitConsoleSetting.value = true;

    if (this.splitContentBrowser.currentContentView !== this.consoleContentView) {
        // Be sure to close the view in the tab content browser before showing it in the
        // split content browser. We can only show a content view in one browser at a time.
        if (this.consoleContentView.parentContainer)
            this.consoleContentView.parentContainer.closeContentView(this.consoleContentView);
        this.splitContentBrowser.showContentView(this.consoleContentView);
    } else {
        // This causes the view to know it was shown and focus the prompt.
        this.splitContentBrowser.shown();
    }

    this.quickConsole.consoleLogVisibilityChanged(true);
};

WebInspector.hideSplitConsole = function()
{
    if (!this.isShowingSplitConsole())
        return;

    this.splitContentBrowser.element.classList.add("hidden");

    this._showingSplitConsoleSetting.value = false;

    // This causes the view to know it was hidden.
    this.splitContentBrowser.hidden();

    this.quickConsole.consoleLogVisibilityChanged(false);
};

WebInspector.showConsoleTab = function(requestedScope)
{
    requestedScope = requestedScope || WebInspector.LogContentView.Scopes.All;

    this.hideSplitConsole();

    this.consoleContentView.scopeBar.item(requestedScope).selected = true;

    this.showRepresentedObject(this._consoleRepresentedObject);

    console.assert(this.isShowingConsoleTab());
};

WebInspector.isShowingConsoleTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WebInspector.ConsoleTabContentView;
};

WebInspector.showElementsTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.ElementsTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.ElementsTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.showDebuggerTab = function(breakpointToSelect)
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.DebuggerTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.DebuggerTabContentView;

    if (breakpointToSelect instanceof WebInspector.Breakpoint)
        tabContentView.revealAndSelectBreakpoint(breakpointToSelect);

    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.isShowingDebuggerTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WebInspector.DebuggerTabContentView;
};

WebInspector.showResourcesTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.ResourcesTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.ResourcesTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.showStorageTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.StorageTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.StorageTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.showNetworkTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.NetworkTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.NetworkTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.showTimelineTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.TimelineTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.TimelineTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.unlocalizedString = function(string)
{
    // Intentionally do nothing, since this is for engineering builds
    // (such as in Debug UI) or in text that is standardized in English.
    // For example, CSS property names and values are never localized.
    return string;
};

WebInspector.UIString = function(string, vararg)
{
    if (WebInspector.dontLocalizeUserInterface)
        return string;

    if (window.localizedStrings && string in window.localizedStrings)
        return window.localizedStrings[string];

    if (!this._missingLocalizedStrings)
        this._missingLocalizedStrings = {};

    if (!(string in this._missingLocalizedStrings)) {
        console.error("Localized string \"" + string + "\" was not found.");
        this._missingLocalizedStrings[string] = true;
    }

    return "LOCALIZED STRING NOT FOUND";
};

WebInspector.indentString = function()
{
    if (WebInspector.settings.indentWithTabs.value)
        return "\t";
    return " ".repeat(WebInspector.settings.indentUnit.value);
};

WebInspector.restoreFocusFromElement = function(element)
{
    if (element && element.isSelfOrAncestor(this.currentFocusElement))
        this.previousFocusElement.focus();
};

WebInspector.toggleNavigationSidebar = function(event)
{
    if (!this.navigationSidebar.collapsed || !this.navigationSidebar.sidebarPanels.length) {
        this.navigationSidebar.collapsed = true;
        return;
    }

    if (!this.navigationSidebar.selectedSidebarPanel)
        this.navigationSidebar.selectedSidebarPanel = this.navigationSidebar.sidebarPanels[0];
    this.navigationSidebar.collapsed = false;
};

WebInspector.toggleDetailsSidebar = function(event)
{
    if (!this.detailsSidebar.collapsed || !this.detailsSidebar.sidebarPanels.length) {
        this.detailsSidebar.collapsed = true;
        return;
    }

    if (!this.detailsSidebar.selectedSidebarPanel)
        this.detailsSidebar.selectedSidebarPanel = this.detailsSidebar.sidebarPanels[0];
    this.detailsSidebar.collapsed = false;
};

WebInspector.tabContentViewClassForRepresentedObject = function(representedObject)
{
    if (representedObject instanceof WebInspector.DOMTree)
        return WebInspector.ElementsTabContentView;

    if (representedObject instanceof WebInspector.TimelineRecording)
        return WebInspector.TimelineTabContentView;

    // We only support one console tab right now. So this isn't an instanceof check.
    if (representedObject === this._consoleRepresentedObject)
        return WebInspector.ConsoleTabContentView;

    if (WebInspector.debuggerManager.paused) {
        if (representedObject instanceof WebInspector.Script)
            return WebInspector.DebuggerTabContentView;

        if (representedObject instanceof WebInspector.Resource && (representedObject.type === WebInspector.Resource.Type.Document || representedObject.type === WebInspector.Resource.Type.Script))
            return WebInspector.DebuggerTabContentView;
    }

    if (representedObject instanceof WebInspector.Frame || representedObject instanceof WebInspector.Resource || representedObject instanceof WebInspector.Script)
        return WebInspector.ResourcesTabContentView;

    // FIXME: Move Content Flows to the Elements tab?
    if (representedObject instanceof WebInspector.ContentFlow)
        return WebInspector.ResourcesTabContentView;

    // FIXME: Move these to a Storage tab.
    if (representedObject instanceof WebInspector.DOMStorageObject || representedObject instanceof WebInspector.CookieStorageObject ||
        representedObject instanceof WebInspector.DatabaseTableObject || representedObject instanceof WebInspector.DatabaseObject ||
        representedObject instanceof WebInspector.ApplicationCacheFrame || representedObject instanceof WebInspector.IndexedDatabaseObjectStore ||
        representedObject instanceof WebInspector.IndexedDatabaseObjectStoreIndex)
        return WebInspector.ResourcesTabContentView;

    if (representedObject instanceof WebInspector.Collection)
        return WebInspector.CollectionContentView;

    return null;
};

WebInspector.tabContentViewForRepresentedObject = function(representedObject, options = {})
{
    let tabContentView = this.tabBrowser.bestTabContentViewForRepresentedObject(representedObject, options);
    if (tabContentView)
        return tabContentView;

    var tabContentViewClass = this.tabContentViewClassForRepresentedObject(representedObject);
    if (!tabContentViewClass) {
        console.error("Unknown representedObject, couldn't create TabContentView.", representedObject);
        return null;
    }

    tabContentView = new tabContentViewClass;

    this.tabBrowser.addTabForContentView(tabContentView);

    return tabContentView;
};

WebInspector.showRepresentedObject = function(representedObject, cookie, options = {})
{
    let tabContentView = this.tabContentViewForRepresentedObject(representedObject, options);
    console.assert(tabContentView);
    if (!tabContentView)
        return;

    this.tabBrowser.showTabForContentView(tabContentView);
    tabContentView.showRepresentedObject(representedObject, cookie);
};

WebInspector.showMainFrameDOMTree = function(nodeToSelect, options = {})
{
    console.assert(WebInspector.frameResourceManager.mainFrame);
    if (!WebInspector.frameResourceManager.mainFrame)
        return;
    this.showRepresentedObject(WebInspector.frameResourceManager.mainFrame.domTree, {nodeToSelect}, options);
};

WebInspector.showSourceCodeForFrame = function(frameIdentifier, options = {})
{
    var frame = WebInspector.frameResourceManager.frameForIdentifier(frameIdentifier);
    if (!frame) {
        this._frameIdentifierToShowSourceCodeWhenAvailable = frameIdentifier;
        return;
    }

    this._frameIdentifierToShowSourceCodeWhenAvailable = undefined;

    this.showRepresentedObject(frame, null, options);
};

WebInspector.showSourceCode = function(sourceCode, options = {})
{
    const positionToReveal = options.positionToReveal;

    console.assert(!positionToReveal || positionToReveal instanceof WebInspector.SourceCodePosition, positionToReveal);
    var representedObject = sourceCode;

    if (representedObject instanceof WebInspector.Script) {
        // A script represented by a resource should always show the resource.
        representedObject = representedObject.resource || representedObject;
    }

    var cookie = positionToReveal ? {lineNumber: positionToReveal.lineNumber, columnNumber: positionToReveal.columnNumber} : {};
    this.showRepresentedObject(representedObject, cookie, options);
};

WebInspector.showSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    this.showSourceCode(sourceCodeLocation.displaySourceCode, Object.shallowMerge(options, {
        positionToReveal: sourceCodeLocation.displayPosition()
    }));
};

WebInspector.showOriginalUnformattedSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    this.showSourceCode(sourceCodeLocation.sourceCode, Object.shallowMerge(options, {
        positionToReveal: sourceCodeLocation.position(),
        forceUnformatted: true
    }));
};

WebInspector.showOriginalOrFormattedSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    this.showSourceCode(sourceCodeLocation.sourceCode, Object.shallowMerge(options, {
        positionToReveal: sourceCodeLocation.formattedPosition()
    }));
};

WebInspector.showOriginalOrFormattedSourceCodeTextRange = function(sourceCodeTextRange, options = {})
{
    var textRangeToSelect = sourceCodeTextRange.formattedTextRange;
    this.showSourceCode(sourceCodeTextRange.sourceCode, Object.shallowMerge(options, {
        positionToReveal: textRangeToSelect.startPosition(),
        textRangeToSelect
    }));
};

WebInspector.showResourceRequest = function(resource, options = {})
{
    this.showRepresentedObject(resource, {[WebInspector.ResourceClusterContentView.ContentViewIdentifierCookieKey]: WebInspector.ResourceClusterContentView.RequestIdentifier}, options);
};

WebInspector.debuggerToggleBreakpoints = function(event)
{
    WebInspector.debuggerManager.breakpointsEnabled = !WebInspector.debuggerManager.breakpointsEnabled;
};

WebInspector.debuggerPauseResumeToggle = function(event)
{
    if (WebInspector.debuggerManager.paused)
        WebInspector.debuggerManager.resume();
    else
        WebInspector.debuggerManager.pause();
};

WebInspector.debuggerStepOver = function(event)
{
    WebInspector.debuggerManager.stepOver();
};

WebInspector.debuggerStepInto = function(event)
{
    WebInspector.debuggerManager.stepInto();
};

WebInspector.debuggerStepOut = function(event)
{
    WebInspector.debuggerManager.stepOut();
};

WebInspector._searchTextDidChange = function(event)
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.SearchTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.SearchTabContentView;

    var searchQuery = this._searchToolbarItem.text;
    this._searchToolbarItem.text = "";

    this.tabBrowser.showTabForContentView(tabContentView);

    tabContentView.performSearch(searchQuery);
};

WebInspector._focusSearchField = function(event)
{
    if (this.tabBrowser.selectedTabContentView instanceof WebInspector.SearchTabContentView) {
        this.tabBrowser.selectedTabContentView.focusSearchField();
        return;
    }

    this._searchToolbarItem.focus();
};

WebInspector._focusChanged = function(event)
{
    // Make a caret selection inside the focused element if there isn't a range selection and there isn't already
    // a caret selection inside. This is needed (at least) to remove caret from console when focus is moved.
    // The selection change should not apply to text fields and text areas either.

    if (WebInspector.isEventTargetAnEditableField(event)) {
        // Still update the currentFocusElement if inside of a CodeMirror editor.
        var codeMirrorEditorElement = event.target.enclosingNodeOrSelfWithClass("CodeMirror");
        if (codeMirrorEditorElement && codeMirrorEditorElement !== this.currentFocusElement) {
            this.previousFocusElement = this.currentFocusElement;
            this.currentFocusElement = codeMirrorEditorElement;
        }
        return;
    }

    var selection = window.getSelection();
    if (!selection.isCollapsed)
        return;

    var element = event.target;

    if (element !== this.currentFocusElement) {
        this.previousFocusElement = this.currentFocusElement;
        this.currentFocusElement = element;
    }

    if (element.isInsertionCaretInside())
        return;

    var selectionRange = element.ownerDocument.createRange();
    selectionRange.setStart(element, 0);
    selectionRange.setEnd(element, 0);

    selection.removeAllRanges();
    selection.addRange(selectionRange);
};

WebInspector._mouseWasClicked = function(event)
{
    this.handlePossibleLinkClick(event);
};

WebInspector._dragOver = function(event)
{
    // Do nothing if another event listener handled the event already.
    if (event.defaultPrevented)
        return;

    // Allow dropping into editable areas.
    if (WebInspector.isEventTargetAnEditableField(event))
        return;

    // Prevent the drop from being accepted.
    event.dataTransfer.dropEffect = "none";
    event.preventDefault();
};

WebInspector._captureDidStart = function(event)
{
    this._dashboardContainer.showDashboardViewForRepresentedObject(this.dashboardManager.dashboards.replay);
};

WebInspector._debuggerDidPause = function(event)
{
    // FIXME: <webkit.org/b/###> Web Inspector: Preference for Auto Showing Scope Chain sidebar on pause
    this.showDebuggerTab();

    this._dashboardContainer.showDashboardViewForRepresentedObject(this.dashboardManager.dashboards.debugger);

    InspectorFrontendHost.bringToFront();
};

WebInspector._debuggerDidResume = function(event)
{
    this._dashboardContainer.closeDashboardViewForRepresentedObject(this.dashboardManager.dashboards.debugger);
};

WebInspector._frameWasAdded = function(event)
{
    if (!this._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    var frame = event.data.frame;
    if (frame.id !== this._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    function delayedWork()
    {
        this.showSourceCodeForFrame(frame.id);
    }

    // Delay showing the frame since FrameWasAdded is called before MainFrameChanged.
    // Calling showSourceCodeForFrame before MainFrameChanged will show the frame then close it.
    setTimeout(delayedWork.bind(this));
};

WebInspector._mainFrameDidChange = function(event)
{
    this._updateDownloadToolbarButton();

    this.updateWindowTitle();
};

WebInspector._mainResourceDidChange = function(event)
{
    if (!event.target.isMainFrame())
        return;

    this._inProvisionalLoad = false;

    // Run cookie restoration after we are sure all of the Tabs and NavigationSidebarPanels
    // have updated with respect to the main resource change.
    setTimeout(this._restoreCookieForOpenTabs.bind(this, WebInspector.StateRestorationType.Navigation));

    this._updateDownloadToolbarButton();

    this.updateWindowTitle();
};

WebInspector._provisionalLoadStarted = function(event)
{
    if (!event.target.isMainFrame())
        return;

    this._saveCookieForOpenTabs();

    this._inProvisionalLoad = true;
};

WebInspector._restoreCookieForOpenTabs = function(restorationType)
{
    for (var tabBarItem of this.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WebInspector.TabContentView))
            continue;
        tabContentView.restoreStateFromCookie(restorationType);
    }
};

WebInspector._saveCookieForOpenTabs = function()
{
    for (var tabBarItem of this.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WebInspector.TabContentView))
            continue;
        tabContentView.saveStateToCookie();
    }
};

WebInspector._windowFocused = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.remove(this.docked ? "window-docked-inactive" : "window-inactive");
};

WebInspector._windowBlurred = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.add(this.docked ? "window-docked-inactive" : "window-inactive");
};

WebInspector._windowResized = function(event)
{
    this.toolbar.updateLayout(WebInspector.View.LayoutReason.Resize);
    this.tabBar.updateLayout(WebInspector.View.LayoutReason.Resize);
    this._tabBrowserSizeDidChange();
};

WebInspector._updateModifierKeys = function(event)
{
    var didChange = this.modifierKeys.altKey !== event.altKey || this.modifierKeys.metaKey !== event.metaKey || this.modifierKeys.shiftKey !== event.shiftKey;

    this.modifierKeys = {altKey: event.altKey, metaKey: event.metaKey, shiftKey: event.shiftKey};

    if (didChange)
        this.notifications.dispatchEventToListeners(WebInspector.Notification.GlobalModifierKeysDidChange, event);
};

WebInspector._windowKeyDown = function(event)
{
    this._updateModifierKeys(event);
};

WebInspector._windowKeyUp = function(event)
{
    this._updateModifierKeys(event);
};

WebInspector._mouseDown = function(event)
{
    if (this.toolbar.element.isSelfOrAncestor(event.target))
        this._toolbarMouseDown(event);
};

WebInspector._mouseMoved = function(event)
{
    this._updateModifierKeys(event);
    this.mouseCoords = {
        x: event.pageX,
        y: event.pageY
    };
};

WebInspector._pageHidden = function(event)
{
    this._saveCookieForOpenTabs();
};

WebInspector._contextMenuRequested = function(event)
{
    let proposedContextMenu;

    // This is setting is only defined in engineering builds.
    if (WebInspector.isDebugUIEnabled()) {
        proposedContextMenu = WebInspector.ContextMenu.createFromEvent(event);
        proposedContextMenu.appendSeparator();
        proposedContextMenu.appendItem(WebInspector.unlocalizedString("Reload Web Inspector"), () => {
            window.location.reload();
        });

        let protocolSubMenu = proposedContextMenu.appendSubMenuItem(WebInspector.unlocalizedString("Protocol Debugging"), null, false);
        let isCapturingTraffic = InspectorBackend.activeTracer instanceof WebInspector.CapturingProtocolTracer;

        protocolSubMenu.appendCheckboxItem(WebInspector.unlocalizedString("Capture Trace"), () => {
            if (isCapturingTraffic)
                InspectorBackend.activeTracer = null;
            else
                InspectorBackend.activeTracer = new WebInspector.CapturingProtocolTracer;
        }, isCapturingTraffic);

        protocolSubMenu.appendSeparator();

        protocolSubMenu.appendItem(WebInspector.unlocalizedString("Export Trace\u2026"), () => {
            const forceSaveAs = true;
            WebInspector.saveDataToFile(InspectorBackend.activeTracer.trace.saveData, forceSaveAs);
        }, !isCapturingTraffic);
    } else {
        const onlyExisting = true;
        proposedContextMenu = WebInspector.ContextMenu.createFromEvent(event, onlyExisting);
    }

    if (proposedContextMenu)
        proposedContextMenu.show();
};

WebInspector.isDebugUIEnabled = function()
{
    return WebInspector.showDebugUISetting && WebInspector.showDebugUISetting.value;
};

WebInspector._undock = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WebInspector.DockConfiguration.Undocked);
};

WebInspector._dockBottom = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WebInspector.DockConfiguration.Bottom);
};

WebInspector._dockRight = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WebInspector.DockConfiguration.Right);
};

WebInspector._dockLeft = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WebInspector.DockConfiguration.Left);
};

WebInspector._togglePreviousDockConfiguration = function(event)
{
    InspectorFrontendHost.requestSetDockSide(this._previousDockConfiguration);
};

WebInspector._updateDockNavigationItems = function()
{
    if (this._dockingAvailable || this.docked) {
        this._closeToolbarButton.hidden = !this.docked;
        this._undockToolbarButton.hidden = this._dockConfiguration === WebInspector.DockConfiguration.Undocked;
        this._dockBottomToolbarButton.hidden = this._dockConfiguration === WebInspector.DockConfiguration.Bottom;
        this._dockToSideToolbarButton.hidden = this._dockConfiguration === WebInspector.DockConfiguration.Right || this._dockConfiguration === WebInspector.DockConfiguration.Left;
    } else {
        this._closeToolbarButton.hidden = true;
        this._undockToolbarButton.hidden = true;
        this._dockBottomToolbarButton.hidden = true;
        this._dockToSideToolbarButton.hidden = true;
    }
};

WebInspector._tabBrowserSizeDidChange = function()
{
    this.tabBrowser.updateLayout(WebInspector.View.LayoutReason.Resize);
    this.splitContentBrowser.updateLayout(WebInspector.View.LayoutReason.Resize);
    this.quickConsole.updateLayout(WebInspector.View.LayoutReason.Resize);
};

WebInspector._quickConsoleDidResize = function(event)
{
    this.tabBrowser.updateLayout(WebInspector.View.LayoutReason.Resize);
};

WebInspector._sidebarWidthDidChange = function(event)
{
    this._tabBrowserSizeDidChange();
};

WebInspector._setupViewHierarchy = function()
{
    let rootView = WebInspector.View.rootView();
    rootView.addSubview(this.toolbar);
    rootView.addSubview(this.tabBar);
    rootView.addSubview(this.navigationSidebar);
    rootView.addSubview(this.tabBrowser);
    rootView.addSubview(this.splitContentBrowser);
    rootView.addSubview(this.quickConsole);
    rootView.addSubview(this.detailsSidebar);
};

WebInspector._tabBrowserSelectedTabContentViewDidChange = function(event)
{
    if (this.tabBar.selectedTabBarItem && this.tabBar.selectedTabBarItem.representedObject.constructor.shouldSaveTab())
        this._selectedTabIndexSetting.value = this.tabBar.tabBarItems.indexOf(this.tabBar.selectedTabBarItem);

    if (!this.doesCurrentTabSupportSplitContentBrowser())
        this.hideSplitConsole();

    if (!this.isShowingSplitConsole())
        this.quickConsole.consoleLogVisibilityChanged(this.isShowingConsoleTab());
};

WebInspector._initializeWebSocketIfNeeded = function()
{
    if (!InspectorFrontendHost.initializeWebSocket)
        return;

    var queryParams = parseLocationQueryParameters();

    if ("ws" in queryParams)
        var url = "ws://" + queryParams.ws;
    else if ("page" in queryParams) {
        var page = queryParams.page;
        var host = "host" in queryParams ? queryParams.host : window.location.host;
        var url = "ws://" + host + "/devtools/page/" + page;
    }

    if (!url)
        return;

    InspectorFrontendHost.initializeWebSocket(url);
};

WebInspector._updateSplitConsoleHeight = function(height)
{
    const minimumHeight = 64;
    const maximumHeight = window.innerHeight * 0.55;

    height = Math.max(minimumHeight, Math.min(height, maximumHeight));

    this.splitContentBrowser.element.style.height = height + "px";
};

WebInspector._consoleResizerMouseDown = function(event)
{
    if (event.button !== 0 || event.ctrlKey)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (!event.target.classList.contains("navigation-bar") && !event.target.classList.contains("flexible-space"))
        return;

    var resizerElement = event.target;
    var mouseOffset = resizerElement.offsetHeight - (event.pageY - resizerElement.totalOffsetTop);

    function dockedResizerDrag(event)
    {
        if (event.button !== 0)
            return;

        var height = window.innerHeight - event.pageY - mouseOffset;

        this._splitConsoleHeightSetting.value = height;

        this._updateSplitConsoleHeight(height);

        this.quickConsole.dispatchEventToListeners(WebInspector.QuickConsole.Event.DidResize);
    }

    function dockedResizerDragEnd(event)
    {
        if (event.button !== 0)
            return;

        this.elementDragEnd(event);
    }

    this.elementDragStart(resizerElement, dockedResizerDrag.bind(this), dockedResizerDragEnd.bind(this), event, "row-resize");
};

WebInspector._toolbarMouseDown = function(event)
{
    if (event.ctrlKey)
        return;

    if (this._dockConfiguration === WebInspector.DockConfiguration.Right || this._dockConfiguration === WebInspector.DockConfiguration.Left)
        return;

    if (this.docked)
        this._dockedResizerMouseDown(event);
    else
        this._moveWindowMouseDown(event);
};

WebInspector._dockedResizerMouseDown = function(event)
{
    if (event.button !== 0 || event.ctrlKey)
        return;

    if (!this.docked)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (event.target.id !== "docked-resizer" && !event.target.classList.contains("toolbar") &&
        !event.target.classList.contains("flexible-space") && !event.target.classList.contains("item-section"))
        return;

    event[WebInspector.Popover.EventPreventDismissSymbol] = true;

    let windowProperty = this._dockConfiguration === WebInspector.DockConfiguration.Bottom ? "innerHeight" : "innerWidth";
    let eventScreenProperty = this._dockConfiguration === WebInspector.DockConfiguration.Bottom ? "screenY" : "screenX";
    let eventClientProperty = this._dockConfiguration === WebInspector.DockConfiguration.Bottom ? "clientY" : "clientX";

    var resizerElement = event.target;
    var firstClientPosition = event[eventClientProperty];
    var lastScreenPosition = event[eventScreenProperty];

    function dockedResizerDrag(event)
    {
        if (event.button !== 0)
            return;

        var position = event[eventScreenProperty];
        var delta = position - lastScreenPosition;
        var clientPosition = event[eventClientProperty];

        lastScreenPosition = position;

        if (this._dockConfiguration === WebInspector.DockConfiguration.Left) {
            // If the mouse is travelling rightward but is positioned left of the resizer, ignore the event.
            if (delta > 0 && clientPosition < firstClientPosition)
                return;

            // If the mouse is travelling leftward but is positioned to the right of the resizer, ignore the event.
            if (delta < 0 && clientPosition > window[windowProperty])
                return;

            // We later subtract the delta from the current position, but since the inspected view and inspector view
            // are flipped when docked to left, we want dragging to have the opposite effect from docked to right.
            delta *= -1;
        } else {
            // If the mouse is travelling downward/rightward but is positioned above/left of the resizer, ignore the event.
            if (delta > 0 && clientPosition < firstClientPosition)
                return;

            // If the mouse is travelling upward/leftward but is positioned below/right of the resizer, ignore the event.
            if (delta < 0 && clientPosition > firstClientPosition)
                return;
        }

        let dimension = Math.max(0, window[windowProperty] - delta);
        // If zoomed in/out, there be greater/fewer document pixels shown, but the inspector's
        // width or height should be the same in device pixels regardless of the document zoom.
        dimension *= this.getZoomFactor();

        if (this._dockConfiguration === WebInspector.DockConfiguration.Bottom)
            InspectorFrontendHost.setAttachedWindowHeight(dimension);
        else
            InspectorFrontendHost.setAttachedWindowWidth(dimension);
    }

    function dockedResizerDragEnd(event)
    {
        if (event.button !== 0)
            return;

        WebInspector.elementDragEnd(event);
    }

    WebInspector.elementDragStart(resizerElement, dockedResizerDrag.bind(this), dockedResizerDragEnd.bind(this), event, this._dockConfiguration === WebInspector.DockConfiguration.Bottom ? "row-resize" : "col-resize");
};

WebInspector._moveWindowMouseDown = function(event)
{
    console.assert(!this.docked);

    if (event.button !== 0 || event.ctrlKey)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (!event.target.classList.contains("toolbar") && !event.target.classList.contains("flexible-space") &&
        !event.target.classList.contains("item-section"))
        return;

    event[WebInspector.Popover.EventPreventDismissSymbol] = true;

    if (WebInspector.Platform.name === "mac") {
        // New Mac releases can start a window drag.
        if (WebInspector.Platform.version.release >= 11) {
            InspectorFrontendHost.startWindowDrag();
            event.preventDefault();
            return;
        }

        // Ignore dragging on the top of the toolbar on Mac if the system handles it.
        if (WebInspector.Platform.version.release === 10) {
            const windowDragHandledTitleBarHeight = 22;
            if (event.pageY < windowDragHandledTitleBarHeight) {
                event.preventDefault();
                return;
            }
        }
    }

    var lastScreenX = event.screenX;
    var lastScreenY = event.screenY;

    function toolbarDrag(event)
    {
        if (event.button !== 0)
            return;

        var x = event.screenX - lastScreenX;
        var y = event.screenY - lastScreenY;

        InspectorFrontendHost.moveWindowBy(x, y);

        lastScreenX = event.screenX;
        lastScreenY = event.screenY;
    }

    function toolbarDragEnd(event)
    {
        if (event.button !== 0)
            return;

        WebInspector.elementDragEnd(event);
    }

    WebInspector.elementDragStart(event.target, toolbarDrag, toolbarDragEnd, event, "default");
};

WebInspector._storageWasInspected = function(event)
{
    this.showStorageTab();
};

WebInspector._domNodeWasInspected = function(event)
{
    this.domTreeManager.highlightDOMNodeForTwoSeconds(event.data.node.id);

    InspectorFrontendHost.bringToFront();

    this.showElementsTab();
    this.showMainFrameDOMTree(event.data.node);
};

WebInspector._inspectModeStateChanged = function(event)
{
    this._inspectModeToolbarButton.activated = this.domTreeManager.inspectModeEnabled;
};

WebInspector._toggleInspectMode = function(event)
{
    this.domTreeManager.inspectModeEnabled = !this.domTreeManager.inspectModeEnabled;
};

WebInspector._downloadWebArchive = function(event)
{
    this.archiveMainFrame();
};

WebInspector._reloadPage = function(event)
{
    if (!window.PageAgent)
        return;

    PageAgent.reload();
    event.preventDefault();
};

WebInspector._reloadPageClicked = function(event)
{
    // Ignore cache when the shift key is pressed.
    PageAgent.reload(window.event ? window.event.shiftKey : false);
};

WebInspector._reloadPageIgnoringCache = function(event)
{
    if (!window.PageAgent)
        return;

    PageAgent.reload(true);
    event.preventDefault();
};

WebInspector._updateReloadToolbarButton = function()
{
    if (!window.PageAgent) {
        this._reloadToolbarButton.hidden = true;
        return;
    }

    this._reloadToolbarButton.hidden = false;
};

WebInspector._updateDownloadToolbarButton = function()
{
    // COMPATIBILITY (iOS 7): Page.archive did not exist yet.
    if (!window.PageAgent || !PageAgent.archive || this.debuggableType !== WebInspector.DebuggableType.Web) {
        this._downloadToolbarButton.hidden = true;
        return;
    }

    if (this._downloadingPage) {
        this._downloadToolbarButton.enabled = false;
        return;
    }

    this._downloadToolbarButton.enabled = this.canArchiveMainFrame();
};

WebInspector._toggleInspectMode = function(event)
{
    this.domTreeManager.inspectModeEnabled = !this.domTreeManager.inspectModeEnabled;
};

WebInspector._showConsoleTab = function(event)
{
    this.showConsoleTab();
};

WebInspector._focusConsolePrompt = function(event)
{
    this.quickConsole.prompt.focus();
};

WebInspector._focusedContentBrowser = function()
{
    if (this.tabBrowser.element.isSelfOrAncestor(this.currentFocusElement) || document.activeElement === document.body) {
        var tabContentView = this.tabBrowser.selectedTabContentView;
        if (tabContentView instanceof WebInspector.ContentBrowserTabContentView)
            return tabContentView.contentBrowser;
        return null;
    }

    if (this.splitContentBrowser.element.isSelfOrAncestor(this.currentFocusElement)
        || (WebInspector.isShowingSplitConsole() && this.quickConsole.element.isSelfOrAncestor(this.currentFocusElement)))
        return this.splitContentBrowser;

    return null;
};

WebInspector._focusedContentView = function()
{
    if (this.tabBrowser.element.isSelfOrAncestor(this.currentFocusElement) || document.activeElement === document.body) {
        var tabContentView = this.tabBrowser.selectedTabContentView;
        if (tabContentView instanceof WebInspector.ContentBrowserTabContentView)
            return tabContentView.contentBrowser.currentContentView;
        return tabContentView;
    }

    if (this.splitContentBrowser.element.isSelfOrAncestor(this.currentFocusElement)
        || (WebInspector.isShowingSplitConsole() && this.quickConsole.element.isSelfOrAncestor(this.currentFocusElement)))
        return this.splitContentBrowser.currentContentView;

    return null;
};

WebInspector._focusedOrVisibleContentBrowser = function()
{
    let focusedContentBrowser = this._focusedContentBrowser();
    if (focusedContentBrowser)
        return focusedContentBrowser;

    var tabContentView = this.tabBrowser.selectedTabContentView;
    if (tabContentView instanceof WebInspector.ContentBrowserTabContentView)
        return tabContentView.contentBrowser;

    return null;
};

WebInspector.focusedOrVisibleContentView = function()
{
    let focusedContentView = this._focusedContentView();
    if (focusedContentView)
        return focusedContentView;

    var tabContentView = this.tabBrowser.selectedTabContentView;
    if (tabContentView instanceof WebInspector.ContentBrowserTabContentView)
        return tabContentView.contentBrowser.currentContentView;
    return tabContentView;
};

WebInspector._beforecopy = function(event)
{
    var selection = window.getSelection();

    // If there is no selection, see if the focused element or focused ContentView can handle the copy event.
    if (selection.isCollapsed && !WebInspector.isEventTargetAnEditableField(event)) {
        var focusedCopyHandler = this.currentFocusElement && this.currentFocusElement.copyHandler;
        if (focusedCopyHandler && typeof focusedCopyHandler.handleBeforeCopyEvent === "function") {
            focusedCopyHandler.handleBeforeCopyEvent(event);
            if (event.defaultPrevented)
                return;
        }

        var focusedContentView = this._focusedContentView();
        if (focusedContentView && typeof focusedContentView.handleCopyEvent === "function") {
            event.preventDefault();
            return;
        }

        return;
    }

    if (selection.isCollapsed)
        return;

    // Say we can handle it (by preventing default) to remove word break characters.
    event.preventDefault();
};

WebInspector._find = function(event)
{
    let contentBrowser = this._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.showFindBanner();
};

WebInspector._save = function(event)
{
    var contentView = this.focusedOrVisibleContentView();
    if (!contentView || !contentView.supportsSave)
        return;

    WebInspector.saveDataToFile(contentView.saveData);
};

WebInspector._saveAs = function(event)
{
    var contentView = this.focusedOrVisibleContentView();
    if (!contentView || !contentView.supportsSave)
        return;

    WebInspector.saveDataToFile(contentView.saveData, true);
};

WebInspector._clear = function(event)
{
    let contentView = this.focusedOrVisibleContentView();
    if (!contentView || typeof contentView.handleClearShortcut !== "function") {
        // If the current content view is unable to handle this event, clear the console to reset
        // the dashboard counters.
        this.logManager.requestClearMessages();
        return;
    }

    contentView.handleClearShortcut(event);
};

WebInspector._copy = function(event)
{
    var selection = window.getSelection();

    // If there is no selection, pass the copy event on to the focused element or focused ContentView.
    if (selection.isCollapsed && !WebInspector.isEventTargetAnEditableField(event)) {
        var focusedCopyHandler = this.currentFocusElement && this.currentFocusElement.copyHandler;
        if (focusedCopyHandler && typeof focusedCopyHandler.handleCopyEvent === "function") {
            focusedCopyHandler.handleCopyEvent(event);
            if (event.defaultPrevented)
                return;
        }

        var focusedContentView = this._focusedContentView();
        if (focusedContentView && typeof focusedContentView.handleCopyEvent === "function") {
            focusedContentView.handleCopyEvent(event);
            return;
        }

        return;
    }

    if (selection.isCollapsed)
        return;

    // Remove word break characters from the selection before putting it on the pasteboard.
    var selectionString = selection.toString().removeWordBreakCharacters();
    event.clipboardData.setData("text/plain", selectionString);
    event.preventDefault();
};

WebInspector._increaseZoom = function(event)
{
    const epsilon = 0.0001;
    const maximumZoom = 2.4;
    let currentZoom = this.getZoomFactor();
    if (currentZoom + epsilon >= maximumZoom) {
        InspectorFrontendHost.beep();
        return;
    }

    this.setZoomFactor(Math.min(maximumZoom, currentZoom + 0.2));
};

WebInspector._decreaseZoom = function(event)
{
    const epsilon = 0.0001;
    const minimumZoom = 0.6;
    let currentZoom = this.getZoomFactor();
    if (currentZoom - epsilon <= minimumZoom) {
        InspectorFrontendHost.beep();
        return;
    }

    this.setZoomFactor(Math.max(minimumZoom, currentZoom - 0.2));
};

WebInspector._resetZoom = function(event)
{
    this.setZoomFactor(1);
};

WebInspector.getZoomFactor = function()
{
    return WebInspector.settings.zoomFactor.value;
};

WebInspector.setZoomFactor = function(factor)
{
    InspectorFrontendHost.setZoomFactor(factor);
    // Round-trip through the frontend host API in case the requested factor is not used.
    WebInspector.settings.zoomFactor.value = InspectorFrontendHost.zoomFactor();
};

WebInspector.resolvedLayoutDirection = function()
{
    let layoutDirection = WebInspector.settings.layoutDirection.value;
    if (layoutDirection === WebInspector.LayoutDirection.System)
        layoutDirection = InspectorFrontendHost.userInterfaceLayoutDirection();

    return layoutDirection;
}

WebInspector.setLayoutDirection = function(value)
{
    if (!Object.values(WebInspector.LayoutDirection).includes(value))
        WebInspector.reportInternalError("Unknown layout direction requested: " + value);

    if (value === WebInspector.settings.layoutDirection.value)
        return;

    WebInspector.settings.layoutDirection.value = value;

    if (value === WebInspector.LayoutDirection.RTL && this._dockConfiguration === WebInspector.DockConfiguration.Right)
        this._dockLeft();

    if (value === WebInspector.LayoutDirection.LTR && this._dockConfiguration === WebInspector.DockConfiguration.Left)
        this._dockRight();

    window.location.reload();
};

WebInspector._showTabAtIndex = function(i, event)
{
    if (i <= WebInspector.tabBar.tabBarItems.length)
        WebInspector.tabBar.selectedTabBarItem = i - 1;
};

WebInspector._showJavaScriptTypeInformationSettingChanged = function(event)
{
    if (this.showJavaScriptTypeInformationSetting.value) {
        for (let target of WebInspector.targets)
            target.RuntimeAgent.enableTypeProfiler();
    } else {
        for (let target of WebInspector.targets)
            target.RuntimeAgent.disableTypeProfiler();
    }
};

WebInspector._enableControlFlowProfilerSettingChanged = function(event)
{
    if (this.enableControlFlowProfilerSetting.value) {
        for (let target of WebInspector.targets)
            target.RuntimeAgent.enableControlFlowProfiler();
    } else {
        for (let target of WebInspector.targets)
            target.RuntimeAgent.disableControlFlowProfiler();
    }
};

WebInspector.elementDragStart = function(element, dividerDrag, elementDragEnd, event, cursor, eventTarget)
{
    if (WebInspector._elementDraggingEventListener || WebInspector._elementEndDraggingEventListener)
        WebInspector.elementDragEnd(event);

    if (element) {
        // Install glass pane
        if (WebInspector._elementDraggingGlassPane)
            WebInspector._elementDraggingGlassPane.remove();

        var glassPane = document.createElement("div");
        glassPane.style.cssText = "position:absolute;top:0;bottom:0;left:0;right:0;opacity:0;z-index:1";
        glassPane.id = "glass-pane-for-drag";
        element.ownerDocument.body.appendChild(glassPane);
        WebInspector._elementDraggingGlassPane = glassPane;
    }

    WebInspector._elementDraggingEventListener = dividerDrag;
    WebInspector._elementEndDraggingEventListener = elementDragEnd;

    var targetDocument = event.target.ownerDocument;

    WebInspector._elementDraggingEventTarget = eventTarget || targetDocument;
    WebInspector._elementDraggingEventTarget.addEventListener("mousemove", dividerDrag, true);
    WebInspector._elementDraggingEventTarget.addEventListener("mouseup", elementDragEnd, true);

    targetDocument.body.style.cursor = cursor;

    event.preventDefault();
};

WebInspector.elementDragEnd = function(event)
{
    WebInspector._elementDraggingEventTarget.removeEventListener("mousemove", WebInspector._elementDraggingEventListener, true);
    WebInspector._elementDraggingEventTarget.removeEventListener("mouseup", WebInspector._elementEndDraggingEventListener, true);

    event.target.ownerDocument.body.style.removeProperty("cursor");

    if (WebInspector._elementDraggingGlassPane)
        WebInspector._elementDraggingGlassPane.remove();

    delete WebInspector._elementDraggingGlassPane;
    delete WebInspector._elementDraggingEventTarget;
    delete WebInspector._elementDraggingEventListener;
    delete WebInspector._elementEndDraggingEventListener;

    event.preventDefault();
};

WebInspector.createMessageTextView = function(message, isError)
{
    var messageElement = document.createElement("div");
    messageElement.className = "message-text-view";
    if (isError)
        messageElement.classList.add("error");

    messageElement.textContent = message;

    return messageElement;
};

WebInspector.createGoToArrowButton = function()
{
    var button = document.createElement("button");
    button.addEventListener("mousedown", (event) => { event.stopPropagation(); }, true);
    button.className = "go-to-arrow";
    button.tabIndex = -1;
    return button;
};

WebInspector.createSourceCodeLocationLink = function(sourceCodeLocation, dontFloat, useGoToArrowButton)
{
    console.assert(sourceCodeLocation);
    if (!sourceCodeLocation)
        return null;

    var linkElement = document.createElement("a");
    linkElement.className = "go-to-link";
    WebInspector.linkifyElement(linkElement, sourceCodeLocation);
    sourceCodeLocation.populateLiveDisplayLocationTooltip(linkElement);

    if (useGoToArrowButton)
        linkElement.appendChild(WebInspector.createGoToArrowButton());
    else
        sourceCodeLocation.populateLiveDisplayLocationString(linkElement, "textContent");

    if (dontFloat)
        linkElement.classList.add("dont-float");

    return linkElement;
};

WebInspector.linkifyLocation = function(url, lineNumber, columnNumber, className)
{
    var sourceCode = WebInspector.sourceCodeForURL(url);

    if (!sourceCode) {
        var anchor = document.createElement("a");
        anchor.href = url;
        anchor.lineNumber = lineNumber;
        if (className)
            anchor.className = className;
        anchor.append(WebInspector.displayNameForURL(url) + ":" + lineNumber);
        return anchor;
    }

    var sourceCodeLocation = sourceCode.createSourceCodeLocation(lineNumber, columnNumber);
    var linkElement = WebInspector.createSourceCodeLocationLink(sourceCodeLocation, true);
    if (className)
        linkElement.classList.add(className);
    return linkElement;
};

WebInspector.linkifyElement = function(linkElement, sourceCodeLocation) {
    console.assert(sourceCodeLocation);

    function showSourceCodeLocation(event)
    {
        event.stopPropagation();
        event.preventDefault();

        if (event.metaKey)
            this.showOriginalUnformattedSourceCodeLocation(sourceCodeLocation);
        else
            this.showSourceCodeLocation(sourceCodeLocation);
    }

    linkElement.addEventListener("click", showSourceCodeLocation.bind(this));
};

WebInspector.sourceCodeForURL = function(url)
{
    var sourceCode = WebInspector.frameResourceManager.resourceForURL(url);
    if (!sourceCode) {
        sourceCode = WebInspector.debuggerManager.scriptsForURL(url, WebInspector.assumingMainTarget())[0];
        if (sourceCode)
            sourceCode = sourceCode.resource || sourceCode;
    }
    return sourceCode || null;
};

WebInspector.linkifyURLAsNode = function(url, linkText, classes)
{
    if (!linkText)
        linkText = url;

    classes = (classes ? classes + " " : "");

    var a = document.createElement("a");
    a.href = url;
    a.className = classes;

    a.textContent = linkText;
    a.style.maxWidth = "100%";

    return a;
};

WebInspector.linkifyStringAsFragmentWithCustomLinkifier = function(string, linkifier)
{
    var container = document.createDocumentFragment();
    var linkStringRegEx = /(?:[a-zA-Z][a-zA-Z0-9+.-]{2,}:\/\/|www\.)[\w$\-_+*'=\|\/\\(){}[\]%@&#~,:;.!?]{2,}[\w$\-_+*=\|\/\\({%@&#~]/;
    var lineColumnRegEx = /:(\d+)(:(\d+))?$/;

    while (string) {
        var linkString = linkStringRegEx.exec(string);
        if (!linkString)
            break;

        linkString = linkString[0];
        var linkIndex = string.indexOf(linkString);
        var nonLink = string.substring(0, linkIndex);
        container.append(nonLink);

        if (linkString.startsWith("data:") || linkString.startsWith("javascript:") || linkString.startsWith("mailto:")) {
            container.append(linkString);
            string = string.substring(linkIndex + linkString.length, string.length);
            continue;
        }

        var title = linkString;
        var realURL = linkString.startsWith("www.") ? "http://" + linkString : linkString;
        var lineColumnMatch = lineColumnRegEx.exec(realURL);
        if (lineColumnMatch)
            realURL = realURL.substring(0, realURL.length - lineColumnMatch[0].length);

        var lineNumber;
        if (lineColumnMatch)
            lineNumber = parseInt(lineColumnMatch[1]) - 1;

        var linkNode = linkifier(title, realURL, lineNumber);
        container.appendChild(linkNode);
        string = string.substring(linkIndex + linkString.length, string.length);
    }

    if (string)
        container.append(string);

    return container;
};

WebInspector.linkifyStringAsFragment = function(string)
{
    function linkifier(title, url, lineNumber)
    {
        var urlNode = WebInspector.linkifyURLAsNode(url, title, undefined);
        if (lineNumber !== undefined)
            urlNode.lineNumber = lineNumber;

        return urlNode;
    }

    return WebInspector.linkifyStringAsFragmentWithCustomLinkifier(string, linkifier);
};

WebInspector.createResourceLink = function(resource, className)
{
    function handleClick(event)
    {
        event.stopPropagation();
        event.preventDefault();

        WebInspector.showRepresentedObject(resource);
    }

    let linkNode = document.createElement("a");
    linkNode.classList.add("resource-link", className);
    linkNode.title = resource.url;
    linkNode.textContent = (resource.urlComponents.lastPathComponent || resource.url).insertWordBreakCharacters();
    linkNode.addEventListener("click", handleClick.bind(this));
    return linkNode;
};

WebInspector._undoKeyboardShortcut = function(event)
{
    if (!this.isEditingAnyField() && !this.isEventTargetAnEditableField(event)) {
        this.undo();
        event.preventDefault();
    }
};

WebInspector._redoKeyboardShortcut = function(event)
{
    if (!this.isEditingAnyField() && !this.isEventTargetAnEditableField(event)) {
        this.redo();
        event.preventDefault();
    }
};

WebInspector.undo = function()
{
    DOMAgent.undo();
};

WebInspector.redo = function()
{
    DOMAgent.redo();
};

WebInspector.highlightRangesWithStyleClass = function(element, resultRanges, styleClass, changes)
{
    changes = changes || [];
    var highlightNodes = [];
    var lineText = element.textContent;
    var ownerDocument = element.ownerDocument;
    var textNodeSnapshot = ownerDocument.evaluate(".//text()", element, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

    var snapshotLength = textNodeSnapshot.snapshotLength;
    if (snapshotLength === 0)
        return highlightNodes;

    var nodeRanges = [];
    var rangeEndOffset = 0;
    for (var i = 0; i < snapshotLength; ++i) {
        var range = {};
        range.offset = rangeEndOffset;
        range.length = textNodeSnapshot.snapshotItem(i).textContent.length;
        rangeEndOffset = range.offset + range.length;
        nodeRanges.push(range);
    }

    var startIndex = 0;
    for (var i = 0; i < resultRanges.length; ++i) {
        var startOffset = resultRanges[i].offset;
        var endOffset = startOffset + resultRanges[i].length;

        while (startIndex < snapshotLength && nodeRanges[startIndex].offset + nodeRanges[startIndex].length <= startOffset)
            startIndex++;
        var endIndex = startIndex;
        while (endIndex < snapshotLength && nodeRanges[endIndex].offset + nodeRanges[endIndex].length < endOffset)
            endIndex++;
        if (endIndex === snapshotLength)
            break;

        var highlightNode = ownerDocument.createElement("span");
        highlightNode.className = styleClass;
        highlightNode.textContent = lineText.substring(startOffset, endOffset);

        var lastTextNode = textNodeSnapshot.snapshotItem(endIndex);
        var lastText = lastTextNode.textContent;
        lastTextNode.textContent = lastText.substring(endOffset - nodeRanges[endIndex].offset);
        changes.push({node: lastTextNode, type: "changed", oldText: lastText, newText: lastTextNode.textContent});

        if (startIndex === endIndex) {
            lastTextNode.parentElement.insertBefore(highlightNode, lastTextNode);
            changes.push({node: highlightNode, type: "added", nextSibling: lastTextNode, parent: lastTextNode.parentElement});
            highlightNodes.push(highlightNode);

            var prefixNode = ownerDocument.createTextNode(lastText.substring(0, startOffset - nodeRanges[startIndex].offset));
            lastTextNode.parentElement.insertBefore(prefixNode, highlightNode);
            changes.push({node: prefixNode, type: "added", nextSibling: highlightNode, parent: lastTextNode.parentElement});
        } else {
            var firstTextNode = textNodeSnapshot.snapshotItem(startIndex);
            var firstText = firstTextNode.textContent;
            var anchorElement = firstTextNode.nextSibling;

            firstTextNode.parentElement.insertBefore(highlightNode, anchorElement);
            changes.push({node: highlightNode, type: "added", nextSibling: anchorElement, parent: firstTextNode.parentElement});
            highlightNodes.push(highlightNode);

            firstTextNode.textContent = firstText.substring(0, startOffset - nodeRanges[startIndex].offset);
            changes.push({node: firstTextNode, type: "changed", oldText: firstText, newText: firstTextNode.textContent});

            for (var j = startIndex + 1; j < endIndex; j++) {
                var textNode = textNodeSnapshot.snapshotItem(j);
                var text = textNode.textContent;
                textNode.textContent = "";
                changes.push({node: textNode, type: "changed", oldText: text, newText: textNode.textContent});
            }
        }
        startIndex = endIndex;
        nodeRanges[startIndex].offset = endOffset;
        nodeRanges[startIndex].length = lastTextNode.textContent.length;

    }
    return highlightNodes;
};

WebInspector.revertDomChanges = function(domChanges)
{
    for (var i = domChanges.length - 1; i >= 0; --i) {
        var entry = domChanges[i];
        switch (entry.type) {
        case "added":
            entry.node.remove();
            break;
        case "changed":
            entry.node.textContent = entry.oldText;
            break;
        }
    }
};

WebInspector.archiveMainFrame = function()
{
    this._downloadingPage = true;
    this._updateDownloadToolbarButton();

    PageAgent.archive((error, data) => {
        this._downloadingPage = false;
        this._updateDownloadToolbarButton();

        if (error)
            return;

        let mainFrame = WebInspector.frameResourceManager.mainFrame;
        let archiveName = mainFrame.mainResource.urlComponents.host || mainFrame.mainResource.displayName || "Archive";
        let url = "web-inspector:///" + encodeURI(archiveName) + ".webarchive";

        InspectorFrontendHost.save(url, data, true, true);
    });
};

WebInspector.canArchiveMainFrame = function()
{
    // COMPATIBILITY (iOS 7): Page.archive did not exist yet.
    if (!PageAgent.archive || this.debuggableType !== WebInspector.DebuggableType.Web)
        return false;

    if (!WebInspector.frameResourceManager.mainFrame || !WebInspector.frameResourceManager.mainFrame.mainResource)
        return false;

    return WebInspector.Resource.typeFromMIMEType(WebInspector.frameResourceManager.mainFrame.mainResource.mimeType) === WebInspector.Resource.Type.Document;
};

WebInspector.addWindowKeydownListener = function(listener)
{
    if (typeof listener.handleKeydownEvent !== "function")
        return;

    this._windowKeydownListeners.push(listener);

    this._updateWindowKeydownListener();
};

WebInspector.removeWindowKeydownListener = function(listener)
{
    this._windowKeydownListeners.remove(listener);

    this._updateWindowKeydownListener();
};

WebInspector._updateWindowKeydownListener = function()
{
    if (this._windowKeydownListeners.length === 1)
        window.addEventListener("keydown", WebInspector._sharedWindowKeydownListener, true);
    else if (!this._windowKeydownListeners.length)
        window.removeEventListener("keydown", WebInspector._sharedWindowKeydownListener, true);
};

WebInspector._sharedWindowKeydownListener = function(event)
{
    for (var i = WebInspector._windowKeydownListeners.length - 1; i >= 0; --i) {
        if (WebInspector._windowKeydownListeners[i].handleKeydownEvent(event)) {
            event.stopImmediatePropagation();
            event.preventDefault();
            break;
        }
    }
};

WebInspector.reportInternalError = function(errorOrString, details={})
{
    // The 'details' object includes additional information from the caller as free-form string keys and values.
    // Each key and value will be shown in the uncaught exception reporter, console error message, or in
    // a pre-filled bug report generated for this internal error.

    let error = (errorOrString instanceof Error) ? errorOrString : new Error(errorOrString);
    error.details = details;

    // The error will be displayed in the Uncaught Exception Reporter sheet if DebugUI is enabled.
    if (WebInspector.isDebugUIEnabled()) {
        // This assert allows us to stop the debugger at an internal exception. It doesn't re-throw
        // exceptions because the original exception would be lost through window.onerror.
        // This workaround can be removed once <https://webkit.org/b/158192> is fixed.
        console.assert(false, "An internal exception was thrown.", error);
        handleInternalException(error);
    } else
        console.error(error);
};

Object.defineProperty(WebInspector, "targets",
{
    get() { return this.targetManager.targets; }
});

// Many places assume the main target because they cannot yet be
// used by reached by Worker debugging. Eventually, once all
// Worker domains have been implemented, all of these must be
// handled properly.
WebInspector.assumingMainTarget = function()
{
    return WebInspector.mainTarget;
};

// OpenResourceDialog delegate

WebInspector.dialogWasDismissed = function(dialog)
{
    let representedObject = dialog.representedObject;
    if (!representedObject)
        return;

    WebInspector.showRepresentedObject(representedObject, dialog.cookie);
};

WebInspector.DockConfiguration = {
    Right: "right",
    Left: "left",
    Bottom: "bottom",
    Undocked: "undocked",
};
