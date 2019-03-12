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

WI.ContentViewCookieType = {
    ApplicationCache: "application-cache",
    CookieStorage: "cookie-storage",
    Database: "database",
    DatabaseTable: "database-table",
    DOMStorage: "dom-storage",
    Resource: "resource", // includes Frame too.
    Timelines: "timelines"
};

WI.SelectedSidebarPanelCookieKey = "selected-sidebar-panel";
WI.TypeIdentifierCookieKey = "represented-object-type";

WI.StateRestorationType = {
    Load: "state-restoration-load",
    Navigation: "state-restoration-navigation",
    Delayed: "state-restoration-delayed",
};

WI.LayoutDirection = {
    System: "system",
    LTR: "ltr",
    RTL: "rtl",
};

WI.loaded = function()
{
    // Register observers for events from the InspectorBackend.
    if (InspectorBackend.registerTargetDispatcher)
        InspectorBackend.registerTargetDispatcher(new WI.TargetObserver);
    if (InspectorBackend.registerInspectorDispatcher)
        InspectorBackend.registerInspectorDispatcher(new WI.InspectorObserver);
    if (InspectorBackend.registerPageDispatcher)
        InspectorBackend.registerPageDispatcher(new WI.PageObserver);
    if (InspectorBackend.registerConsoleDispatcher)
        InspectorBackend.registerConsoleDispatcher(new WI.ConsoleObserver);
    if (InspectorBackend.registerNetworkDispatcher)
        InspectorBackend.registerNetworkDispatcher(new WI.NetworkObserver);
    if (InspectorBackend.registerDOMDispatcher)
        InspectorBackend.registerDOMDispatcher(new WI.DOMObserver);
    if (InspectorBackend.registerDebuggerDispatcher)
        InspectorBackend.registerDebuggerDispatcher(new WI.DebuggerObserver);
    if (InspectorBackend.registerHeapDispatcher)
        InspectorBackend.registerHeapDispatcher(new WI.HeapObserver);
    if (InspectorBackend.registerMemoryDispatcher)
        InspectorBackend.registerMemoryDispatcher(new WI.MemoryObserver);
    if (InspectorBackend.registerDatabaseDispatcher)
        InspectorBackend.registerDatabaseDispatcher(new WI.DatabaseObserver);
    if (InspectorBackend.registerDOMStorageDispatcher)
        InspectorBackend.registerDOMStorageDispatcher(new WI.DOMStorageObserver);
    if (InspectorBackend.registerApplicationCacheDispatcher)
        InspectorBackend.registerApplicationCacheDispatcher(new WI.ApplicationCacheObserver);
    if (InspectorBackend.registerCPUProfilerDispatcher)
        InspectorBackend.registerCPUProfilerDispatcher(new WI.CPUProfilerObserver);
    if (InspectorBackend.registerScriptProfilerDispatcher)
        InspectorBackend.registerScriptProfilerDispatcher(new WI.ScriptProfilerObserver);
    if (InspectorBackend.registerTimelineDispatcher)
        InspectorBackend.registerTimelineDispatcher(new WI.TimelineObserver);
    if (InspectorBackend.registerCSSDispatcher)
        InspectorBackend.registerCSSDispatcher(new WI.CSSObserver);
    if (InspectorBackend.registerLayerTreeDispatcher)
        InspectorBackend.registerLayerTreeDispatcher(new WI.LayerTreeObserver);
    if (InspectorBackend.registerRuntimeDispatcher)
        InspectorBackend.registerRuntimeDispatcher(new WI.RuntimeObserver);
    if (InspectorBackend.registerWorkerDispatcher)
        InspectorBackend.registerWorkerDispatcher(new WI.WorkerObserver);
    if (InspectorBackend.registerCanvasDispatcher)
        InspectorBackend.registerCanvasDispatcher(new WI.CanvasObserver);

    // Listen for the ProvisionalLoadStarted event before registering for events so our code gets called before any managers or sidebars.
    // This lets us save a state cookie before any managers or sidebars do any resets that would affect state (namely TimelineManager).
    WI.Frame.addEventListener(WI.Frame.Event.ProvisionalLoadStarted, this._provisionalLoadStarted, this);

    // Populate any UIStrings that must be done early after localized strings have loaded.
    WI.KeyboardShortcut.Key.Space._displayName = WI.UIString("Space");

    // Create the singleton managers next, before the user interface elements, so the user interface can register
    // as event listeners on these managers.
    WI.managers = [
        WI.targetManager = new WI.TargetManager,
        WI.branchManager = new WI.BranchManager,
        WI.networkManager = new WI.NetworkManager,
        WI.domStorageManager = new WI.DOMStorageManager,
        WI.databaseManager = new WI.DatabaseManager,
        WI.indexedDBManager = new WI.IndexedDBManager,
        WI.domManager = new WI.DOMManager,
        WI.cssManager = new WI.CSSManager,
        WI.consoleManager = new WI.ConsoleManager,
        WI.runtimeManager = new WI.RuntimeManager,
        WI.heapManager = new WI.HeapManager,
        WI.memoryManager = new WI.MemoryManager,
        WI.applicationCacheManager = new WI.ApplicationCacheManager,
        WI.timelineManager = new WI.TimelineManager,
        WI.auditManager = new WI.AuditManager,
        WI.debuggerManager = new WI.DebuggerManager,
        WI.layerTreeManager = new WI.LayerTreeManager,
        WI.workerManager = new WI.WorkerManager,
        WI.domDebuggerManager = new WI.DOMDebuggerManager,
        WI.canvasManager = new WI.CanvasManager,
    ];

    // Register for events.
    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
    WI.domManager.addEventListener(WI.DOMManager.Event.InspectModeStateChanged, this._inspectModeStateChanged, this);
    WI.domManager.addEventListener(WI.DOMManager.Event.DOMNodeWasInspected, this._domNodeWasInspected, this);
    WI.domStorageManager.addEventListener(WI.DOMStorageManager.Event.DOMStorageObjectWasInspected, this._domStorageWasInspected, this);
    WI.databaseManager.addEventListener(WI.DatabaseManager.Event.DatabaseWasInspected, this._databaseWasInspected, this);
    WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, this._mainFrameDidChange, this);
    WI.networkManager.addEventListener(WI.NetworkManager.Event.FrameWasAdded, this._frameWasAdded, this);

    WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

    document.addEventListener("DOMContentLoaded", this.contentLoaded.bind(this));

    // Create settings.
    this._showingSplitConsoleSetting = new WI.Setting("showing-split-console", false);
    this._openTabsSetting = new WI.Setting("open-tab-types", [
        WI.ElementsTabContentView.Type,
        WI.NetworkTabContentView.Type,
        WI.DebuggerTabContentView.Type,
        WI.ResourcesTabContentView.Type,
        WI.TimelineTabContentView.Type,
        WI.StorageTabContentView.Type,
        WI.CanvasTabContentView.Type,
        WI.AuditTabContentView.Type,
        WI.ConsoleTabContentView.Type,
    ]);
    this._selectedTabIndexSetting = new WI.Setting("selected-tab-index", 0);

    // State.
    this.printStylesEnabled = false;
    this.setZoomFactor(WI.settings.zoomFactor.value);
    this.mouseCoords = {x: 0, y: 0};
    this.modifierKeys = {altKey: false, metaKey: false, shiftKey: false};
    this.visible = false;
    this._windowKeydownListeners = [];
    this._targetsAvailablePromise = new WI.WrappedPromise;
    WI._overridenDeviceUserAgent = null;
    WI._overridenDeviceSettings = new Map;

    // Targets.
    WI.backendTarget = null;
    WI.pageTarget = null;

    if (!window.TargetAgent)
        WI.targetManager.createDirectBackendTarget();
    else {
        // FIXME: Eliminate `TargetAgent.exists` once the local inspector
        // is configured to use the Multiplexing code path.
        TargetAgent.exists((error) => {
            if (error)
                WI.targetManager.createDirectBackendTarget();
        });
    }
};

WI.initializeBackendTarget = function(target)
{
    console.assert(!WI.mainTarget);

    WI.backendTarget = target;

    WI.resetMainExecutionContext();

    this._targetsAvailablePromise.resolve();
};

WI.initializePageTarget = function(target)
{
    console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.Web);
    console.assert(target.type === WI.Target.Type.Page || target instanceof WI.DirectBackendTarget);

    WI.pageTarget = target;

    WI.redirectGlobalAgentsToConnection(WI.pageTarget.connection);

    WI.resetMainExecutionContext();
};

WI.transitionPageTarget = function(target)
{
    console.assert(!WI.pageTarget);
    console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.Web);
    console.assert(target.type === WI.Target.Type.Page);

    WI.pageTarget = target;

    WI.redirectGlobalAgentsToConnection(WI.pageTarget.connection);

    WI.resetMainExecutionContext();

    // Actions to transition the page target.
    this.notifications.dispatchEventToListeners(WI.Notification.TransitionPageTarget);
    WI.domManager.transitionPageTarget();
    WI.networkManager.transitionPageTarget();
    WI.timelineManager.transitionPageTarget();
};

WI.terminatePageTarget = function(target)
{
    console.assert(WI.pageTarget);
    console.assert(WI.pageTarget === target);
    console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.Web);

    // Remove any Worker targets associated with this page.
    let workerTargets = WI.targets.filter((x) => x.type === WI.Target.Type.Worker);
    for (let workerTarget of workerTargets)
        WI.workerManager.workerTerminated(workerTarget.identifier);

    WI.pageTarget = null;

    WI.redirectGlobalAgentsToConnection(WI.backendConnection);
};

WI.resetMainExecutionContext = function()
{
    if (WI.mainTarget instanceof WI.MultiplexingBackendTarget)
        return;

    if (WI.mainTarget.executionContext) {
        WI.runtimeManager.activeExecutionContext = WI.mainTarget.executionContext;
        if (WI.quickConsole)
            WI.quickConsole.initializeMainExecutionContextPathComponent();
    }
};

WI.redirectGlobalAgentsToConnection = function(connection)
{
    // This makes global window.FooAgent dispatch to the active page target.
    for (let [domain, agent] of Object.entries(InspectorBackend._agents)) {
        if (domain !== "Target")
            agent.connection = connection;
    }
};

