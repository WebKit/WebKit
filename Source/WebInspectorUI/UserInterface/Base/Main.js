/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
    if (InspectorBackend.registerDatabaseDispatcher)
        InspectorBackend.registerDatabaseDispatcher(new WebInspector.DatabaseObserver);
    if (InspectorBackend.registerDOMStorageDispatcher)
        InspectorBackend.registerDOMStorageDispatcher(new WebInspector.DOMStorageObserver);
    if (InspectorBackend.registerApplicationCacheDispatcher)
        InspectorBackend.registerApplicationCacheDispatcher(new WebInspector.ApplicationCacheObserver);
    if (InspectorBackend.registerTimelineDispatcher)
        InspectorBackend.registerTimelineDispatcher(new WebInspector.TimelineObserver);
    if (InspectorBackend.registerCSSDispatcher)
        InspectorBackend.registerCSSDispatcher(new WebInspector.CSSObserver);
    if (InspectorBackend.registerLayerTreeDispatcher)
        InspectorBackend.registerLayerTreeDispatcher(new WebInspector.LayerTreeObserver);
    if (InspectorBackend.registerRuntimeDispatcher)
        InspectorBackend.registerRuntimeDispatcher(new WebInspector.RuntimeObserver);
    if (InspectorBackend.registerReplayDispatcher)
        InspectorBackend.registerReplayDispatcher(new WebInspector.ReplayObserver);

    // Enable agents.
    if (window.InspectorAgent)
        InspectorAgent.enable();

    // Perform one-time tasks.
    WebInspector.CSSCompletions.requestCSSNameCompletions();
    this._generateDisclosureTriangleImages();

    // Listen for the ProvisionalLoadStarted event before registering for events so our code gets called before any managers or sidebars.
    // This lets us save a state cookie before any managers or sidebars do any resets that would affect state (namely TimelineManager).
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ProvisionalLoadStarted, this._provisionalLoadStarted, this);

    // Create the singleton managers next, before the user interface elements, so the user interface can register
    // as event listeners on these managers.
    this.branchManager = new WebInspector.BranchManager;
    this.frameResourceManager = new WebInspector.FrameResourceManager;
    this.storageManager = new WebInspector.StorageManager;
    this.domTreeManager = new WebInspector.DOMTreeManager;
    this.cssStyleManager = new WebInspector.CSSStyleManager;
    this.logManager = new WebInspector.LogManager;
    this.issueManager = new WebInspector.IssueManager;
    this.analyzerManager = new WebInspector.AnalyzerManager;
    this.runtimeManager = new WebInspector.RuntimeManager;
    this.applicationCacheManager = new WebInspector.ApplicationCacheManager;
    this.timelineManager = new WebInspector.TimelineManager;
    this.debuggerManager = new WebInspector.DebuggerManager;
    this.sourceMapManager = new WebInspector.SourceMapManager;
    this.layerTreeManager = new WebInspector.LayerTreeManager;
    this.dashboardManager = new WebInspector.DashboardManager;
    this.probeManager = new WebInspector.ProbeManager;
    this.replayManager = new WebInspector.ReplayManager;

    // Enable the Console Agent after creating the singleton managers.
    if (window.ConsoleAgent)
        ConsoleAgent.enable();

    // Tell the backend we are initialized after all our initialization messages have been sent.
    setTimeout(function() {
        if (window.InspectorAgent && InspectorAgent.initialized)
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
    this._toolbarDockedRightDisplayModeSetting = new WebInspector.Setting("toolbar-docked-right-display-mode", WebInspector.Toolbar.DisplayMode.IconAndLabelVertical);
    this._toolbarDockedRightSizeModeSetting = new WebInspector.Setting("toolbar-docked-right-size-mode",WebInspector.Toolbar.SizeMode.Normal);

    this._toolbarDockedBottomDisplayModeSetting = new WebInspector.Setting("toolbar-docked-display-mode", WebInspector.Toolbar.DisplayMode.IconAndLabelHorizontal);
    this._toolbarDockedBottomSizeModeSetting = new WebInspector.Setting("toolbar-docked-size-mode",WebInspector.Toolbar.SizeMode.Small);

    this._toolbarUndockedDisplayModeSetting = new WebInspector.Setting("toolbar-undocked-display-mode", WebInspector.Toolbar.DisplayMode.IconAndLabelVertical);
    this._toolbarUndockedSizeModeSetting = new WebInspector.Setting("toolbar-undocked-size-mode",WebInspector.Toolbar.SizeMode.Normal);

    this._showingSplitConsoleSetting = new WebInspector.Setting("showing-split-console", false);
    this._splitConsoleHeightSetting = new WebInspector.Setting("split-console-height", 150);

    this._dockButtonToggledSetting = new WebInspector.Setting("dock-button-toggled", false);

    this._openTabsSetting = new WebInspector.Setting("open-tabs", ["elements", "resources", "timeline", "debugger", "console"]);
    this._selectedTabIndexSetting = new WebInspector.Setting("selected-tab-index", 0);

    this.showShadowDOMSetting = new WebInspector.Setting("show-shadow-dom", false);
    this.showReplayInterfaceSetting = new WebInspector.Setting("show-web-replay", false);

    this.showJavaScriptTypeInformationSetting = new WebInspector.Setting("show-javascript-type-information", false);
    if (this.showJavaScriptTypeInformationSetting.value && window.RuntimeAgent && RuntimeAgent.enableTypeProfiler)
        RuntimeAgent.enableTypeProfiler();

    this.showPaintRectsSetting = new WebInspector.Setting("show-paint-rects", false);
    if (this.showPaintRectsSetting.value && window.PageAgent && PageAgent.setShowPaintRects)
        PageAgent.setShowPaintRects(true);

    this.mouseCoords = {
        x: 0,
        y: 0
    };

    this._windowKeydownListeners = [];
};

WebInspector.contentLoaded = function()
{
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
    window.addEventListener("mousemove", this._mouseMoved.bind(this), true);
    window.addEventListener("pagehide", this._pageHidden.bind(this));

    // Add platform style classes so the UI can be tweaked per-platform.
    document.body.classList.add(WebInspector.Platform.name + "-platform");
    if (WebInspector.Platform.isNightlyBuild)
        document.body.classList.add("nightly-build");
    if (WebInspector.Platform.isLegacyMacOS)
        document.body.classList.add("legacy");
    if (WebInspector.Platform.version.name)
        document.body.classList.add(WebInspector.Platform.version.name);

    document.body.classList.add(this.debuggableType);

    // Create the user interface elements.
    this.toolbar = new WebInspector.Toolbar(document.getElementById("toolbar"));
    this.toolbar.addEventListener(WebInspector.Toolbar.Event.DisplayModeDidChange, this._toolbarDisplayModeDidChange, this);
    this.toolbar.addEventListener(WebInspector.Toolbar.Event.SizeModeDidChange, this._toolbarSizeModeDidChange, this);

    this.tabBar = new WebInspector.TabBar(document.getElementById("tab-bar"));

    var contentElement = document.getElementById("content");
    contentElement.setAttribute("role", "main");
    contentElement.setAttribute("aria-label", WebInspector.UIString("Content"));

    this.splitContentBrowser = new WebInspector.ContentBrowser(document.getElementById("split-content-browser"), this, true);
    this.splitContentBrowser.navigationBar.element.addEventListener("mousedown", this._consoleResizerMouseDown.bind(this));

    this.quickConsole = new WebInspector.QuickConsole(document.getElementById("quick-console"));
    this.quickConsole.addEventListener(WebInspector.QuickConsole.Event.DidResize, this._quickConsoleDidResize, this);

    this._consoleRepresentedObject = new WebInspector.LogObject;
    this._consoleTreeElement = new WebInspector.LogTreeElement(this._consoleRepresentedObject);
    this.consoleContentView = WebInspector.splitContentBrowser.contentViewForRepresentedObject(this._consoleRepresentedObject);
    this.consoleLogViewController = this.consoleContentView.logViewController;

    // FIXME: The sidebars should be flipped in RTL languages.
    this.navigationSidebar = new WebInspector.Sidebar(document.getElementById("navigation-sidebar"), WebInspector.Sidebar.Sides.Left);
    this.navigationSidebar.addEventListener(WebInspector.Sidebar.Event.WidthDidChange, this._sidebarWidthDidChange, this);

    this.detailsSidebar = new WebInspector.Sidebar(document.getElementById("details-sidebar"), WebInspector.Sidebar.Sides.Right, null, null, WebInspector.UIString("Details"));
    this.detailsSidebar.addEventListener(WebInspector.Sidebar.Event.WidthDidChange, this._sidebarWidthDidChange, this);

    this.navigationSidebarKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "0", this.toggleNavigationSidebar.bind(this));
    this.detailsSidebarKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Option, "0", this.toggleDetailsSidebar.bind(this));

    this.tabBrowser = new WebInspector.TabBrowser(document.getElementById("tab-browser"), this.tabBar, this.navigationSidebar, this.detailsSidebar);
    this.tabBrowser.addEventListener(WebInspector.TabBrowser.Event.SelectedTabContentViewDidChange, this._tabBrowserSelectedTabContentViewDidChange, this);

    this._reloadPageKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "R", this._reloadPage.bind(this));
    this._reloadPageIgnoringCacheKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl | WebInspector.KeyboardShortcut.Modifier.Shift, "R", this._reloadPageIgnoringCache.bind(this));

    this._consoleKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Option | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "C", this._showConsoleTab.bind(this));

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

    this.undockButtonNavigationItem = new WebInspector.ToggleControlToolbarItem("undock", WebInspector.UIString("Detach into separate window"), "", platformImagePath("Undock.svg"), "", 16, 14);
    this.undockButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._undock, this);

    this.closeButtonNavigationItem = new WebInspector.ControlToolbarItem("dock-close", WebInspector.UIString("Close"), platformImagePath("Close.svg"), 16, 14);
    this.closeButtonNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this.close, this);

    this.toolbar.addToolbarItem(this.closeButtonNavigationItem, WebInspector.Toolbar.Section.Control);
    this.toolbar.addToolbarItem(this.undockButtonNavigationItem, WebInspector.Toolbar.Section.Control);

    this.dashboardContainer = new WebInspector.DashboardContainerView;
    this.dashboardContainer.showDashboardViewForRepresentedObject(this.dashboardManager.dashboards.default);

    this.toolbar.addToolbarItem(this.dashboardContainer.toolbarItem, WebInspector.Toolbar.Section.Center);

    this.resourceDetailsSidebarPanel = new WebInspector.ResourceDetailsSidebarPanel;
    this.domNodeDetailsSidebarPanel = new WebInspector.DOMNodeDetailsSidebarPanel;
    this.cssStyleDetailsSidebarPanel = new WebInspector.CSSStyleDetailsSidebarPanel;
    this.applicationCacheDetailsSidebarPanel = new WebInspector.ApplicationCacheDetailsSidebarPanel;
    this.scopeChainDetailsSidebarPanel = new WebInspector.ScopeChainDetailsSidebarPanel;
    this.probeDetailsSidebarPanel = new WebInspector.ProbeDetailsSidebarPanel;
    this.renderingFrameDetailsSidebarPanel = new WebInspector.RenderingFrameDetailsSidebarPanel;

    if (window.LayerTreeAgent)
        this.layerTreeDetailsSidebarPanel = new WebInspector.LayerTreeDetailsSidebarPanel;

    this.modifierKeys = {altKey: false, metaKey: false, shiftKey: false};

    this.toolbar.element.addEventListener("mousedown", this._toolbarMouseDown.bind(this));
    document.getElementById("docked-resizer").addEventListener("mousedown", this._dockedResizerMouseDown.bind(this));

    this._updateDockNavigationItems();
    this._updateToolbarHeight();

    for (var tabType of this._openTabsSetting.value) {
        var tabContentView = this._tabContentViewForType(tabType);
        if (!tabContentView)
            continue;
        this.tabBrowser.addTabForContentView(tabContentView, true);
    }

    this.tabBar.selectedTabBarItem = this._selectedTabIndexSetting.value;

    if (!this.tabBar.selectedTabBarItem)
        this.tabBar.selectedTabBarItem = 0;

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

    this._contentLoaded = true;

    this.runBootstrapOperations();
};