WI.contentLoaded = function()
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
    document.addEventListener("focus", WI._focusChanged.bind(this), true);

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
    document.body.classList.add(WI.Platform.name + "-platform");
    if (WI.Platform.isNightlyBuild)
        document.body.classList.add("nightly-build");

    if (WI.Platform.name === "mac") {
        document.body.classList.add(WI.Platform.version.name);

        if (WI.Platform.version.release >= 11)
            document.body.classList.add("latest-mac");
        else
            document.body.classList.add("legacy-mac");
    }

    document.body.classList.add(WI.sharedApp.debuggableType);
    document.body.setAttribute("dir", this.resolvedLayoutDirection());

    WI.settings.showJavaScriptTypeInformation.addEventListener(WI.Setting.Event.Changed, this._showJavaScriptTypeInformationSettingChanged, this);
    WI.settings.enableControlFlowProfiler.addEventListener(WI.Setting.Event.Changed, this._enableControlFlowProfilerSettingChanged, this);
    WI.settings.resourceCachingDisabled.addEventListener(WI.Setting.Event.Changed, this._resourceCachingDisabledSettingChanged, this);

    function setTabSize() {
        document.body.style.tabSize = WI.settings.tabSize.value;
    }
    WI.settings.tabSize.addEventListener(WI.Setting.Event.Changed, setTabSize);
    setTabSize();

    function setInvalidCharacterClassName() {
        document.body.classList.toggle("show-invalid-characters", WI.settings.showInvalidCharacters.value);
    }
    WI.settings.showInvalidCharacters.addEventListener(WI.Setting.Event.Changed, setInvalidCharacterClassName);
    setInvalidCharacterClassName();

    function setWhitespaceCharacterClassName() {
        document.body.classList.toggle("show-whitespace-characters", WI.settings.showWhitespaceCharacters.value);
    }
    WI.settings.showWhitespaceCharacters.addEventListener(WI.Setting.Event.Changed, setWhitespaceCharacterClassName);
    setWhitespaceCharacterClassName();

    this.settingsTabContentView = new WI.SettingsTabContentView;

    this._settingsKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Comma, this._showSettingsTab.bind(this));

    // Create the user interface elements.
    this.toolbar = new WI.Toolbar(document.getElementById("toolbar"));

    if (WI.settings.experimentalEnableNewTabBar.value)
        this.tabBar = new WI.TabBar(document.getElementById("tab-bar"));
    else {
        this.tabBar = new WI.LegacyTabBar(document.getElementById("tab-bar"));
        this.tabBar.addEventListener(WI.TabBar.Event.OpenDefaultTab, this._openDefaultTab, this);
    }

    this._contentElement = document.getElementById("content");
    this._contentElement.setAttribute("role", "main");
    this._contentElement.setAttribute("aria-label", WI.UIString("Content"));

    this.clearKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "K", this._clear.bind(this));

    // FIXME: <https://webkit.org/b/151310> Web Inspector: Command-E should propagate to other search fields (including the system)
    this.populateFindKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "E", this._populateFind.bind(this));
    this.populateFindKeyboardShortcut.implicitlyPreventsDefault = false;
    this.findNextKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "G", this._findNext.bind(this));
    this.findPreviousKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "G", this._findPrevious.bind(this));

    this.consoleDrawer = new WI.ConsoleDrawer(document.getElementById("console-drawer"));
    this.consoleDrawer.addEventListener(WI.ConsoleDrawer.Event.CollapsedStateChanged, this._consoleDrawerCollapsedStateDidChange, this);
    this.consoleDrawer.addEventListener(WI.ConsoleDrawer.Event.Resized, this._consoleDrawerDidResize, this);

    this.quickConsole = new WI.QuickConsole(document.getElementById("quick-console"));

    this._consoleRepresentedObject = new WI.LogObject;
    this.consoleContentView = this.consoleDrawer.contentViewForRepresentedObject(this._consoleRepresentedObject);
    this.consoleLogViewController = this.consoleContentView.logViewController;
    this.breakpointPopoverController = new WI.BreakpointPopoverController;

    // FIXME: The sidebars should be flipped in RTL languages.
    this.navigationSidebar = new WI.Sidebar(document.getElementById("navigation-sidebar"), WI.Sidebar.Sides.Left);
    this.navigationSidebar.addEventListener(WI.Sidebar.Event.WidthDidChange, this._sidebarWidthDidChange, this);

    this.detailsSidebar = new WI.Sidebar(document.getElementById("details-sidebar"), WI.Sidebar.Sides.Right, null, null, WI.UIString("Details"), true);
    this.detailsSidebar.addEventListener(WI.Sidebar.Event.WidthDidChange, this._sidebarWidthDidChange, this);

    this.searchKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "F", this._focusSearchField.bind(this));
    this._findKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "F", this._find.bind(this));
    this.saveKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "S", this._save.bind(this));
    this._saveAsKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "S", this._saveAs.bind(this));

    this.openResourceKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "O", this._showOpenResourceDialog.bind(this));
    new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "P", this._showOpenResourceDialog.bind(this));

    this.navigationSidebarKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "0", this.toggleNavigationSidebar.bind(this));
    this.detailsSidebarKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, "0", this.toggleDetailsSidebar.bind(this));

    let boundIncreaseZoom = this._increaseZoom.bind(this);
    let boundDecreaseZoom = this._decreaseZoom.bind(this);
    this._increaseZoomKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Plus, boundIncreaseZoom);
    this._decreaseZoomKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Minus, boundDecreaseZoom);
    this._increaseZoomKeyboardShortcut2 = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Plus, boundIncreaseZoom);
    this._decreaseZoomKeyboardShortcut2 = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Minus, boundDecreaseZoom);
    this._resetZoomKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "0", this._resetZoom.bind(this));

    this._showTabAtIndexKeyboardShortcuts = [1, 2, 3, 4, 5, 6, 7, 8, 9].map((i) => new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, `${i}`, this._showTabAtIndex.bind(this, i)));
    this._openNewTabKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, "T", this.showNewTabTab.bind(this));

    this.tabBrowser = new WI.TabBrowser(document.getElementById("tab-browser"), this.tabBar, this.navigationSidebar, this.detailsSidebar);
    this.tabBrowser.addEventListener(WI.TabBrowser.Event.SelectedTabContentViewDidChange, this._tabBrowserSelectedTabContentViewDidChange, this);

    this._reloadPageKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "R", this._reloadPage.bind(this));
    this._reloadPageFromOriginKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, "R", this._reloadPageFromOrigin.bind(this));
    this._reloadPageKeyboardShortcut.implicitlyPreventsDefault = this._reloadPageFromOriginKeyboardShortcut.implicitlyPreventsDefault = false;

    this._consoleTabKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Option | WI.KeyboardShortcut.Modifier.CommandOrControl, "C", this._showConsoleTab.bind(this));
    this._quickConsoleKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Control, WI.KeyboardShortcut.Key.Apostrophe, this._focusConsolePrompt.bind(this));

    this._inspectModeKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "C", this._toggleInspectMode.bind(this));

    this._undoKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "Z", this._undoKeyboardShortcut.bind(this));
    this._redoKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "Z", this._redoKeyboardShortcut.bind(this));
    this._undoKeyboardShortcut.implicitlyPreventsDefault = this._redoKeyboardShortcut.implicitlyPreventsDefault = false;

    this.toggleBreakpointsKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "Y", this.debuggerToggleBreakpoints.bind(this));
    this.pauseOrResumeKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Control | WI.KeyboardShortcut.Modifier.CommandOrControl, "Y", this.debuggerPauseResumeToggle.bind(this));
    this.stepOverKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.F6, this.debuggerStepOver.bind(this));
    this.stepIntoKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.F7, this.debuggerStepInto.bind(this));
    this.stepOutKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.F8, this.debuggerStepOut.bind(this));

    this.pauseOrResumeAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Backslash, this.debuggerPauseResumeToggle.bind(this));
    this.stepOverAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.SingleQuote, this.debuggerStepOver.bind(this));
    this.stepIntoAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Semicolon, this.debuggerStepInto.bind(this));
    this.stepOutAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Semicolon, this.debuggerStepOut.bind(this));

    this._closeToolbarButton = new WI.ControlToolbarItem("dock-close", WI.UIString("Close"), "Images/Close.svg", 16, 14);
    this._closeToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this.close, this);

    this._undockToolbarButton = new WI.ButtonToolbarItem("undock", WI.UIString("Detach into separate window"), "Images/Undock.svg");
    this._undockToolbarButton.element.classList.add(WI.Popover.IgnoreAutoDismissClassName);
    this._undockToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._undock, this);

    let dockImage = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? "Images/DockLeft.svg" : "Images/DockRight.svg";
    this._dockToSideToolbarButton = new WI.ButtonToolbarItem("dock-right", WI.UIString("Dock to side of window"), dockImage);
    this._dockToSideToolbarButton.element.classList.add(WI.Popover.IgnoreAutoDismissClassName);

    let dockToSideCallback = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? this._dockLeft : this._dockRight;
    this._dockToSideToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, dockToSideCallback, this);

    this._dockBottomToolbarButton = new WI.ButtonToolbarItem("dock-bottom", WI.UIString("Dock to bottom of window"), "Images/DockBottom.svg");
    this._dockBottomToolbarButton.element.classList.add(WI.Popover.IgnoreAutoDismissClassName);
    this._dockBottomToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._dockBottom, this);

    this._togglePreviousDockConfigurationKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "D", this._togglePreviousDockConfiguration.bind(this));

    let reloadToolTip;
    if (WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript)
        reloadToolTip = WI.UIString("Restart (%s)").format(this._reloadPageKeyboardShortcut.displayName);
    else
        reloadToolTip = WI.UIString("Reload page (%s)\nReload page ignoring cache (%s)").format(this._reloadPageKeyboardShortcut.displayName, this._reloadPageFromOriginKeyboardShortcut.displayName);
    this._reloadToolbarButton = new WI.ButtonToolbarItem("reload", reloadToolTip, "Images/ReloadToolbar.svg");
    this._reloadToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._reloadToolbarButtonClicked, this);

    this._downloadToolbarButton = new WI.ButtonToolbarItem("download", WI.UIString("Download Web Archive"), "Images/DownloadArrow.svg");
    this._downloadToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._downloadWebArchive, this);

    let elementSelectionToolTip = WI.UIString("Start element selection (%s)").format(WI._inspectModeKeyboardShortcut.displayName);
    let activatedElementSelectionToolTip = WI.UIString("Stop element selection (%s)").format(WI._inspectModeKeyboardShortcut.displayName);
    this._inspectModeToolbarButton = new WI.ActivateButtonToolbarItem("inspect", elementSelectionToolTip, activatedElementSelectionToolTip, "Images/Crosshair.svg");
    this._inspectModeToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._toggleInspectMode, this);

    // COMPATIBILITY (iOS 12.2): Page.overrideSetting did not exist.
    if (InspectorFrontendHost.isRemote && WI.sharedApp.debuggableType === WI.DebuggableType.Web && InspectorBackend.domains.Page && InspectorBackend.domains.Page.overrideUserAgent && InspectorBackend.domains.Page.overrideSetting) {
        const deviceSettingsTooltip = WI.UIString("Device Settings");
        WI._deviceSettingsToolbarButton = new WI.ActivateButtonToolbarItem("device-settings", deviceSettingsTooltip, deviceSettingsTooltip, "Images/Device.svg");
        WI._deviceSettingsToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleDeviceSettingsToolbarButtonClicked, this);

        WI._deviceSettingsPopover = null;
    }

    this._updateReloadToolbarButton();
    this._updateDownloadToolbarButton();
    this._updateInspectModeToolbarButton();

    this._dashboards = {
        default: new WI.DefaultDashboard,
        debugger: new WI.DebuggerDashboard,
    };

    this._dashboardContainer = new WI.DashboardContainerView;
    this._dashboardContainer.showDashboardViewForRepresentedObject(this._dashboards.default);

    this.toolbar.addToolbarItem(this._closeToolbarButton, WI.Toolbar.Section.Control);

    this.toolbar.addToolbarItem(this._undockToolbarButton, WI.Toolbar.Section.Left);
    this.toolbar.addToolbarItem(this._dockToSideToolbarButton, WI.Toolbar.Section.Left);
    this.toolbar.addToolbarItem(this._dockBottomToolbarButton, WI.Toolbar.Section.Left);

    this.toolbar.addToolbarItem(this._reloadToolbarButton, WI.Toolbar.Section.CenterLeft);
    this.toolbar.addToolbarItem(this._downloadToolbarButton, WI.Toolbar.Section.CenterLeft);

    this.toolbar.addToolbarItem(this._dashboardContainer.toolbarItem, WI.Toolbar.Section.Center);

    this.toolbar.addToolbarItem(this._inspectModeToolbarButton, WI.Toolbar.Section.CenterRight);

    if (WI._deviceSettingsToolbarButton)
        this.toolbar.addToolbarItem(WI._deviceSettingsToolbarButton, WI.Toolbar.Section.CenterRight);

    this._searchTabContentView = new WI.SearchTabContentView;

    if (WI.settings.experimentalEnableNewTabBar.value) {
        this.tabBrowser.addTabForContentView(this._searchTabContentView, {suppressAnimations: true});
        this.tabBar.addTabBarItem(this.settingsTabContentView.tabBarItem, {suppressAnimations: true});
    } else {
        const incremental = false;
        this._searchToolbarItem = new WI.SearchBar("inspector-search", WI.UIString("Search"), incremental);
        this._searchToolbarItem.addEventListener(WI.SearchBar.Event.TextChanged, this._searchTextDidChange, this);
        this.toolbar.addToolbarItem(this._searchToolbarItem, WI.Toolbar.Section.Right);
    }

    let dockedResizerElement = document.getElementById("docked-resizer");
    dockedResizerElement.classList.add(WI.Popover.IgnoreAutoDismissClassName);
    dockedResizerElement.addEventListener("mousedown", this._dockedResizerMouseDown.bind(this));

    this._dockingAvailable = false;

    this._updateDockNavigationItems();
    this._setupViewHierarchy();

    // These tabs are always available for selecting, modulo isTabAllowed().
    // Other tabs may be engineering-only or toggled at runtime if incomplete.
    let productionTabClasses = [
        WI.ElementsTabContentView,
        WI.NetworkTabContentView,
        WI.SourcesTabContentView,
        WI.DebuggerTabContentView,
        WI.ResourcesTabContentView,
        WI.TimelineTabContentView,
        WI.StorageTabContentView,
        WI.CanvasTabContentView,
        WI.LayersTabContentView,
        WI.AuditTabContentView,
        WI.ConsoleTabContentView,
        WI.SearchTabContentView,
        WI.NewTabContentView,
        WI.SettingsTabContentView,
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

        if (!this.isNewTabWithTypeAllowed(tabType))
            continue;

        let tabContentView = this._createTabContentViewForType(tabType);
        if (!tabContentView)
            continue;
        this.tabBrowser.addTabForContentView(tabContentView, {suppressAnimations: true});
    }

    this._restoreCookieForOpenTabs(WI.StateRestorationType.Load);

    this.tabBar.selectedTabBarItem = this._selectedTabIndexSetting.value;

    if (!this.tabBar.selectedTabBarItem)
        this.tabBar.selectedTabBarItem = 0;

    if (!this.tabBar.normalTabCount)
        this.showNewTabTab({suppressAnimations: true});

    // Listen to the events after restoring the saved tabs to avoid recursion.
    this.tabBar.addEventListener(WI.TabBar.Event.TabBarItemAdded, this._rememberOpenTabs, this);
    this.tabBar.addEventListener(WI.TabBar.Event.TabBarItemRemoved, this._rememberOpenTabs, this);
    this.tabBar.addEventListener(WI.TabBar.Event.TabBarItemsReordered, this._rememberOpenTabs, this);

    // Signal that the frontend is now ready to receive messages.
    WI.whenTargetsAvailable().then(() => {
        InspectorFrontendAPI.loadCompleted();
    });

    // Tell the InspectorFrontendHost we loaded, which causes the window to display
    // and pending InspectorFrontendAPI commands to be sent.
    InspectorFrontendHost.loaded();

    if (this._showingSplitConsoleSetting.value)
        this.showSplitConsole();

    // Store this on the window in case the WebInspector global gets corrupted.
    window.__frontendCompletedLoad = true;

    if (WI.runBootstrapOperations)
        WI.runBootstrapOperations();
};

WI.performOneTimeFrontendInitializationsUsingTarget = function(target)
{
    if (!WI.__didPerformConsoleInitialization && target.ConsoleAgent) {
        WI.__didPerformConsoleInitialization = true;
        WI.consoleManager.initializeLogChannels(target);
    }

    if (!WI.__didPerformCSSInitialization && target.CSSAgent) {
        WI.__didPerformCSSInitialization = true;
        WI.CSSCompletions.initializeCSSCompletions(target);
    }
};

WI.initializeTarget = function(target)
{
    if (target.PageAgent) {
        // COMPATIBILITY (iOS 12.2): Page.overrideUserAgent did not exist.
        if (target.PageAgent.overrideUserAgent && WI._overridenDeviceUserAgent)
            target.PageAgent.overrideUserAgent(WI._overridenDeviceUserAgent);

        // COMPATIBILITY (iOS 12.2): Page.overrideSetting did not exist.
        if (target.PageAgent.overrideSetting) {
            for (let [setting, value] of WI._overridenDeviceSettings)
                target.PageAgent.overrideSetting(setting, value);
        }

        // COMPATIBILITY (iOS 11.3)
        if (target.PageAgent.setShowRulers && WI.settings.showRulers.value)
            target.PageAgent.setShowRulers(true);

        // COMPATIBILITY (iOS 8): Page.setShowPaintRects did not exist.
        if (target.PageAgent.setShowPaintRects && WI.settings.showPaintRects.value)
            target.PageAgent.setShowPaintRects(true);
    }
};

WI.targetsAvailable = function()
{
    return this._targetsAvailablePromise.settled;
};

WI.whenTargetsAvailable = function()
{
    return this._targetsAvailablePromise.promise;
};

WI.isTabTypeAllowed = function(tabType)
{
    let tabClass = this._knownTabClassesByType.get(tabType);
    if (!tabClass)
        return false;

    return tabClass.isTabAllowed();
};

WI.knownTabClasses = function()
{
    return new Set(this._knownTabClassesByType.values());
};

WI._showOpenResourceDialog = function()
{
    if (!this._openResourceDialog)
        this._openResourceDialog = new WI.OpenResourceDialog(this);

    if (this._openResourceDialog.visible)
        return;

    this._openResourceDialog.present(this._contentElement);
};

WI._createTabContentViewForType = function(tabType)
{
    let tabClass = this._knownTabClassesByType.get(tabType);
    if (!tabClass) {
        console.error("Unknown tab type", tabType);
        return null;
    }

    console.assert(WI.TabContentView.isPrototypeOf(tabClass));
    return new tabClass;
};

WI._rememberOpenTabs = function()
{
    let seenTabTypes = new Set;
    let openTabs = [];

    for (let tabBarItem of this.tabBar.tabBarItems) {
        let tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
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

WI._openDefaultTab = function(event)
{
    this.showNewTabTab({suppressAnimations: true});
};

WI._showSettingsTab = function(event)
{
    if (event.keyIdentifier === "U+002C") // ","
        this.tabBrowser.showTabForContentView(this.settingsTabContentView);
};

WI._tryToRestorePendingTabs = function()
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

        this.tabBrowser.addTabForContentView(tabContentView, {
            suppressAnimations: true,
            insertionIndex: index,
        });

        tabContentView.restoreStateFromCookie(WI.StateRestorationType.Load);
    }

    this._pendingOpenTabs = stillPendingOpenTabs;

    if (!WI.settings.experimentalEnableNewTabBar.value)
        this.tabBar.updateNewTabTabBarItemState();
};

WI.showNewTabTab = function(options)
{
    if (!this.isNewTabWithTypeAllowed(WI.NewTabContentView.Type))
        return;

    let tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.NewTabContentView);
    if (!tabContentView)
        tabContentView = new WI.NewTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView, options);
};

WI.isNewTabWithTypeAllowed = function(tabType)
{
    let tabClass = this._knownTabClassesByType.get(tabType);
    if (!tabClass || !tabClass.isTabAllowed())
        return false;

    // Only allow one tab per class for now.
    for (let tabBarItem of this.tabBar.tabBarItems) {
        let tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        if (tabContentView.constructor === tabClass)
            return false;
    }

    if (tabClass === WI.NewTabContentView) {
        let allTabs = Array.from(this.knownTabClasses());
        let addableTabs = allTabs.filter((tabClass) => !tabClass.tabInfo().isEphemeral);
        let canMakeNewTab = addableTabs.some((tabClass) => WI.isNewTabWithTypeAllowed(tabClass.Type));
        return canMakeNewTab;
    }

    return true;
};

WI.createNewTabWithType = function(tabType, options = {})
{
    console.assert(this.isNewTabWithTypeAllowed(tabType));

    let {referencedView, shouldReplaceTab, shouldShowNewTab} = options;
    console.assert(!referencedView || referencedView instanceof WI.TabContentView, referencedView);
    console.assert(!shouldReplaceTab || referencedView, "Must provide a reference view to replace a tab.");

    let tabContentView = this._createTabContentViewForType(tabType);
    const suppressAnimations = true;
    this.tabBrowser.addTabForContentView(tabContentView, {
        suppressAnimations,
        insertionIndex: referencedView ? this.tabBar.tabBarItems.indexOf(referencedView.tabBarItem) : undefined,
    });

    if (shouldReplaceTab)
        this.tabBrowser.closeTabForContentView(referencedView, {suppressAnimations});

    if (shouldShowNewTab)
        this.tabBrowser.showTabForContentView(tabContentView);
};

WI.activateExtraDomains = function(domains)
{
    this.notifications.dispatchEventToListeners(WI.Notification.ExtraDomainsActivated, {domains});

    if (WI.mainTarget) {
        if (!WI.pageTarget && WI.mainTarget.DOMAgent)
            WI.pageTarget = WI.mainTarget;

        if (WI.mainTarget.CSSAgent)
            WI.CSSCompletions.initializeCSSCompletions(WI.assumingMainTarget());

        if (WI.mainTarget.DOMAgent)
            WI.domManager.ensureDocument();

        if (WI.mainTarget.PageAgent)
            WI.networkManager.initializeTarget(WI.mainTarget);
    }

    this._updateReloadToolbarButton();
    this._updateDownloadToolbarButton();
    this._updateInspectModeToolbarButton();

    this._tryToRestorePendingTabs();
};