WebInspector._tabContentViewForType = function(tabType)
{
    switch (tabType) {
    case WebInspector.ElementsTabContentView.Type:
        return new WebInspector.ElementsTabContentView;
    case WebInspector.ResourcesTabContentView.Type:
        return new WebInspector.ResourcesTabContentView;
    case WebInspector.TimelineTabContentView.Type:
        return new WebInspector.TimelineTabContentView;
    case WebInspector.DebuggerTabContentView.Type:
        return new WebInspector.DebuggerTabContentView;
    case WebInspector.ConsoleTabContentView.Type:
        return new WebInspector.ConsoleTabContentView;
    default:
        console.error("Unknown tab type", tabType);
    }

    return null;
};

WebInspector._rememberOpenTabs = function()
{
    var openTabs = [];

    for (var tabBarItem of this.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (tabContentView instanceof WebInspector.SettingsTabContentView)
            continue;
        console.assert(tabContentView.type, "Tab type can't be null, undefined, or empty string", tabContentView.type, tabContentView);
        openTabs.push(tabContentView.type);
    }

    this._openTabsSetting.value = openTabs;
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

    WebInspector.CSSCompletions.requestCSSNameCompletions();
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
    console.assert(mainFrame);

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

WebInspector.updateDockedState = function(side)
{
    if (this._dockSide === side)
        return;

    this._dockSide = side;

    this.docked = side !== "undocked";

    this._ignoreToolbarModeDidChangeEvents = true;

    if (side === "bottom") {
        document.body.classList.add("docked", "bottom");
        document.body.classList.remove("window-inactive", "right");

        this.toolbar.displayMode = this._toolbarDockedBottomDisplayModeSetting.value;
        this.toolbar.sizeMode = this._toolbarDockedBottomSizeModeSetting.value;
    } else if (side === "right") {
        document.body.classList.add("docked", "right");
        document.body.classList.remove("window-inactive", "bottom");

        this.toolbar.displayMode = this._toolbarDockedRightDisplayModeSetting.value;
        this.toolbar.sizeMode = this._toolbarDockedRightSizeModeSetting.value;
    } else {
        document.body.classList.remove("docked", "right", "bottom");

        this.toolbar.displayMode = this._toolbarUndockedDisplayModeSetting.value;
        this.toolbar.sizeMode = this._toolbarUndockedSizeModeSetting.value;
    }

    this._ignoreToolbarModeDidChangeEvents = false;

    this._updateDockNavigationItems();
    this._updateToolbarHeight();
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
    var resource = frame.url === url ? frame.mainResource : frame.resourceForURL(url, searchChildFrames);
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

WebInspector.isConsoleFocused = function()
{
    return this.quickConsole.prompt.focused;
};

WebInspector.isShowingSplitConsole = function()
{
    return !this.splitContentBrowser.element.classList.contains("hidden");
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
    this.hideSplitConsole();

    var scope = requestedScope || WebInspector.LogContentView.Scopes.All;

    // If the requested scope is already selected and the console is showing, then switch back to All.
    if (this.isShowingConsoleTab() && this.consoleContentView.scopeBar.item(scope).selected)
        scope = WebInspector.LogContentView.Scopes.All;

    if (requestedScope || !this.consoleContentView.scopeBar.selectedItems.length)
        this.consoleContentView.scopeBar.item(scope).selected = true;

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

WebInspector.showResourcesTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.ResourcesTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.ResourcesTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
};

WebInspector.showTimelineTab = function()
{
    var tabContentView = this.tabBrowser.bestTabContentViewForClass(WebInspector.TimelineTabContentView);
    if (!tabContentView)
        tabContentView = new WebInspector.TimelineTabContentView;
    this.tabBrowser.showTabForContentView(tabContentView);
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

    return null;
};

WebInspector.tabContentViewForRepresentedObject = function(representedObject)
{
    var tabContentView = this.tabBrowser.bestTabContentViewForRepresentedObject(representedObject);
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

WebInspector.showRepresentedObject = function(representedObject, cookie, forceShowTab)
{
    var tabContentView = this.tabContentViewForRepresentedObject(representedObject);
    console.assert(tabContentView);
    if (!tabContentView)
        return;

    if (window.event || forceShowTab)
        this.tabBrowser.showTabForContentView(tabContentView);

    tabContentView.showRepresentedObject(representedObject, cookie);
};

WebInspector.showMainFrameDOMTree = function(nodeToSelect, forceShowTab)
{
    console.assert(WebInspector.frameResourceManager.mainFrame);
    if (!WebInspector.frameResourceManager.mainFrame)
        return;
    this.showRepresentedObject(WebInspector.frameResourceManager.mainFrame.domTree, {nodeToSelect}, forceShowTab);
};

WebInspector.showContentFlowDOMTree = function(contentFlow, nodeToSelect, forceShowTab)
{
    this.showRepresentedObject(contentFlow, {nodeToSelect}, forceShowTab);
};

WebInspector.showSourceCodeForFrame = function(frameIdentifier, forceShowTab)
{
    var frame = WebInspector.frameResourceManager.frameForIdentifier(frameIdentifier);
    if (!frame) {
        this._frameIdentifierToShowSourceCodeWhenAvailable = frameIdentifier;
        return;
    }

    this._frameIdentifierToShowSourceCodeWhenAvailable = undefined;

    this.showRepresentedObject(frame, null, forceShowTab);
};

WebInspector.showSourceCode = function(sourceCode, positionToReveal, textRangeToSelect, forceUnformatted, forceShowTab)
{
    console.assert(!positionToReveal || positionToReveal instanceof WebInspector.SourceCodePosition, positionToReveal);
    var representedObject = sourceCode;

    if (representedObject instanceof WebInspector.Script) {
        // A script represented by a resource should always show the resource.
        representedObject = representedObject.resource || representedObject;
    }

    var cookie = positionToReveal ? {lineNumber: positionToReveal.lineNumber, columnNumber: positionToReveal.columnNumber} : {};
    this.showRepresentedObject(representedObject, cookie, forceShowTab);
};

WebInspector.showSourceCodeLocation = function(sourceCodeLocation, forceShowTab)
{
    this.showSourceCode(sourceCodeLocation.displaySourceCode, sourceCodeLocation.displayPosition(), null, false, forceShowTab);
};

WebInspector.showOriginalUnformattedSourceCodeLocation = function(sourceCodeLocation, forceShowTab)
{
    this.showSourceCode(sourceCodeLocation.sourceCode, sourceCodeLocation.position(), null, true);
};

WebInspector.showOriginalOrFormattedSourceCodeLocation = function(sourceCodeLocation, forceShowTab)
{
    this.showSourceCode(sourceCodeLocation.sourceCode, sourceCodeLocation.formattedPosition(), null, false, forceShowTab);
};

WebInspector.showOriginalOrFormattedSourceCodeTextRange = function(sourceCodeTextRange, forceShowTab)
{
    var textRangeToSelect = sourceCodeTextRange.formattedTextRange;
    this.showSourceCode(sourceCodeTextRange.sourceCode, textRangeToSelect.startPosition(), textRangeToSelect, false, forceShowTab);
};

WebInspector.showResourceRequest = function(resource, forceShowTab)
{
    this.showRepresentedObject(resource, {[WebInspector.ResourceClusterContentView.ContentViewIdentifierCookieKey]: WebInspector.ResourceClusterContentView.RequestIdentifier}, forceShowTab);
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

WebInspector._focusChanged = function(event)
{
    // Make a caret selection inside the focused element if there isn't a range selection and there isn't already
    // a caret selection inside. This is needed (at least) to remove caret from console when focus is moved.
    // The selection change should not apply to text fields and text areas either.

    if (WebInspector.isEventTargetAnEditableField(event))
        return;

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
    this.dashboardContainer.showDashboardViewForRepresentedObject(this.dashboardManager.dashboards.replay);
};

WebInspector._debuggerDidPause = function(event)
{
    this.showDebuggerTab();

    this.dashboardContainer.showDashboardViewForRepresentedObject(this.dashboardManager.dashboards.debugger);

    InspectorFrontendHost.bringToFront();
};

WebInspector._debuggerDidResume = function(event)
{
    this.dashboardContainer.closeDashboardViewForRepresentedObject(this.dashboardManager.dashboards.debugger);
};

WebInspector._frameWasAdded = function(event)
{
    if (!this._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    var frame = event.data.frame;
    if (frame.id !== this._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    this.showSourceCodeForFrame(frame.id);
};

WebInspector._mainFrameDidChange = function(event)
{
    this.updateWindowTitle();
};

WebInspector._mainResourceDidChange = function(event)
{
    if (!event.target.isMainFrame())
        return;

    this._inProvisionalLoad = false;

    this.updateWindowTitle();
};

WebInspector._provisionalLoadStarted = function(event)
{
    if (!event.target.isMainFrame())
        return;

    this._inProvisionalLoad = true;
};

WebInspector._windowFocused = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE || this.docked)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.remove("window-inactive");
};

WebInspector._windowBlurred = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE || this.docked)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.add("window-inactive");
};

WebInspector._windowResized = function(event)
{
    this.toolbar.updateLayout();
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

    var opposite = !this._dockButtonToggledSetting.value;
    this.undockButtonNavigationItem.toggled = (event.altKey && !event.metaKey && !event.shiftKey) ? opposite : !opposite;
};

WebInspector._windowKeyUp = function(event)
{
    this._updateModifierKeys(event);

    var opposite = !this._dockButtonToggledSetting.value;
    this.undockButtonNavigationItem.toggled = (event.altKey && !event.metaKey && !event.shiftKey) ? opposite : !opposite;
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
};

WebInspector._undock = function(event)
{
    this._dockButtonToggledSetting.value = this.undockButtonNavigationItem.toggled;

    if (this.undockButtonNavigationItem.toggled)
        InspectorFrontendHost.requestSetDockSide(this._dockSide === "bottom" ? "right" : "bottom");
    else
        InspectorFrontendHost.requestSetDockSide("undocked");
};

WebInspector._updateDockNavigationItems = function()
{
    // The close and undock buttons are only available when docked.
    var docked = this.docked;
    this.closeButtonNavigationItem.hidden = !docked;
    this.undockButtonNavigationItem.hidden = !docked;

    if (docked) {
        this.undockButtonNavigationItem.alternateImage = this._dockSide === "bottom" ? platformImagePath("DockRight.svg") : platformImagePath("DockBottom.svg");
        this.undockButtonNavigationItem.alternateToolTip = this._dockSide === "bottom" ? WebInspector.UIString("Dock to right of window") : WebInspector.UIString("Dock to bottom of window");
    }

    this.undockButtonNavigationItem.toggled = this._dockButtonToggledSetting.value;
};

WebInspector._tabBrowserSizeDidChange = function()
{
    this.tabBrowser.updateLayout();
    this.splitContentBrowser.updateLayout();
    this.quickConsole.updateLayout();
};

WebInspector._quickConsoleDidResize = function(event)
{
    this.tabBrowser.updateLayout();
};

WebInspector._sidebarWidthDidChange = function(event)
{
    this._tabBrowserSizeDidChange();
};

WebInspector._updateToolbarHeight = function()
{
    if (WebInspector.Platform.isLegacyMacOS)
        InspectorFrontendHost.setToolbarHeight(this.toolbar.element.offsetHeight);
};

WebInspector._toolbarDisplayModeDidChange = function(event)
{
    if (this._ignoreToolbarModeDidChangeEvents)
        return;

    if (this._dockSide === "bottom")
        this._toolbarDockedBottomDisplayModeSetting.value = this.toolbar.displayMode;
    else if (this._dockSide === "right")
        this._toolbarDockedRightDisplayModeSetting.value = this.toolbar.displayMode;
    else
        this._toolbarUndockedDisplayModeSetting.value = this.toolbar.displayMode;

    this._updateToolbarHeight();
};

WebInspector._toolbarSizeModeDidChange = function(event)
{
    if (this._ignoreToolbarModeDidChangeEvents)
        return;

    if (this._dockSide === "bottom")
        this._toolbarDockedBottomSizeModeSetting.value = this.toolbar.sizeMode;
    else if (this._dockSide === "right")
        this._toolbarDockedRightSizeModeSetting.value = this.toolbar.sizeMode;
    else
        this._toolbarUndockedSizeModeSetting.value = this.toolbar.sizeMode;

    this._updateToolbarHeight();
};

WebInspector._tabBrowserSelectedTabContentViewDidChange = function(event)
{
    if (this.tabBar.selectedTabBarItem)
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

    if (this._dockSide === "right")
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

    var windowProperty = this._dockSide === "bottom" ? "innerHeight" : "innerWidth";
    var eventScreenProperty = this._dockSide === "bottom" ? "screenY" : "screenX";
    var eventClientProperty = this._dockSide === "bottom" ? "clientY" : "clientX";

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

        // If delta is positive the docked Inspector size is decreasing, in which case the cursor client position
        // with respect to the target cannot be less than the first mouse down position within the target.
        if (delta > 0 && clientPosition < firstClientPosition)
            return;

        // If delta is negative the docked Inspector size is increasing, in which case the cursor client position
        // with respect to the target cannot be greater than the first mouse down position within the target.
        if (delta < 0 && clientPosition > firstClientPosition)
            return;

        var dimension = Math.max(0, window[windowProperty] - delta);

        if (this._dockSide === "bottom")
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

    WebInspector.elementDragStart(resizerElement, dockedResizerDrag.bind(this), dockedResizerDragEnd.bind(this), event, this._dockSide === "bottom" ? "row-resize" : "col-resize");
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

    // Ignore dragging on the top of the toolbar on Mac where the inspector content fills the entire window.
    if (WebInspector.Platform.name === "mac" && WebInspector.Platform.version.release >= 10) {
        const windowDragHandledTitleBarHeight = 22;
        if (event.pageY < windowDragHandledTitleBarHeight) {
            event.preventDefault();
            return;
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
    // FIXME: This should show a Storage tab when we have one.
    this.showResourcesTab();
};

WebInspector._domNodeWasInspected = function(event)
{
    WebInspector.domTreeManager.highlightDOMNodeForTwoSeconds(event.data.node.id);

    InspectorFrontendHost.bringToFront();

    this.showMainFrameDOMTree(event.data.node);
};

WebInspector._inspectModeStateChanged = function(event)
{
//    this._inspectModeToolbarButton.activated = WebInspector.domTreeManager.inspectModeEnabled;
};

WebInspector._toggleInspectMode = function(event)
{
    WebInspector.domTreeManager.inspectModeEnabled = !WebInspector.domTreeManager.inspectModeEnabled;
};

WebInspector._reloadPage = function(event)
{
    PageAgent.reload();
};

WebInspector._reloadPageIgnoringCache = function(event)
{
    PageAgent.reload(true);
};

WebInspector._toggleInspectMode = function(event)
{
    this.domTreeManager.inspectModeEnabled = !this.domTreeManager.inspectModeEnabled;
};

WebInspector._showConsoleTab = function(event)
{
    this.showConsoleTab();
};

WebInspector._focusedContentView = function()
{
    if (this.tabBrowser.element.isSelfOrAncestor(this.currentFocusElement))
        return this.tabBrowser.selectedTabContentView;
    if (this.splitContentBrowser.element.isSelfOrAncestor(this.currentFocusElement))
        return  this.splitContentBrowser.currentContentView;
    return null;
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

WebInspector._generateDisclosureTriangleImages = function()
{
    var specifications = {};
    specifications["normal"] = {fillColor: [140, 140, 140, 1]};
    specifications["normal-active"] = {fillColor: [128, 128, 128, 1]};

    generateColoredImagesForCSS("Images/DisclosureTriangleSmallOpen.svg", specifications, 13, 13, "disclosure-triangle-small-open-");
    generateColoredImagesForCSS("Images/DisclosureTriangleSmallClosed.svg", specifications, 13, 13, "disclosure-triangle-small-closed-");

    specifications["selected"] = {fillColor: [255, 255, 255, 1]};

    generateColoredImagesForCSS("Images/DisclosureTriangleTinyOpen.svg", specifications, 8, 8, "disclosure-triangle-tiny-open-");
    generateColoredImagesForCSS("Images/DisclosureTriangleTinyClosed.svg", specifications, 8, 8, "disclosure-triangle-tiny-closed-");
};

WebInspector.elementDragStart = function(element, dividerDrag, elementDragEnd, event, cursor, eventTarget)
{
    if (WebInspector._elementDraggingEventListener || WebInspector._elementEndDraggingEventListener)
        WebInspector.elementDragEnd(event);

    if (element) {
        // Install glass pane
        if (WebInspector._elementDraggingGlassPane)
            WebInspector._elementDraggingGlassPane.parentElement.removeChild(WebInspector._elementDraggingGlassPane);

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
        WebInspector._elementDraggingGlassPane.parentElement.removeChild(WebInspector._elementDraggingGlassPane);

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
    if (!WebInspector._generatedGoToArrowButtonImages) {
        WebInspector._generatedGoToArrowButtonImages = true;

        var specifications = {};
        specifications["go-to-arrow-normal"] = {fillColor: [0, 0, 0, 0.5]};
        specifications["go-to-arrow-normal-active"] = {fillColor: [0, 0, 0, 0.7]};
        specifications["go-to-arrow-selected"] = {fillColor: [255, 255, 255, 0.8]};
        specifications["go-to-arrow-selected-active"] = {fillColor: [255, 255, 255, 1]};

        generateColoredImagesForCSS("Images/GoToArrow.svg", specifications, 10, 10);
    }

    function stopPropagation(event)
    {
        event.stopPropagation()
    }

    var button = document.createElement("button");
    button.addEventListener("mousedown", stopPropagation, true);
    button.className = "go-to-arrow";
    button.tabIndex = -1;
    return button;
};

WebInspector.createSourceCodeLocationLink = function(sourceCodeLocation, dontFloat, useGoToArrowButton)
{
    console.assert(sourceCodeLocation);
    if (!sourceCodeLocation)
        return null;

    function showSourceCodeLocation(event)
    {
        event.stopPropagation();
        event.preventDefault();

        if (event.metaKey)
            this.showOriginalUnformattedSourceCodeLocation(sourceCodeLocation);
        else
            this.showSourceCodeLocation(sourceCodeLocation);
    }

    var linkElement = document.createElement("a");
    linkElement.className = "go-to-link";
    linkElement.addEventListener("click", showSourceCodeLocation.bind(this));
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
    var sourceCode = WebInspector.frameResourceManager.resourceForURL(url);
    if (!sourceCode) {
        sourceCode = WebInspector.debuggerManager.scriptsForURL(url)[0];
        if (sourceCode)
            sourceCode = sourceCode.resource || sourceCode;
    }

    if (!sourceCode) {
        var anchor = document.createElement("a");
        anchor.href  = url;
        anchor.lineNumber = lineNumber;
        if (className)
            anchor.className = className;
        anchor.appendChild(document.createTextNode(WebInspector.displayNameForURL(url) + ":" + lineNumber));
        return anchor;
    }

    var sourceCodeLocation = sourceCode.createSourceCodeLocation(lineNumber, columnNumber);
    var linkElement = WebInspector.createSourceCodeLocationLink(sourceCodeLocation, true);
    if (className)
        linkElement.classList.add(className);
    return linkElement;
};

WebInspector.linkifyURLAsNode = function(url, linkText, classes, tooltipText)
{
    if (!linkText)
        linkText = url;

    classes = (classes ? classes + " " : "");

    var a = document.createElement("a");
    a.href = url;
    a.className = classes;

    if (tooltipText === undefined)
        a.title = url;
    else if (typeof tooltipText !== "string" || tooltipText.length)
        a.title = tooltipText;

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
        container.appendChild(document.createTextNode(nonLink));

        var title = linkString;
        var realURL = (linkString.startsWith("www.") ? "http://" + linkString : linkString);
        var lineColumnMatch = lineColumnRegEx.exec(realURL);
        if (lineColumnMatch)
            realURL = realURL.substring(0, realURL.length - lineColumnMatch[0].length);

        var linkNode = linkifier(title, realURL, lineColumnMatch ? lineColumnMatch[1] : undefined);
        container.appendChild(linkNode);
        string = string.substring(linkIndex + linkString.length, string.length);
    }

    if (string)
        container.appendChild(document.createTextNode(string));

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
        changes.push({ node: lastTextNode, type: "changed", oldText: lastText, newText: lastTextNode.textContent });

        if (startIndex === endIndex) {
            lastTextNode.parentElement.insertBefore(highlightNode, lastTextNode);
            changes.push({ node: highlightNode, type: "added", nextSibling: lastTextNode, parent: lastTextNode.parentElement });
            highlightNodes.push(highlightNode);

            var prefixNode = ownerDocument.createTextNode(lastText.substring(0, startOffset - nodeRanges[startIndex].offset));
            lastTextNode.parentElement.insertBefore(prefixNode, highlightNode);
            changes.push({ node: prefixNode, type: "added", nextSibling: highlightNode, parent: lastTextNode.parentElement });
        } else {
            var firstTextNode = textNodeSnapshot.snapshotItem(startIndex);
            var firstText = firstTextNode.textContent;
            var anchorElement = firstTextNode.nextSibling;

            firstTextNode.parentElement.insertBefore(highlightNode, anchorElement);
            changes.push({ node: highlightNode, type: "added", nextSibling: anchorElement, parent: firstTextNode.parentElement });
            highlightNodes.push(highlightNode);

            firstTextNode.textContent = firstText.substring(0, startOffset - nodeRanges[startIndex].offset);
            changes.push({ node: firstTextNode, type: "changed", oldText: firstText, newText: firstTextNode.textContent });

            for (var j = startIndex + 1; j < endIndex; j++) {
                var textNode = textNodeSnapshot.snapshotItem(j);
                var text = textNode.textContent;
                textNode.textContent = "";
                changes.push({ node: textNode, type: "changed", oldText: text, newText: textNode.textContent });
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
            if (entry.node.parentElement)
                entry.node.parentElement.removeChild(entry.node);
            break;
        case "changed":
            entry.node.textContent = entry.oldText;
            break;
        }
    }
};

WebInspector.archiveMainFrame = function()
{
    this.notifications.dispatchEventToListeners(WebInspector.Notification.PageArchiveStarted, event);

    PageAgent.archive(function(error, data) {
        this.notifications.dispatchEventToListeners(WebInspector.Notification.PageArchiveEnded, event);
        if (error)
            return;

        var mainFrame = WebInspector.frameResourceManager.mainFrame;
        var archiveName = mainFrame.mainResource.urlComponents.host || mainFrame.mainResource.displayName || "Archive";
        var url = "web-inspector:///" + encodeURI(archiveName) + ".webarchive";
        InspectorFrontendHost.save(url, data, true, true);
    }.bind(this));
};

WebInspector.canArchiveMainFrame = function()
{
    if (!PageAgent.archive || this.debuggableType !== WebInspector.DebuggableType.Web)
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
    if (this._windowKeydownListeners.length > 0)
        window.addEventListener("keydown", WebInspector._sharedWindowKeydownListener, true);
    else
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