WI.updateWindowTitle = function()
{
    var mainFrame = this.networkManager.mainFrame;
    if (!mainFrame)
        return;

    var urlComponents = mainFrame.mainResource.urlComponents;

    var lastPathComponent;
    try {
        lastPathComponent = decodeURIComponent(urlComponents.lastPathComponent || "");
    } catch {
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

WI.updateDockingAvailability = function(available)
{
    this._dockingAvailable = available;

    this._updateDockNavigationItems();
};

WI.updateDockedState = function(side)
{
    if (this._dockConfiguration === side)
        return;

    this._previousDockConfiguration = this._dockConfiguration;

    if (!this._previousDockConfiguration) {
        if (side === WI.DockConfiguration.Right || side === WI.DockConfiguration.Left)
            this._previousDockConfiguration = WI.DockConfiguration.Bottom;
        else
            this._previousDockConfiguration = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? WI.DockConfiguration.Left : WI.DockConfiguration.Right;
    }

    this._dockConfiguration = side;

    this.docked = side !== WI.DockConfiguration.Undocked;

    this._ignoreToolbarModeDidChangeEvents = true;

    if (side === WI.DockConfiguration.Bottom) {
        document.body.classList.add("docked", WI.DockConfiguration.Bottom);
        document.body.classList.remove("window-inactive", WI.DockConfiguration.Right, WI.DockConfiguration.Left);
    } else if (side === WI.DockConfiguration.Right) {
        document.body.classList.add("docked", WI.DockConfiguration.Right);
        document.body.classList.remove("window-inactive", WI.DockConfiguration.Bottom, WI.DockConfiguration.Left);
    } else if (side === WI.DockConfiguration.Left) {
        document.body.classList.add("docked", WI.DockConfiguration.Left);
        document.body.classList.remove("window-inactive", WI.DockConfiguration.Bottom, WI.DockConfiguration.Right);
    } else
        document.body.classList.remove("docked", WI.DockConfiguration.Right, WI.DockConfiguration.Left, WI.DockConfiguration.Bottom);

    this._ignoreToolbarModeDidChangeEvents = false;

    this._updateDockNavigationItems();

    if (!this.dockedConfigurationSupportsSplitContentBrowser() && !this.doesCurrentTabSupportSplitContentBrowser())
        this.hideSplitConsole();
};

WI.updateVisibilityState = function(visible)
{
    this.visible = visible;
    this.notifications.dispatchEventToListeners(WI.Notification.VisibilityStateDidChange);
};

WI.handlePossibleLinkClick = function(event, frame, options = {})
{
    let anchorElement = event.target.closest("a");
    if (!anchorElement || !anchorElement.href)
        return false;

    if (WI.isBeingEdited(anchorElement)) {
        // Don't follow the link when it is being edited.
        return false;
    }

    // Prevent the link from navigating, since we don't do any navigation by following links normally.
    event.preventDefault();
    event.stopPropagation();

    this.openURL(anchorElement.href, frame, {
        ...options,
        lineNumber: anchorElement.lineNumber,
        ignoreSearchTab: !WI.isShowingSearchTab(),
    });

    return true;
};

WI.openURL = function(url, frame, options = {})
{
    console.assert(url);
    if (!url)
        return;

    console.assert(typeof options.lineNumber === "undefined" || typeof options.lineNumber === "number", "lineNumber should be a number.");

    // If alwaysOpenExternally is not defined, base it off the command/meta key for the current event.
    if (options.alwaysOpenExternally === undefined || options.alwaysOpenExternally === null)
        options.alwaysOpenExternally = window.event ? window.event.metaKey : false;

    if (options.alwaysOpenExternally) {
        InspectorFrontendHost.openInNewTab(url);
        return;
    }

    let searchChildFrames = false;
    if (!frame) {
        frame = this.networkManager.mainFrame;
        searchChildFrames = true;
    }

    let resource;
    let simplifiedURL = removeURLFragment(url);
    if (frame) {
        // WI.Frame.resourceForURL does not check the main resource, only sub-resources. So check both.
        resource = frame.url === simplifiedURL ? frame.mainResource : frame.resourceForURL(simplifiedURL, searchChildFrames);
    } else if (WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker)
        resource = WI.mainTarget.resourceCollection.resourceForURL(removeURLFragment(url));

    if (resource) {
        let positionToReveal = new WI.SourceCodePosition(options.lineNumber, 0);
        this.showSourceCode(resource, {...options, positionToReveal});
        return;
    }

    InspectorFrontendHost.openInNewTab(url);
};

WI.close = function()
{
    if (this._isClosing)
        return;

    this._isClosing = true;

    InspectorFrontendHost.closeWindow();
};

WI.isConsoleFocused = function()
{
    return this.quickConsole.prompt.focused;
};

WI.isShowingSplitConsole = function()
{
    return !this.consoleDrawer.collapsed;
};

WI.dockedConfigurationSupportsSplitContentBrowser = function()
{
    return this._dockConfiguration !== WI.DockConfiguration.Bottom;
};

WI.doesCurrentTabSupportSplitContentBrowser = function()
{
    var currentContentView = this.tabBrowser.selectedTabContentView;
    return !currentContentView || currentContentView.supportsSplitContentBrowser;
};

WI.toggleSplitConsole = function()
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

WI.showSplitConsole = function()
{
    if (!this.doesCurrentTabSupportSplitContentBrowser()) {
        this.showConsoleTab();
        return;
    }

    this.consoleDrawer.collapsed = false;

    if (this.consoleDrawer.currentContentView === this.consoleContentView)
        return;

    this.consoleDrawer.showContentView(this.consoleContentView);
};

WI.hideSplitConsole = function()
{
    if (!this.isShowingSplitConsole())
        return;

    this.consoleDrawer.collapsed = true;
};

WI.showConsoleTab = function(requestedScope)
{
    requestedScope = requestedScope || WI.LogContentView.Scopes.All;

    this.hideSplitConsole();

    this.consoleContentView.scopeBar.item(requestedScope).selected = true;

    this.showRepresentedObject(this._consoleRepresentedObject);

    console.assert(this.isShowingConsoleTab());
};

WI.isShowingConsoleTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.ConsoleTabContentView;
};

WI.showElementsTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.ElementsTabContentView);
    if (!tabContentView)
        tabContentView = new WI.ElementsTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingElementsTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.ElementsTabContentView;
};

WI.showSourcesTab = function(options = {})
{
    let tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.SourcesTabContentView);
    if (!tabContentView)
        tabContentView = new WI.SourcesTabContentView;

    if (options.breakpointToSelect instanceof WI.Breakpoint)
        tabContentView.revealAndSelectBreakpoint(options.breakpointToSelect);

    if (options.showScopeChainSidebar)
        tabContentView.showScopeChainDetailsSidebarPanel();

    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingSourcesTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.SourcesTabContentView;
};

WI.showDebuggerTab = function(options)
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.DebuggerTabContentView);
    if (!tabContentView)
        tabContentView = new WI.DebuggerTabContentView;

    if (options.breakpointToSelect instanceof WI.Breakpoint)
        tabContentView.revealAndSelectBreakpoint(options.breakpointToSelect);

    if (options.showScopeChainSidebar)
        tabContentView.showScopeChainDetailsSidebarPanel();

    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingDebuggerTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.DebuggerTabContentView;
};

WI.showResourcesTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.ResourcesTabContentView);
    if (!tabContentView)
        tabContentView = new WI.ResourcesTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingResourcesTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.ResourcesTabContentView;
};

WI.showStorageTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.StorageTabContentView);
    if (!tabContentView)
        tabContentView = new WI.StorageTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.showNetworkTab = function()
{
    let tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.NetworkTabContentView);
    if (!tabContentView)
        tabContentView = new WI.NetworkTabContentView;

    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingNetworkTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.NetworkTabContentView;
};

WI.isShowingSearchTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.SearchTabContentView;
};

WI.showTimelineTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.TimelineTabContentView);
    if (!tabContentView)
        tabContentView = new WI.TimelineTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.showLayersTab = function(options = {})
{
    let tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.LayersTabContentView);
    if (!tabContentView)
        tabContentView = new WI.LayersTabContentView;
    if (options.nodeToSelect)
        tabContentView.selectLayerForNode(options.nodeToSelect);
    this.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingLayersTab = function()
{
    return this.tabBrowser.selectedTabContentView instanceof WI.LayersTabContentView;
};

WI.indentString = function()
{
    if (WI.settings.indentWithTabs.value)
        return "\t";
    return " ".repeat(WI.settings.indentUnit.value);
};

WI.restoreFocusFromElement = function(element)
{
    if (element && element.contains(this.currentFocusElement))
        this.previousFocusElement.focus();
};

WI.toggleNavigationSidebar = function(event)
{
    if (!this.navigationSidebar.collapsed || !this.navigationSidebar.sidebarPanels.length) {
        this.navigationSidebar.collapsed = true;
        return;
    }

    if (!this.navigationSidebar.selectedSidebarPanel)
        this.navigationSidebar.selectedSidebarPanel = this.navigationSidebar.sidebarPanels[0];
    this.navigationSidebar.collapsed = false;
};

WI.toggleDetailsSidebar = function(event)
{
    if (!this.detailsSidebar.collapsed || !this.detailsSidebar.sidebarPanels.length) {
        this.detailsSidebar.collapsed = true;
        return;
    }

    if (!this.detailsSidebar.selectedSidebarPanel)
        this.detailsSidebar.selectedSidebarPanel = this.detailsSidebar.sidebarPanels[0];
    this.detailsSidebar.collapsed = false;
};

WI.getMaximumSidebarWidth = function(sidebar)
{
    console.assert(sidebar instanceof WI.Sidebar);

    const minimumContentBrowserWidth = 100;

    let minimumWidth = window.innerWidth - minimumContentBrowserWidth;
    let tabContentView = this.tabBrowser.selectedTabContentView;
    console.assert(tabContentView);
    if (!tabContentView)
        return minimumWidth;

    let otherSidebar = null;
    if (sidebar === this.navigationSidebar)
        otherSidebar = tabContentView.detailsSidebarPanels.length ? this.detailsSidebar : null;
    else
        otherSidebar = tabContentView.navigationSidebarPanel ? this.navigationSidebar : null;

    if (otherSidebar)
        minimumWidth -= otherSidebar.width;

    return minimumWidth;
};

WI.tabContentViewClassForRepresentedObject = function(representedObject)
{
    if (representedObject instanceof WI.DOMTree)
        return WI.ElementsTabContentView;

    if (representedObject instanceof WI.TimelineRecording)
        return WI.TimelineTabContentView;

    // We only support one console tab right now. So this isn't an instanceof check.
    if (representedObject === this._consoleRepresentedObject)
        return WI.ConsoleTabContentView;

    if (WI.settings.experimentalEnableSourcesTab.value) {
        if (representedObject instanceof WI.Frame
            || representedObject instanceof WI.FrameCollection
            || representedObject instanceof WI.Resource
            || representedObject instanceof WI.ResourceCollection
            || representedObject instanceof WI.Script
            || representedObject instanceof WI.ScriptCollection
            || representedObject instanceof WI.CSSStyleSheet
            || representedObject instanceof WI.CSSStyleSheetCollection)
            return WI.SourcesTabContentView;
    } else {
        if (WI.debuggerManager.paused) {
            if (representedObject instanceof WI.Script)
                return WI.DebuggerTabContentView;

            if (representedObject instanceof WI.Resource && (representedObject.type === WI.Resource.Type.Document || representedObject.type === WI.Resource.Type.Script))
                return WI.DebuggerTabContentView;
        }

        if (representedObject instanceof WI.Frame
            || representedObject instanceof WI.FrameCollection
            || representedObject instanceof WI.Resource
            || representedObject instanceof WI.ResourceCollection
            || representedObject instanceof WI.Script
            || representedObject instanceof WI.ScriptCollection
            || representedObject instanceof WI.CSSStyleSheet
            || representedObject instanceof WI.CSSStyleSheetCollection)
            return WI.ResourcesTabContentView;
    }

    if (representedObject instanceof WI.DOMStorageObject || representedObject instanceof WI.CookieStorageObject ||
        representedObject instanceof WI.DatabaseTableObject || representedObject instanceof WI.DatabaseObject ||
        representedObject instanceof WI.ApplicationCacheFrame || representedObject instanceof WI.IndexedDatabaseObjectStore ||
        representedObject instanceof WI.IndexedDatabase || representedObject instanceof WI.IndexedDatabaseObjectStoreIndex)
        return WI.StorageTabContentView;

    if (representedObject instanceof WI.AuditTestCase || representedObject instanceof WI.AuditTestGroup
        || representedObject instanceof WI.AuditTestCaseResult || representedObject instanceof WI.AuditTestGroupResult)
        return WI.AuditTabContentView;

    if (representedObject instanceof WI.CanvasCollection)
        return WI.CanvasTabContentView;

    if (representedObject instanceof WI.Recording)
        return WI.CanvasTabContentView;

    return null;
};

WI.tabContentViewForRepresentedObject = function(representedObject, options = {})
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

WI.showRepresentedObject = function(representedObject, cookie, options = {})
{
    let tabContentView = this.tabContentViewForRepresentedObject(representedObject, options);
    console.assert(tabContentView);
    if (!tabContentView)
        return;

    this.tabBrowser.showTabForContentView(tabContentView, options);
    tabContentView.showRepresentedObject(representedObject, cookie);
};

WI.showMainFrameDOMTree = function(nodeToSelect, options = {})
{
    console.assert(WI.networkManager.mainFrame);
    if (!WI.networkManager.mainFrame)
        return;
    this.showRepresentedObject(WI.networkManager.mainFrame.domTree, {nodeToSelect}, options);
};

WI.showSourceCodeForFrame = function(frameIdentifier, options = {})
{
    var frame = WI.networkManager.frameForIdentifier(frameIdentifier);
    if (!frame) {
        this._frameIdentifierToShowSourceCodeWhenAvailable = frameIdentifier;
        return;
    }

    this._frameIdentifierToShowSourceCodeWhenAvailable = undefined;

    this.showRepresentedObject(frame, null, options);
};

WI.showSourceCode = function(sourceCode, options = {})
{
    const positionToReveal = options.positionToReveal;

    console.assert(!positionToReveal || positionToReveal instanceof WI.SourceCodePosition, positionToReveal);
    var representedObject = sourceCode;

    if (representedObject instanceof WI.Script) {
        // A script represented by a resource should always show the resource.
        representedObject = representedObject.resource || representedObject;
    }

    var cookie = positionToReveal ? {lineNumber: positionToReveal.lineNumber, columnNumber: positionToReveal.columnNumber} : {};
    this.showRepresentedObject(representedObject, cookie, options);
};

WI.showSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    this.showSourceCode(sourceCodeLocation.displaySourceCode, {
        ...options,
        positionToReveal: sourceCodeLocation.displayPosition(),
    });
};

WI.showOriginalUnformattedSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    this.showSourceCode(sourceCodeLocation.sourceCode, {
        ...options,
        positionToReveal: sourceCodeLocation.position(),
        forceUnformatted: true,
    });
};

WI.showOriginalOrFormattedSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    this.showSourceCode(sourceCodeLocation.sourceCode, {
        ...options,
        positionToReveal: sourceCodeLocation.formattedPosition(),
    });
};

WI.showOriginalOrFormattedSourceCodeTextRange = function(sourceCodeTextRange, options = {})
{
    var textRangeToSelect = sourceCodeTextRange.formattedTextRange;
    this.showSourceCode(sourceCodeTextRange.sourceCode, {
        ...options,
        positionToReveal: textRangeToSelect.startPosition(),
        textRangeToSelect,
    });
};

WI.showResourceRequest = function(resource, options = {})
{
    this.showRepresentedObject(resource, {[WI.ResourceClusterContentView.ContentViewIdentifierCookieKey]: WI.ResourceClusterContentView.RequestIdentifier}, options);
};

WI.debuggerToggleBreakpoints = function(event)
{
    WI.debuggerManager.breakpointsEnabled = !WI.debuggerManager.breakpointsEnabled;
};

WI.debuggerPauseResumeToggle = function(event)
{
    if (WI.debuggerManager.paused)
        WI.debuggerManager.resume();
    else
        WI.debuggerManager.pause();
};

WI.debuggerStepOver = function(event)
{
    WI.debuggerManager.stepOver();
};

WI.debuggerStepInto = function(event)
{
    WI.debuggerManager.stepInto();
};

WI.debuggerStepOut = function(event)
{
    WI.debuggerManager.stepOut();
};

WI._searchTextDidChange = function(event)
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WI.SearchTabContentView);
    if (!tabContentView)
        tabContentView = new WI.SearchTabContentView;

    var searchQuery = this._searchToolbarItem.text;
    this._searchToolbarItem.text = "";

    this.tabBrowser.showTabForContentView(tabContentView);

    tabContentView.performSearch(searchQuery);
};

WI._focusSearchField = function(event)
{
    if (WI.settings.experimentalEnableNewTabBar.value)
        this.tabBrowser.showTabForContentView(this._searchTabContentView);

    if (this.tabBrowser.selectedTabContentView instanceof WI.SearchTabContentView) {
        this.tabBrowser.selectedTabContentView.focusSearchField();
        return;
    }

    if (this._searchToolbarItem)
        this._searchToolbarItem.focus();
};

WI._focusChanged = function(event)
{
    // Make a caret selection inside the focused element if there isn't a range selection and there isn't already
    // a caret selection inside. This is needed (at least) to remove caret from console when focus is moved.
    // The selection change should not apply to text fields and text areas either.

    if (WI.isEventTargetAnEditableField(event)) {
        // Still update the currentFocusElement if inside of a CodeMirror editor or an input element.
        let newFocusElement = null;
        if (event.target instanceof HTMLInputElement || event.target instanceof HTMLTextAreaElement)
            newFocusElement = event.target;
        else {
            let codeMirror = WI.enclosingCodeMirror(event.target);
            if (codeMirror) {
                let codeMirrorElement = codeMirror.getWrapperElement();
                if (codeMirrorElement && codeMirrorElement !== this.currentFocusElement)
                    newFocusElement = codeMirrorElement;
            }
        }

        if (newFocusElement) {
            this.previousFocusElement = this.currentFocusElement;
            this.currentFocusElement = newFocusElement;
        }

        // Due to the change in WI.isEventTargetAnEditableField (r196271), this return
        // will also get run when WI.startEditing is called on an element. We do not want
        // to return early in this case, as WI.EditingConfig handles its own editing
        // completion, so only return early if the focus change target is not from WI.startEditing.
        if (!WI.isBeingEdited(event.target))
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

WI._mouseWasClicked = function(event)
{
    this.handlePossibleLinkClick(event);
};

WI._dragOver = function(event)
{
    // Do nothing if another event listener handled the event already.
    if (event.defaultPrevented)
        return;

    // Allow dropping into editable areas.
    if (WI.isEventTargetAnEditableField(event))
        return;

    // Prevent the drop from being accepted.
    event.dataTransfer.dropEffect = "none";
    event.preventDefault();
};

WI._debuggerDidPause = function(event)
{
    if (WI.settings.experimentalEnableSourcesTab.value)
        WI.showSourcesTab({showScopeChainSidebar: WI.settings.showScopeChainOnPause.value});
    else
        WI.showDebuggerTab({showScopeChainSidebar: WI.settings.showScopeChainOnPause.value});

    this._dashboardContainer.showDashboardViewForRepresentedObject(this._dashboards.debugger);

    InspectorFrontendHost.bringToFront();
};

WI._debuggerDidResume = function(event)
{
    this._dashboardContainer.closeDashboardViewForRepresentedObject(this._dashboards.debugger);
};

WI._frameWasAdded = function(event)
{
    if (!this._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    var frame = event.data.frame;
    if (frame.id !== this._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    function delayedWork()
    {
        const options = {
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };
        this.showSourceCodeForFrame(frame.id, options);
    }

    // Delay showing the frame since FrameWasAdded is called before MainFrameChanged.
    // Calling showSourceCodeForFrame before MainFrameChanged will show the frame then close it.
    setTimeout(delayedWork.bind(this));
};

WI._mainFrameDidChange = function(event)
{
    this._updateDownloadToolbarButton();

    this.updateWindowTitle();
};

WI._mainResourceDidChange = function(event)
{
    if (!event.target.isMainFrame())
        return;

    // Run cookie restoration after we are sure all of the Tabs and NavigationSidebarPanels
    // have updated with respect to the main resource change.
    setTimeout(this._restoreCookieForOpenTabs.bind(this, WI.StateRestorationType.Navigation));

    this._updateDownloadToolbarButton();

    this.updateWindowTitle();
};

WI._provisionalLoadStarted = function(event)
{
    if (!event.target.isMainFrame())
        return;

    this._saveCookieForOpenTabs();
};

WI._restoreCookieForOpenTabs = function(restorationType)
{
    for (var tabBarItem of this.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        tabContentView.restoreStateFromCookie(restorationType);
    }
};

WI._saveCookieForOpenTabs = function()
{
    for (var tabBarItem of this.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        tabContentView.saveStateToCookie();
    }
};

WI._windowFocused = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.remove(this.docked ? "window-docked-inactive" : "window-inactive");
};

WI._windowBlurred = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.add(this.docked ? "window-docked-inactive" : "window-inactive");
};

WI._windowResized = function(event)
{
    this.toolbar.updateLayout(WI.View.LayoutReason.Resize);
    this.tabBar.updateLayout(WI.View.LayoutReason.Resize);
    this._tabBrowserSizeDidChange();
};

WI._updateModifierKeys = function(event)
{
    let metaKeyDidChange = this.modifierKeys.metaKey !== event.metaKey;
    let didChange = this.modifierKeys.altKey !== event.altKey || metaKeyDidChange || this.modifierKeys.shiftKey !== event.shiftKey;

    this.modifierKeys = {altKey: event.altKey, metaKey: event.metaKey, shiftKey: event.shiftKey};

    if (metaKeyDidChange)
        document.body.classList.toggle("meta-key-pressed", this.modifierKeys.metaKey);

    if (didChange)
        this.notifications.dispatchEventToListeners(WI.Notification.GlobalModifierKeysDidChange, event);
};

WI._windowKeyDown = function(event)
{
    this._updateModifierKeys(event);
};

WI._windowKeyUp = function(event)
{
    this._updateModifierKeys(event);
};

WI._mouseDown = function(event)
{
    if (this.toolbar.element.contains(event.target))
        this._toolbarMouseDown(event);
};

WI._mouseMoved = function(event)
{
    this._updateModifierKeys(event);
    this.mouseCoords = {
        x: event.pageX,
        y: event.pageY
    };
};

WI._pageHidden = function(event)
{
    this._saveCookieForOpenTabs();
};

WI._contextMenuRequested = function(event)
{
    let proposedContextMenu;

    // This is setting is only defined in engineering builds.
    if (WI.isDebugUIEnabled()) {
        proposedContextMenu = WI.ContextMenu.createFromEvent(event);
        proposedContextMenu.appendSeparator();
        proposedContextMenu.appendItem(WI.unlocalizedString("Reload Web Inspector"), () => {
            InspectorFrontendHost.reopen();
        });

        let protocolSubMenu = proposedContextMenu.appendSubMenuItem(WI.unlocalizedString("Protocol Debugging"), null, false);
        let isCapturingTraffic = InspectorBackend.activeTracer instanceof WI.CapturingProtocolTracer;

        protocolSubMenu.appendCheckboxItem(WI.unlocalizedString("Capture Trace"), () => {
            if (isCapturingTraffic)
                InspectorBackend.activeTracer = null;
            else
                InspectorBackend.activeTracer = new WI.CapturingProtocolTracer;
        }, isCapturingTraffic);

        protocolSubMenu.appendSeparator();

        protocolSubMenu.appendItem(WI.unlocalizedString("Export Trace\u2026"), () => {
            const forceSaveAs = true;
            WI.FileUtilities.save(InspectorBackend.activeTracer.trace.saveData, forceSaveAs);
        }, !isCapturingTraffic);
    } else {
        const onlyExisting = true;
        proposedContextMenu = WI.ContextMenu.createFromEvent(event, onlyExisting);
    }

    if (proposedContextMenu)
        proposedContextMenu.show();
};

WI.isDebugUIEnabled = function()
{
    return WI.showDebugUISetting && WI.showDebugUISetting.value;
};

WI._undock = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Undocked);
};

WI._dockBottom = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Bottom);
};

WI._dockRight = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Right);
};

WI._dockLeft = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Left);
};

WI._togglePreviousDockConfiguration = function(event)
{
    InspectorFrontendHost.requestSetDockSide(this._previousDockConfiguration);
};

WI._updateDockNavigationItems = function()
{
    if (this._dockingAvailable || this.docked) {
        this._closeToolbarButton.hidden = !this.docked;
        this._undockToolbarButton.hidden = this._dockConfiguration === WI.DockConfiguration.Undocked;
        this._dockBottomToolbarButton.hidden = this._dockConfiguration === WI.DockConfiguration.Bottom;
        this._dockToSideToolbarButton.hidden = this._dockConfiguration === WI.DockConfiguration.Right || this._dockConfiguration === WI.DockConfiguration.Left;
    } else {
        this._closeToolbarButton.hidden = true;
        this._undockToolbarButton.hidden = true;
        this._dockBottomToolbarButton.hidden = true;
        this._dockToSideToolbarButton.hidden = true;
    }
};

WI._tabBrowserSizeDidChange = function()
{
    this.tabBrowser.updateLayout(WI.View.LayoutReason.Resize);
    this.consoleDrawer.updateLayout(WI.View.LayoutReason.Resize);
    this.quickConsole.updateLayout(WI.View.LayoutReason.Resize);
};

WI._consoleDrawerCollapsedStateDidChange = function(event)
{
    this._showingSplitConsoleSetting.value = WI.isShowingSplitConsole();

    WI._consoleDrawerDidResize();
};

WI._consoleDrawerDidResize = function(event)
{
    this.tabBrowser.updateLayout(WI.View.LayoutReason.Resize);
};

WI._sidebarWidthDidChange = function(event)
{
    this._tabBrowserSizeDidChange();
};

WI._setupViewHierarchy = function()
{
    let rootView = WI.View.rootView();
    rootView.addSubview(this.toolbar);
    rootView.addSubview(this.tabBar);
    rootView.addSubview(this.navigationSidebar);
    rootView.addSubview(this.tabBrowser);
    rootView.addSubview(this.consoleDrawer);
    rootView.addSubview(this.quickConsole);
    rootView.addSubview(this.detailsSidebar);
};

WI._tabBrowserSelectedTabContentViewDidChange = function(event)
{
    if (this.tabBar.selectedTabBarItem && this.tabBar.selectedTabBarItem.representedObject.constructor.shouldSaveTab())
        this._selectedTabIndexSetting.value = this.tabBar.tabBarItems.indexOf(this.tabBar.selectedTabBarItem);

    if (this.doesCurrentTabSupportSplitContentBrowser()) {
        if (this._shouldRevealSpitConsoleIfSupported) {
            this._shouldRevealSpitConsoleIfSupported = false;
            this.showSplitConsole();
        }
        return;
    }

    this._shouldRevealSpitConsoleIfSupported = this.isShowingSplitConsole();
    this.hideSplitConsole();
};

WI._toolbarMouseDown = function(event)
{
    if (event.ctrlKey)
        return;

    if (this._dockConfiguration === WI.DockConfiguration.Right || this._dockConfiguration === WI.DockConfiguration.Left)
        return;

    if (this.docked)
        this._dockedResizerMouseDown(event);
    else
        this._moveWindowMouseDown(event);
};

WI._dockedResizerMouseDown = function(event)
{
    if (event.button !== 0 || event.ctrlKey)
        return;

    if (!this.docked)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (event.target.id !== "docked-resizer" && !event.target.classList.contains("toolbar") &&
        !event.target.classList.contains("flexible-space") && !event.target.classList.contains("item-section"))
        return;

    event[WI.Popover.EventPreventDismissSymbol] = true;

    let windowProperty = this._dockConfiguration === WI.DockConfiguration.Bottom ? "innerHeight" : "innerWidth";
    let eventScreenProperty = this._dockConfiguration === WI.DockConfiguration.Bottom ? "screenY" : "screenX";
    let eventClientProperty = this._dockConfiguration === WI.DockConfiguration.Bottom ? "clientY" : "clientX";

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

        if (this._dockConfiguration === WI.DockConfiguration.Left) {
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

        if (this._dockConfiguration === WI.DockConfiguration.Bottom)
            InspectorFrontendHost.setAttachedWindowHeight(dimension);
        else
            InspectorFrontendHost.setAttachedWindowWidth(dimension);
    }

    function dockedResizerDragEnd(event)
    {
        if (event.button !== 0)
            return;

        WI.elementDragEnd(event);
    }

    WI.elementDragStart(resizerElement, dockedResizerDrag.bind(this), dockedResizerDragEnd.bind(this), event, this._dockConfiguration === WI.DockConfiguration.Bottom ? "row-resize" : "col-resize");
};

WI._moveWindowMouseDown = function(event)
{
    console.assert(!this.docked);

    if (event.button !== 0 || event.ctrlKey)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (!event.target.classList.contains("toolbar") && !event.target.classList.contains("flexible-space") &&
        !event.target.classList.contains("item-section"))
        return;

    event[WI.Popover.EventPreventDismissSymbol] = true;

    if (WI.Platform.name === "mac") {
        InspectorFrontendHost.startWindowDrag();
        event.preventDefault();
        return;
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

        WI.elementDragEnd(event);
    }

    WI.elementDragStart(event.target, toolbarDrag, toolbarDragEnd, event, "default");
};

WI._domStorageWasInspected = function(event)
{
    this.showStorageTab();
    this.showRepresentedObject(event.data.domStorage, null, {ignoreSearchTab: true});
};

WI._databaseWasInspected = function(event)
{
    this.showStorageTab();
    this.showRepresentedObject(event.data.database, null, {ignoreSearchTab: true});
};

WI._domNodeWasInspected = function(event)
{
    this.domManager.highlightDOMNodeForTwoSeconds(event.data.node.id);

    InspectorFrontendHost.bringToFront();

    this.showElementsTab();
    this.showMainFrameDOMTree(event.data.node, {ignoreSearchTab: true});
};

WI._inspectModeStateChanged = function(event)
{
    this._inspectModeToolbarButton.activated = this.domManager.inspectModeEnabled;
};

WI._toggleInspectMode = function(event)
{
    this.domManager.inspectModeEnabled = !this.domManager.inspectModeEnabled;
};

WI._handleDeviceSettingsToolbarButtonClicked = function(event)
{
    if (WI._deviceSettingsPopover) {
        WI._deviceSettingsPopover.dismiss();
        WI._deviceSettingsPopover = null;
        return;
    }

    function updateActivatedState() {
        WI._deviceSettingsToolbarButton.activated = WI._overridenDeviceUserAgent || WI._overridenDeviceSettings.size > 0;
    }

    function applyOverriddenUserAgent(value, force) {
        if (value === WI._overridenDeviceUserAgent)
            return;

        if (!force && (!value || value === "default")) {
            PageAgent.overrideUserAgent((error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceUserAgent = null;
                updateActivatedState();
                PageAgent.reload();
            });
        } else {
            PageAgent.overrideUserAgent(value, (error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceUserAgent = value;
                updateActivatedState();
                PageAgent.reload();
            });
        }
    }

    function applyOverriddenSetting(setting, value, callback) {
        if (WI._overridenDeviceSettings.has(setting)) {
            // We've just "disabled" the checkbox, so clear the override instead of applying it.
            PageAgent.overrideSetting(setting, (error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceSettings.delete(setting);
                callback(false);
                updateActivatedState();
            });
        } else {
            PageAgent.overrideSetting(setting, value, (error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceSettings.set(setting, value);
                callback(true);
                updateActivatedState();
            });
        }
    }

    function createCheckbox(container, label, setting, value) {
        if (!setting)
            return;

        let labelElement = container.appendChild(document.createElement("label"));

        let checkboxElement = labelElement.appendChild(document.createElement("input"));
        checkboxElement.type = "checkbox";
        checkboxElement.checked = WI._overridenDeviceSettings.has(setting);
        checkboxElement.addEventListener("change", (event) => {
            applyOverriddenSetting(setting, value, (enabled) => {
                checkboxElement.checked = enabled;
            });
        });

        labelElement.append(label);
    }

    function calculateTargetFrame() {
        return WI.Rect.rectFromClientRect(WI._deviceSettingsToolbarButton.element.getBoundingClientRect());
    }

    const preferredEdges = [WI.RectEdge.MAX_Y, WI.RectEdge.MIN_X, WI.RectEdge.MAX_X];

    WI._deviceSettingsPopover = new WI.Popover(this);
    WI._deviceSettingsPopover.windowResizeHandler = function(event) {
        WI._deviceSettingsPopover.present(calculateTargetFrame(), preferredEdges);
    };

    let contentElement = document.createElement("table");
    contentElement.classList.add("device-settings-content");

    let userAgentRow = contentElement.appendChild(document.createElement("tr"));

    let userAgentTitle = userAgentRow.appendChild(document.createElement("td"));
    userAgentTitle.textContent = WI.UIString("User Agent:");

    let userAgentValue = userAgentRow.appendChild(document.createElement("td"));
    userAgentValue.classList.add("user-agent");

    let userAgentValueSelect = userAgentValue.appendChild(document.createElement("select"));

    let userAgentValueInput = null;

    const userAgents = [
        [
            { name: WI.UIString("Default"), value: "default" },
        ],
        [
            { name: "Safari 12.2", value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14_4) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.2 Safari/605.1.15" },
        ],
        [
            { name: `Safari ${emDash} iOS 12.1.3 ${emDash} iPhone`, value: "Mozilla/5.0 (iPhone; CPU iPhone OS 12_1_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1" },
            { name: `Safari ${emDash} iOS 12.1.3 ${emDash} iPod touch`, value: "Mozilla/5.0 (iPod; CPU iPhone OS 12_1_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1" },
            { name: `Safari ${emDash} iOS 12.1.3 ${emDash} iPad`, value: "Mozilla/5.0 (iPad; CPU iPhone OS 12_1_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1" },
        ],
        [
            { name: `Microsoft Edge`, value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299" },
        ],
        [
            { name: `Internet Explorer 11`, value: "Mozilla/5.0 (Windows NT 6.3; Win64, x64; Trident/7.0; rv:11.0) like Gecko" },
            { name: `Internet Explorer 10`, value: "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.2; Win64; x64; Trident/6.0)" },
            { name: `Internet Explorer 9`, value: "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)" },
            { name: `Internet Explorer 8`, value: "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.0; Trident/4.0)" },
            { name: `Internet Explorer 7`, value: "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0)" },
        ],
        [
            { name: `Google Chrome ${emDash} macOS`, value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14_3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36" },
            { name: `Google Chrome ${emDash} Windows`, value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36" },
        ],
        [
            { name: `Firefox ${emDash} macOS`, value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:63.0) Gecko/20100101 Firefox/63.0" },
            { name: `Firefox ${emDash} Windows`, value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:63.0) Gecko/20100101 Firefox/63.0" },
        ],
        [
            { name: WI.UIString("Other\u2026"), value: "other" },
        ],
    ];

    let selectedOptionElement = null;

    for (let group of userAgents) {
        for (let {name, value} of group) {
            let optionElement = userAgentValueSelect.appendChild(document.createElement("option"));
            optionElement.value = value;
            optionElement.textContent = name;

            if (value === WI._overridenDeviceUserAgent)
                selectedOptionElement = optionElement;
        }

        if (group !== userAgents.lastValue)
            userAgentValueSelect.appendChild(document.createElement("hr"));
    }

    function showUserAgentInput() {
        if (userAgentValueInput)
            return;

        userAgentValueInput = userAgentValue.appendChild(document.createElement("input"));
        userAgentValueInput.value = userAgentValueInput.placeholder = WI._overridenDeviceUserAgent || navigator.userAgent;
        userAgentValueInput.addEventListener("click", (clickEvent) => {
            clickEvent.preventDefault();
        });
        userAgentValueInput.addEventListener("change", (inputEvent) => {
            applyOverriddenUserAgent(userAgentValueInput.value, true);
        });

        WI._deviceSettingsPopover.update();
    }

    if (selectedOptionElement)
        userAgentValueSelect.value = selectedOptionElement.value;
    else if (WI._overridenDeviceUserAgent) {
        userAgentValueSelect.value = "other";
        showUserAgentInput();
    }

    userAgentValueSelect.addEventListener("change", () => {
        let value = userAgentValueSelect.value;
        if (value === "other") {
            showUserAgentInput();
            userAgentValueInput.select();
        } else {
            if (userAgentValueInput) {
                userAgentValueInput.remove();
                userAgentValueInput = null;

                WI._deviceSettingsPopover.update();
            }

            applyOverriddenUserAgent(value);
        }
    });

    const settings = [
        {
            name: WI.UIString("Disable:"),
            columns: [
                [
                    {name: WI.UIString("Images"), setting: PageAgent.Setting.ImagesEnabled, value: false},
                    {name: WI.UIString("Styles"), setting: PageAgent.Setting.AuthorAndUserStylesEnabled, value: false},
                    {name: WI.UIString("JavaScript"), setting: PageAgent.Setting.ScriptEnabled, value: false},
                ],
                [
                    {name: WI.UIString("Site-specific Hacks"), setting: PageAgent.Setting.NeedsSiteSpecificQuirks, value: false},
                    {name: WI.UIString("Cross-Origin Restrictions"), setting: PageAgent.Setting.WebSecurityEnabled, value: false},
                ]
            ],
        },
        {
            name: WI.UIString("%s:").format(WI.unlocalizedString("WebRTC")),
            columns: [
                [
                    {name: WI.UIString("Allow Media Capture on Insecure Sites"), setting: PageAgent.Setting.MediaCaptureRequiresSecureConnection, value: false},
                    {name: WI.UIString("Disable ICE Candidate Restrictions"), setting: PageAgent.Setting.ICECandidateFilteringEnabled, value: false},
                    {name: WI.UIString("Use Mock Capture Devices"), setting: PageAgent.Setting.MockCaptureDevicesEnabled, value: true},
                ],
            ],
        },
    ];

    for (let group of settings) {
        if (!group.columns.some((column) => column.some((item) => item.setting)))
            continue;

        let settingsGroupRow = contentElement.appendChild(document.createElement("tr"));

        let settingsGroupTitle = settingsGroupRow.appendChild(document.createElement("td"));
        settingsGroupTitle.textContent = group.name;

        let settingsGroupValue = settingsGroupRow.appendChild(document.createElement("td"));

        let settingsGroupItemContainer = settingsGroupValue.appendChild(document.createElement("div"));
        settingsGroupItemContainer.classList.add("container");

        for (let column of group.columns) {
            let columnElement = settingsGroupItemContainer.appendChild(document.createElement("div"));
            columnElement.classList.add("column");

            for (let item of column)
                createCheckbox(columnElement, item.name, item.setting, item.value);
        }
    }

    WI._deviceSettingsPopover.presentNewContentWithFrame(contentElement, calculateTargetFrame(), preferredEdges);
};

WI._downloadWebArchive = function(event)
{
    this.archiveMainFrame();
};

WI._reloadInspectedInspector = function()
{
    const options = {};
    WI.runtimeManager.evaluateInInspectedWindow(`InspectorFrontendHost.reopen()`, options, function(){});
};

WI._reloadPage = function(event)
{
    if (!window.PageAgent)
        return;

    event.preventDefault();

    if (InspectorFrontendHost.inspectionLevel() > 1) {
        WI._reloadInspectedInspector();
        return;
    }

    PageAgent.reload();
};

WI._reloadPageFromOrigin = function(event)
{
    if (!window.PageAgent)
        return;

    event.preventDefault();

    if (InspectorFrontendHost.inspectionLevel() > 1) {
        WI._reloadInspectedInspector();
        return;
    }

    PageAgent.reload.invoke({ignoreCache: true});
};

WI._reloadToolbarButtonClicked = function(event)
{
    if (InspectorFrontendHost.inspectionLevel() > 1) {
        WI._reloadInspectedInspector();
        return;
    }

    // Reload page from origin if the button is clicked while the shift key is pressed down.
    PageAgent.reload.invoke({ignoreCache: this.modifierKeys.shiftKey});
};

WI._updateReloadToolbarButton = function()
{
    if (!window.PageAgent) {
        this._reloadToolbarButton.hidden = true;
        return;
    }

    this._reloadToolbarButton.hidden = false;
};

WI._updateDownloadToolbarButton = function()
{
    if (!window.PageAgent || this.sharedApp.debuggableType !== WI.DebuggableType.Web) {
        this._downloadToolbarButton.hidden = true;
        return;
    }

    if (this._downloadingPage) {
        this._downloadToolbarButton.enabled = false;
        return;
    }

    this._downloadToolbarButton.enabled = this.canArchiveMainFrame();
};

WI._updateInspectModeToolbarButton = function()
{
    if (!window.DOMAgent || !DOMAgent.setInspectModeEnabled) {
        this._inspectModeToolbarButton.hidden = true;
        return;
    }

    this._inspectModeToolbarButton.hidden = false;
};

WI._toggleInspectMode = function(event)
{
    this.domManager.inspectModeEnabled = !this.domManager.inspectModeEnabled;
};

WI._showConsoleTab = function(event)
{
    this.showConsoleTab();
};

WI._focusConsolePrompt = function(event)
{
    this.quickConsole.prompt.focus();
};

WI._focusedContentBrowser = function()
{
    if (this.currentFocusElement) {
        let contentBrowserElement = this.currentFocusElement.closest(".content-browser");
        if (contentBrowserElement && contentBrowserElement.__view && contentBrowserElement.__view instanceof WI.ContentBrowser)
            return contentBrowserElement.__view;
    }

    if (this.tabBrowser.element.contains(this.currentFocusElement) || document.activeElement === document.body) {
        let tabContentView = this.tabBrowser.selectedTabContentView;
        if (tabContentView.contentBrowser)
            return tabContentView.contentBrowser;
        return null;
    }

    if (this.consoleDrawer.element.contains(this.currentFocusElement)
        || (WI.isShowingSplitConsole() && this.quickConsole.element.contains(this.currentFocusElement)))
        return this.consoleDrawer;

    return null;
};

WI._focusedContentView = function()
{
    if (this.tabBrowser.element.contains(this.currentFocusElement) || document.activeElement === document.body) {
        var tabContentView = this.tabBrowser.selectedTabContentView;
        if (tabContentView.contentBrowser)
            return tabContentView.contentBrowser.currentContentView;
        return tabContentView;
    }

    if (this.consoleDrawer.element.contains(this.currentFocusElement)
        || (WI.isShowingSplitConsole() && this.quickConsole.element.contains(this.currentFocusElement)))
        return this.consoleDrawer.currentContentView;

    return null;
};

WI._focusedOrVisibleContentBrowser = function()
{
    let focusedContentBrowser = this._focusedContentBrowser();
    if (focusedContentBrowser)
        return focusedContentBrowser;

    var tabContentView = this.tabBrowser.selectedTabContentView;
    if (tabContentView.contentBrowser)
        return tabContentView.contentBrowser;

    return null;
};

WI.focusedOrVisibleContentView = function()
{
    let focusedContentView = this._focusedContentView();
    if (focusedContentView)
        return focusedContentView;

    var tabContentView = this.tabBrowser.selectedTabContentView;
    if (tabContentView.contentBrowser)
        return tabContentView.contentBrowser.currentContentView;
    return tabContentView;
};

WI._beforecopy = function(event)
{
    var selection = window.getSelection();

    // If there is no selection, see if the focused element or focused ContentView can handle the copy event.
    if (selection.isCollapsed && !WI.isEventTargetAnEditableField(event)) {
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

WI._find = function(event)
{
    let contentBrowser = this._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.showFindBanner();
};

WI._save = function(event)
{
    var contentView = this.focusedOrVisibleContentView();
    if (!contentView || !contentView.supportsSave)
        return;

    WI.FileUtilities.save(contentView.saveData);
};

WI._saveAs = function(event)
{
    var contentView = this.focusedOrVisibleContentView();
    if (!contentView || !contentView.supportsSave)
        return;

    WI.FileUtilities.save(contentView.saveData, true);
};

WI._clear = function(event)
{
    let contentView = this.focusedOrVisibleContentView();
    if (!contentView || typeof contentView.handleClearShortcut !== "function") {
        // If the current content view is unable to handle this event, clear the console to reset
        // the dashboard counters.
        this.consoleManager.requestClearMessages();
        return;
    }

    contentView.handleClearShortcut(event);
};

WI._populateFind = function(event)
{
    let focusedContentView = this._focusedContentView();
    if (!focusedContentView)
        return;

    if (focusedContentView.supportsCustomFindBanner) {
        focusedContentView.handlePopulateFindShortcut();
        return;
    }

    let contentBrowser = this._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.handlePopulateFindShortcut();
};

WI._findNext = function(event)
{
    let focusedContentView = this._focusedContentView();
    if (!focusedContentView)
        return;

    if (focusedContentView.supportsCustomFindBanner) {
        focusedContentView.handleFindNextShortcut();
        return;
    }

    let contentBrowser = this._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.handleFindNextShortcut();
};

WI._findPrevious = function(event)
{
    let focusedContentView = this._focusedContentView();
    if (!focusedContentView)
        return;

    if (focusedContentView.supportsCustomFindBanner) {
        focusedContentView.handleFindPreviousShortcut();
        return;
    }

    let contentBrowser = this._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.handleFindPreviousShortcut();
};

WI._copy = function(event)
{
    var selection = window.getSelection();

    // If there is no selection, pass the copy event on to the focused element or focused ContentView.
    if (selection.isCollapsed && !WI.isEventTargetAnEditableField(event)) {
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

        let tabContentView = this.tabBrowser.selectedTabContentView;
        if (tabContentView && typeof tabContentView.handleCopyEvent === "function") {
            tabContentView.handleCopyEvent(event);
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

WI._increaseZoom = function(event)
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

WI._decreaseZoom = function(event)
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

WI._resetZoom = function(event)
{
    this.setZoomFactor(1);
};

WI.getZoomFactor = function()
{
    return WI.settings.zoomFactor.value;
};

WI.setZoomFactor = function(factor)
{
    InspectorFrontendHost.setZoomFactor(factor);
    // Round-trip through the frontend host API in case the requested factor is not used.
    WI.settings.zoomFactor.value = InspectorFrontendHost.zoomFactor();
};

WI.resolvedLayoutDirection = function()
{
    let layoutDirection = WI.settings.layoutDirection.value;
    if (layoutDirection === WI.LayoutDirection.System)
        layoutDirection = InspectorFrontendHost.userInterfaceLayoutDirection();

    return layoutDirection;
};

WI.setLayoutDirection = function(value)
{
    if (!Object.values(WI.LayoutDirection).includes(value))
        WI.reportInternalError("Unknown layout direction requested: " + value);

    if (value === WI.settings.layoutDirection.value)
        return;

    WI.settings.layoutDirection.value = value;

    if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL && this._dockConfiguration === WI.DockConfiguration.Right)
        this._dockLeft();

    if (WI.resolvedLayoutDirection() === WI.LayoutDirection.LTR && this._dockConfiguration === WI.DockConfiguration.Left)
        this._dockRight();

    InspectorFrontendHost.reopen();
};

WI._showTabAtIndex = function(i, event)
{
    if (i <= WI.tabBar.tabBarItems.length)
        WI.tabBar.selectedTabBarItem = i - 1;
};

WI._showJavaScriptTypeInformationSettingChanged = function(event)
{
    if (WI.settings.showJavaScriptTypeInformation.value) {
        for (let target of WI.targets)
            target.RuntimeAgent.enableTypeProfiler();
    } else {
        for (let target of WI.targets)
            target.RuntimeAgent.disableTypeProfiler();
    }
};

WI._enableControlFlowProfilerSettingChanged = function(event)
{
    if (WI.settings.enableControlFlowProfiler.value) {
        for (let target of WI.targets)
            target.RuntimeAgent.enableControlFlowProfiler();
    } else {
        for (let target of WI.targets)
            target.RuntimeAgent.disableControlFlowProfiler();
    }
};

WI._resourceCachingDisabledSettingChanged = function(event)
{
    NetworkAgent.setResourceCachingDisabled(WI.settings.resourceCachingDisabled.value);
};

WI.elementDragStart = function(element, dividerDrag, elementDragEnd, event, cursor, eventTarget)
{
    if (WI._elementDraggingEventListener || WI._elementEndDraggingEventListener)
        WI.elementDragEnd(event);

    if (element) {
        // Install glass pane
        if (WI._elementDraggingGlassPane)
            WI._elementDraggingGlassPane.remove();

        var glassPane = document.createElement("div");
        glassPane.style.cssText = "position:absolute;top:0;bottom:0;left:0;right:0;opacity:0;z-index:1";
        glassPane.id = "glass-pane-for-drag";
        element.ownerDocument.body.appendChild(glassPane);
        WI._elementDraggingGlassPane = glassPane;
    }

    WI._elementDraggingEventListener = dividerDrag;
    WI._elementEndDraggingEventListener = elementDragEnd;

    var targetDocument = event.target.ownerDocument;

    WI._elementDraggingEventTarget = eventTarget || targetDocument;
    WI._elementDraggingEventTarget.addEventListener("mousemove", dividerDrag, true);
    WI._elementDraggingEventTarget.addEventListener("mouseup", elementDragEnd, true);

    targetDocument.body.style.cursor = cursor;

    event.preventDefault();
};

WI.elementDragEnd = function(event)
{
    WI._elementDraggingEventTarget.removeEventListener("mousemove", WI._elementDraggingEventListener, true);
    WI._elementDraggingEventTarget.removeEventListener("mouseup", WI._elementEndDraggingEventListener, true);

    event.target.ownerDocument.body.style.removeProperty("cursor");

    if (WI._elementDraggingGlassPane)
        WI._elementDraggingGlassPane.remove();

    delete WI._elementDraggingGlassPane;
    delete WI._elementDraggingEventTarget;
    delete WI._elementDraggingEventListener;
    delete WI._elementEndDraggingEventListener;

    event.preventDefault();
};

WI.createMessageTextView = function(message, isError)
{
    var messageElement = document.createElement("div");
    messageElement.className = "message-text-view";
    if (isError)
        messageElement.classList.add("error");

    let textElement = messageElement.appendChild(document.createElement("div"));
    textElement.className = "message";
    textElement.textContent = message;

    return messageElement;
};

WI.createNavigationItemHelp = function(formatString, navigationItem)
{
    console.assert(typeof formatString === "string");
    console.assert(navigationItem instanceof WI.NavigationItem);

    function append(a, b) {
        a.append(b);
        return a;
    }

    let containerElement = document.createElement("div");
    containerElement.className = "navigation-item-help";
    containerElement.__navigationItem = navigationItem;

    let wrapperElement = document.createElement("div");
    wrapperElement.className = "navigation-bar";
    wrapperElement.appendChild(navigationItem.element);

    String.format(formatString, [wrapperElement], String.standardFormatters, containerElement, append);
    return containerElement;
};

WI.createGoToArrowButton = function()
{
    var button = document.createElement("button");
    button.addEventListener("mousedown", (event) => { event.stopPropagation(); }, true);
    button.className = "go-to-arrow";
    button.tabIndex = -1;
    return button;
};

WI.createSourceCodeLocationLink = function(sourceCodeLocation, options = {})
{
    console.assert(sourceCodeLocation);
    if (!sourceCodeLocation)
        return null;

    var linkElement = document.createElement("a");
    linkElement.className = "go-to-link";
    WI.linkifyElement(linkElement, sourceCodeLocation, options);
    sourceCodeLocation.populateLiveDisplayLocationTooltip(linkElement);

    if (options.useGoToArrowButton)
        linkElement.appendChild(WI.createGoToArrowButton());
    else
        sourceCodeLocation.populateLiveDisplayLocationString(linkElement, "textContent", options.columnStyle, options.nameStyle, options.prefix);

    if (options.dontFloat)
        linkElement.classList.add("dont-float");

    return linkElement;
};

WI.linkifyLocation = function(url, sourceCodePosition, options = {})
{
    var sourceCode = WI.sourceCodeForURL(url);

    if (!sourceCode) {
        var anchor = document.createElement("a");
        anchor.href = url;
        anchor.lineNumber = sourceCodePosition.lineNumber;
        if (options.className)
            anchor.className = options.className;
        anchor.append(WI.displayNameForURL(url) + ":" + sourceCodePosition.lineNumber);
        return anchor;
    }

    let sourceCodeLocation = sourceCode.createSourceCodeLocation(sourceCodePosition.lineNumber, sourceCodePosition.columnNumber);
    let linkElement = WI.createSourceCodeLocationLink(sourceCodeLocation, {
        ...options,
        dontFloat: true,
    });

    if (options.className)
        linkElement.classList.add(options.className);

    return linkElement;
};

WI.linkifyElement = function(linkElement, sourceCodeLocation, options = {}) {
    console.assert(sourceCodeLocation);

    function showSourceCodeLocation(event)
    {
        event.stopPropagation();
        event.preventDefault();

        if (event.metaKey)
            this.showOriginalUnformattedSourceCodeLocation(sourceCodeLocation, options);
        else
            this.showSourceCodeLocation(sourceCodeLocation, options);
    }

    linkElement.addEventListener("click", showSourceCodeLocation.bind(this));
    linkElement.addEventListener("contextmenu", (event) => {
        let contextMenu = WI.ContextMenu.createFromEvent(event);
        WI.appendContextMenuItemsForSourceCode(contextMenu, sourceCodeLocation);
    });
};

WI.sourceCodeForURL = function(url)
{
    var sourceCode = WI.networkManager.resourceForURL(url);
    if (!sourceCode) {
        sourceCode = WI.debuggerManager.scriptsForURL(url, WI.assumingMainTarget())[0];
        if (sourceCode)
            sourceCode = sourceCode.resource || sourceCode;
    }
    return sourceCode || null;
};

WI.linkifyURLAsNode = function(url, linkText, className)
{
    let a = document.createElement("a");
    a.href = url;
    a.className = className || "";
    a.textContent = linkText || url;
    a.style.maxWidth = "100%";
    return a;
};

WI.linkifyStringAsFragmentWithCustomLinkifier = function(string, linkifier)
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

WI.linkifyStringAsFragment = function(string)
{
    function linkifier(title, url, lineNumber)
    {
        var urlNode = WI.linkifyURLAsNode(url, title, undefined);
        if (lineNumber !== undefined)
            urlNode.lineNumber = lineNumber;

        return urlNode;
    }

    return WI.linkifyStringAsFragmentWithCustomLinkifier(string, linkifier);
};

WI.createResourceLink = function(resource, className)
{
    function handleClick(event)
    {
        event.stopPropagation();
        event.preventDefault();

        WI.showRepresentedObject(resource);
    }

    let linkNode = document.createElement("a");
    linkNode.classList.add("resource-link", className);
    linkNode.title = resource.url;
    linkNode.textContent = (resource.urlComponents.lastPathComponent || resource.url).insertWordBreakCharacters();
    linkNode.addEventListener("click", handleClick.bind(this));
    return linkNode;
};

WI._undoKeyboardShortcut = function(event)
{
    if (!this.isEditingAnyField() && !this.isEventTargetAnEditableField(event)) {
        this.undo();
        event.preventDefault();
    }
};

WI._redoKeyboardShortcut = function(event)
{
    if (!this.isEditingAnyField() && !this.isEventTargetAnEditableField(event)) {
        this.redo();
        event.preventDefault();
    }
};

WI.undo = function()
{
    DOMAgent.undo();
};

WI.redo = function()
{
    DOMAgent.redo();
};

WI.highlightRangesWithStyleClass = function(element, resultRanges, styleClass, changes)
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

WI.revertDOMChanges = function(domChanges)
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

WI.archiveMainFrame = function()
{
    this._downloadingPage = true;
    this._updateDownloadToolbarButton();

    PageAgent.archive((error, data) => {
        this._downloadingPage = false;
        this._updateDownloadToolbarButton();

        if (error)
            return;

        let mainFrame = WI.networkManager.mainFrame;
        let archiveName = mainFrame.mainResource.urlComponents.host || mainFrame.mainResource.displayName || "Archive";
        let url = "web-inspector:///" + encodeURI(archiveName) + ".webarchive";

        InspectorFrontendHost.save(url, data, true, true);
    });
};

WI.canArchiveMainFrame = function()
{
    if (this.sharedApp.debuggableType !== WI.DebuggableType.Web)
        return false;

    if (!WI.networkManager.mainFrame || !WI.networkManager.mainFrame.mainResource)
        return false;

    return WI.Resource.typeFromMIMEType(WI.networkManager.mainFrame.mainResource.mimeType) === WI.Resource.Type.Document;
};

WI.addWindowKeydownListener = function(listener)
{
    if (typeof listener.handleKeydownEvent !== "function")
        return;

    this._windowKeydownListeners.push(listener);

    this._updateWindowKeydownListener();
};

WI.removeWindowKeydownListener = function(listener)
{
    this._windowKeydownListeners.remove(listener);

    this._updateWindowKeydownListener();
};

WI._updateWindowKeydownListener = function()
{
    if (this._windowKeydownListeners.length === 1)
        window.addEventListener("keydown", WI._sharedWindowKeydownListener, true);
    else if (!this._windowKeydownListeners.length)
        window.removeEventListener("keydown", WI._sharedWindowKeydownListener, true);
};

WI._sharedWindowKeydownListener = function(event)
{
    for (var i = WI._windowKeydownListeners.length - 1; i >= 0; --i) {
        if (WI._windowKeydownListeners[i].handleKeydownEvent(event)) {
            event.stopImmediatePropagation();
            event.preventDefault();
            break;
        }
    }
};

WI.reportInternalError = function(errorOrString, details = {})
{
    // The 'details' object includes additional information from the caller as free-form string keys and values.
    // Each key and value will be shown in the uncaught exception reporter, console error message, or in
    // a pre-filled bug report generated for this internal error.

    let error = errorOrString instanceof Error ? errorOrString : new Error(errorOrString);
    error.details = details;

    // The error will be displayed in the Uncaught Exception Reporter sheet if DebugUI is enabled.
    if (WI.isDebugUIEnabled()) {
        // This assert allows us to stop the debugger at an internal exception. It doesn't re-throw
        // exceptions because the original exception would be lost through window.onerror.
        // This workaround can be removed once <https://webkit.org/b/158192> is fixed.
        console.assert(false, "An internal exception was thrown.", error);
        handleInternalException(error);
    } else
        console.error(error);
};

// Many places assume the "main" target has resources.
// In the case where the main backend target is a MultiplexingBackendTarget
// that target has essentially nothing. In that case defer to the page
// target, since that is the real "main" target the frontend is assuming.
Object.defineProperty(WI, "mainTarget",
{
    get() { return WI.pageTarget || WI.backendTarget; }
});

// This list of targets are non-Multiplexing targets.
// So if there is a multiplexing target, and multiple sub-targets
// this is just the list of sub-targets. Almost no code expects
// to actually interact with the Multiplexing target.
Object.defineProperty(WI, "targets",
{
    get() { return WI.targetManager.targets; }
});

// Many places assume the main target because they cannot yet be
// used by reached by Worker debugging. Eventually, once all
// Worker domains have been implemented, all of these must be
// handled properly.
WI.assumingMainTarget = function()
{
    return WI.mainTarget;
};

WI.isEngineeringBuild = false;

// OpenResourceDialog delegate

WI.dialogWasDismissedWithRepresentedObject = function(dialog, representedObject)
{
    if (!representedObject)
        return;

    WI.showRepresentedObject(representedObject, dialog.cookie);
};

// Popover delegate

WI.didDismissPopover = function(popover)
{
    if (popover === WI._deviceSettingsPopover)
        WI._deviceSettingsPopover = null;
};

WI.DockConfiguration = {
    Right: "right",
    Left: "left",
    Bottom: "bottom",
    Undocked: "undocked",
};
