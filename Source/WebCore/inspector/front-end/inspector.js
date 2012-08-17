/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

var WebInspector = {
    _panelDescriptors: function()
    {
        this.panels = {};
        WebInspector.inspectorView = new WebInspector.InspectorView();
        var parentElement = document.getElementById("main");
        WebInspector.inspectorView.show(parentElement);
        WebInspector.inspectorView.addEventListener(WebInspector.InspectorView.Events.PanelSelected, this._panelSelected, this);

        var elements = new WebInspector.PanelDescriptor("elements", WebInspector.UIString("Elements"), "ElementsPanel", "ElementsPanel.js");
        var resources = new WebInspector.PanelDescriptor("resources", WebInspector.UIString("Resources"), "ResourcesPanel", "ResourcesPanel.js");
        var network = new WebInspector.PanelDescriptor("network", WebInspector.UIString("Network"), undefined, undefined, new WebInspector.NetworkPanel());
        var scripts = new WebInspector.PanelDescriptor("scripts", WebInspector.UIString("Sources"), undefined, undefined, new WebInspector.ScriptsPanel());
        var timeline = new WebInspector.PanelDescriptor("timeline", WebInspector.UIString("Timeline"), "TimelinePanel", "TimelinePanel.js");
        var profiles = new WebInspector.PanelDescriptor("profiles", WebInspector.UIString("Profiles"), undefined, undefined, new WebInspector.ProfilesPanel());
        var audits = new WebInspector.PanelDescriptor("audits", WebInspector.UIString("Audits"), "AuditsPanel", "AuditsPanel.js");
        var console = new WebInspector.PanelDescriptor("console", WebInspector.UIString("Console"), "ConsolePanel");
        var allDescriptors = [elements, resources, network, scripts, timeline, profiles, audits, console];

        var panelDescriptors = [];
        if (WebInspector.WorkerManager.isWorkerFrontend()) {
            panelDescriptors.push(scripts);
            panelDescriptors.push(timeline);
            panelDescriptors.push(profiles);
            panelDescriptors.push(console);
            return panelDescriptors;
        }
        var allDescriptors = [elements, resources, network, scripts, timeline, profiles, audits, console];
        var hiddenPanels = (InspectorFrontendHost.hiddenPanels() || "").split(',');
        for (var i = 0; i < allDescriptors.length; ++i) {
            if (hiddenPanels.indexOf(allDescriptors[i].name) === -1)
                panelDescriptors.push(allDescriptors[i]);
        }
        return panelDescriptors;
    },

    _panelSelected: function()
    {
        this._toggleConsoleButton.disabled = WebInspector.inspectorView.currentPanel().name === "console";
    },

    _createGlobalStatusBarItems: function()
    {
        var bottomStatusBarContainer = document.getElementById("bottom-status-bar-container");
        this._dockToggleButton = new WebInspector.StatusBarButton("", "dock-status-bar-item", 3);
        this._dockToggleButton.makeLongClickEnabled(this._createDockOptions.bind(this));
        this._dockToggleButton.addEventListener("click", this._toggleAttach.bind(this), false);
        this._updateDockButtonState();

        var mainStatusBar = document.getElementById("main-status-bar");
        mainStatusBar.insertBefore(this._dockToggleButton.element, bottomStatusBarContainer);

        this._toggleConsoleButton = new WebInspector.StatusBarButton(WebInspector.UIString("Show console."), "console-status-bar-item");
        this._toggleConsoleButton.addEventListener("click", this._toggleConsoleButtonClicked.bind(this), false);
        mainStatusBar.insertBefore(this._toggleConsoleButton.element, bottomStatusBarContainer);

        if (!WebInspector.WorkerManager.isWorkerFrontend()) {
            this._nodeSearchButton = new WebInspector.StatusBarButton(WebInspector.UIString("Select an element in the page to inspect it."), "node-search-status-bar-item");
            this._nodeSearchButton.addEventListener("click", this._toggleSearchingForNode, this);
            mainStatusBar.insertBefore(this._nodeSearchButton.element, bottomStatusBarContainer);
        }

        mainStatusBar.appendChild(this.settingsController.statusBarItem);
    },

    _createDockOptions: function()
    {
        var alternateDockToggleButton1 = new WebInspector.StatusBarButton("Dock to main window.", "dock-status-bar-item", 3);
        var alternateDockToggleButton2 = new WebInspector.StatusBarButton("Undock into separate window.", "dock-status-bar-item", 3);

        if (this.attached) {
            alternateDockToggleButton1.state = WebInspector.settings.dockToRight.get() ? "bottom" : "right";
            alternateDockToggleButton2.state = "undock";
        } else {
            alternateDockToggleButton1.state = WebInspector.settings.dockToRight.get() ? "bottom" : "right";
            alternateDockToggleButton2.state = WebInspector.settings.dockToRight.get() ? "right" : "bottom";
        }

        alternateDockToggleButton1.addEventListener("click", onClick.bind(this), false);
        alternateDockToggleButton2.addEventListener("click", onClick.bind(this), false);

        function onClick(e)
        {
            var state = e.target.state;
            if (state === "undock")
                this._toggleAttach();
            else if (state === "right") {
                if (!this.attached)
                    this._toggleAttach();
                WebInspector.settings.dockToRight.set(true);
            } else if (state === "bottom") {
                if (!this.attached)
                    this._toggleAttach();
                WebInspector.settings.dockToRight.set(false);
            }
        }

        return [alternateDockToggleButton1, alternateDockToggleButton2];
    },

    _updateDockButtonState: function()
    {
        if (!this._dockToggleButton)
            return;

        if (this.attached) {
            this._dockToggleButton.disabled = false;
            this._dockToggleButton.state = "undock";
            this._dockToggleButton.title = WebInspector.UIString("Undock into separate window.");
        } else {
            this._dockToggleButton.disabled = this._isDockingUnavailable;
            this._dockToggleButton.state = WebInspector.settings.dockToRight.get() ? "right" : "bottom";
            this._dockToggleButton.title = WebInspector.UIString("Dock to main window.");
        }
    },

    _toggleAttach: function()
    {
        if (!this._attached) {
            InspectorFrontendHost.requestAttachWindow();
            WebInspector.userMetrics.WindowDocked.record();
        } else {
            InspectorFrontendHost.requestDetachWindow();
            WebInspector.userMetrics.WindowUndocked.record();
        }
    },

    _toggleConsoleButtonClicked: function()
    {
        if (this._toggleConsoleButton.disabled)
            return;

        this._toggleConsoleButton.toggled = !this._toggleConsoleButton.toggled;

        var animationType = window.event && window.event.shiftKey ? WebInspector.Drawer.AnimationType.Slow : WebInspector.Drawer.AnimationType.Normal;
        if (this._toggleConsoleButton.toggled) {
            this._toggleConsoleButton.title = WebInspector.UIString("Hide console.");
            this.drawer.show(this.consoleView, animationType);
            this._consoleWasShown = true;
        } else {
            this._toggleConsoleButton.title = WebInspector.UIString("Show console.");
            this.drawer.hide(animationType);
            delete this._consoleWasShown;
        }
    },

    /**
     * @param {Element} statusBarElement
     * @param {WebInspector.View} view
     * @param {function()=} onclose
     */
    showViewInDrawer: function(statusBarElement, view, onclose)
    {
        this._toggleConsoleButton.title = WebInspector.UIString("Hide console.");
        this._toggleConsoleButton.toggled = false;
        this._closePreviousDrawerView();

        var drawerStatusBarHeader = document.createElement("div");
        drawerStatusBarHeader.className = "drawer-header status-bar-item";
        drawerStatusBarHeader.appendChild(statusBarElement);
        drawerStatusBarHeader.onclose = onclose;

        var closeButton = drawerStatusBarHeader.createChild("span");
        closeButton.textContent = WebInspector.UIString("\u00D7");
        closeButton.addStyleClass("drawer-header-close-button");
        closeButton.addEventListener("click", this.closeViewInDrawer.bind(this), false);

        document.getElementById("panel-status-bar").firstElementChild.appendChild(drawerStatusBarHeader);
        this._drawerStatusBarHeader = drawerStatusBarHeader;
        this.drawer.show(view, WebInspector.Drawer.AnimationType.Immediately);
    },

    closeViewInDrawer: function()
    {
        if (this._drawerStatusBarHeader) {
            this._closePreviousDrawerView();

            // Once drawer is closed console should be shown if it was shown before current view replaced it in drawer. 
            if (!this._consoleWasShown)
                this.drawer.hide(WebInspector.Drawer.AnimationType.Immediately);
            else
                this._toggleConsoleButtonClicked();
        }
    },

    _closePreviousDrawerView: function()
    {
        if (this._drawerStatusBarHeader) {
            this._drawerStatusBarHeader.parentElement.removeChild(this._drawerStatusBarHeader);
            if (this._drawerStatusBarHeader.onclose)
                this._drawerStatusBarHeader.onclose();
            delete this._drawerStatusBarHeader;
        }
    },

    get attached()
    {
        return this._attached;
    },

    set attached(x)
    {
        if (this._attached === x)
            return;

        this._attached = x;

        if (x)
            document.body.removeStyleClass("detached");
        else
            document.body.addStyleClass("detached");

        this._setCompactMode(x && !WebInspector.settings.dockToRight.get());
        this._updateDockButtonState();
    },

    isCompactMode: function()
    {
        return this.attached && !WebInspector.settings.dockToRight.get();
    },

    _setCompactMode: function(x)
    {
        var body = document.body;
        if (x)
            body.addStyleClass("compact");
        else
            body.removeStyleClass("compact");

        // This may be called before doLoadedDone, hence the bulk of inspector objects may
        // not be created yet.
        if (WebInspector.toolbar)
            WebInspector.toolbar.compact = x;

        if (WebInspector.drawer)
            WebInspector.drawer.resize();
    },

    _updateErrorAndWarningCounts: function()
    {
        var errorWarningElement = document.getElementById("error-warning-count");
        if (!errorWarningElement)
            return;

        var errors = WebInspector.console.errors;
        var warnings = WebInspector.console.warnings;
        if (!errors && !warnings) {
            errorWarningElement.addStyleClass("hidden");
            return;
        }

        errorWarningElement.removeStyleClass("hidden");

        errorWarningElement.removeChildren();

        if (errors) {
            var errorImageElement = document.createElement("img");
            errorImageElement.id = "error-count-img";
            errorWarningElement.appendChild(errorImageElement);
            var errorElement = document.createElement("span");
            errorElement.id = "error-count";
            errorElement.textContent = errors;
            errorWarningElement.appendChild(errorElement);
        }

        if (warnings) {
            var warningsImageElement = document.createElement("img");
            warningsImageElement.id = "warning-count-img";
            errorWarningElement.appendChild(warningsImageElement);
            var warningsElement = document.createElement("span");
            warningsElement.id = "warning-count";
            warningsElement.textContent = warnings;
            errorWarningElement.appendChild(warningsElement);
        }

        if (errors) {
            if (warnings) {
                if (errors == 1) {
                    if (warnings == 1)
                        errorWarningElement.title = WebInspector.UIString("%d error, %d warning", errors, warnings);
                    else
                        errorWarningElement.title = WebInspector.UIString("%d error, %d warnings", errors, warnings);
                } else if (warnings == 1)
                    errorWarningElement.title = WebInspector.UIString("%d errors, %d warning", errors, warnings);
                else
                    errorWarningElement.title = WebInspector.UIString("%d errors, %d warnings", errors, warnings);
            } else if (errors == 1)
                errorWarningElement.title = WebInspector.UIString("%d error", errors);
            else
                errorWarningElement.title = WebInspector.UIString("%d errors", errors);
        } else if (warnings == 1)
            errorWarningElement.title = WebInspector.UIString("%d warning", warnings);
        else if (warnings)
            errorWarningElement.title = WebInspector.UIString("%d warnings", warnings);
        else
            errorWarningElement.title = null;
    },

    /**
     * @param {NetworkAgent.RequestId} requestId
     * @return {?WebInspector.NetworkRequest}
     */
    networkRequestById: function(requestId)
    {
        return this.panels.network.requestById(requestId);
    },

    get inspectedPageDomain()
    {
        var parsedURL = WebInspector.inspectedPageURL && WebInspector.inspectedPageURL.asParsedURL();
        return parsedURL ? parsedURL.host : "";
    },

    _initializeCapability: function(name, callback, error, result)
    {
        Capabilities[name] = result;
        if (callback)
            callback();
    },

    _zoomIn: function()
    {
        ++this._zoomLevel;
        this._requestZoom();
    },

    _zoomOut: function()
    {
        --this._zoomLevel;
        this._requestZoom();
    },

    _resetZoom: function()
    {
        this._zoomLevel = 0;
        this._requestZoom();
    },

    _requestZoom: function()
    {
        WebInspector.settings.zoomLevel.set(this._zoomLevel);
        InspectorFrontendHost.setZoomFactor(Math.pow(1.2, this._zoomLevel));
    },

    _toggleSearchingForNode: function()
    {
        var enabled = !this._nodeSearchButton.toggled;
        /**
         * @param {?Protocol.Error} error
         */
        function callback(error)
        {
            if (!error)
                this._nodeSearchButton.toggled = enabled;
        }
        WebInspector.domAgent.setInspectModeEnabled(enabled, callback.bind(this));
    }
}

WebInspector.Events = {
    InspectorClosing: "InspectorClosing"
}

{(function parseQueryParameters()
{
    WebInspector.queryParamsObject = {};
    var queryParams = window.location.search;
    if (!queryParams)
        return;
    var params = queryParams.substring(1).split("&");
    for (var i = 0; i < params.length; ++i) {
        var pair = params[i].split("=");
        WebInspector.queryParamsObject[pair[0]] = pair[1];
    }
})();}

WebInspector.loaded = function()
{
    InspectorBackend.loadFromJSONIfNeeded();

    if (WebInspector.WorkerManager.isDedicatedWorkerFrontend()) {
        // Do not create socket for the worker front-end.
        WebInspector.doLoadedDone();
        return;
    }

    var ws;
    if ("ws" in WebInspector.queryParamsObject)
        ws = "ws://" + WebInspector.queryParamsObject.ws;
    else if ("page" in WebInspector.queryParamsObject) {
        var page = WebInspector.queryParamsObject.page;
        var host = "host" in WebInspector.queryParamsObject ? WebInspector.queryParamsObject.host : window.location.host;
        ws = "ws://" + host + "/devtools/page/" + page;
    }

    if (ws) {
        WebInspector.socket = new WebSocket(ws);
        WebInspector.socket.onmessage = function(message) { InspectorBackend.dispatch(message.data); }
        WebInspector.socket.onerror = function(error) { console.error(error); }
        WebInspector.socket.onopen = function() {
            InspectorFrontendHost.sendMessageToBackend = WebInspector.socket.send.bind(WebInspector.socket);
            WebInspector.doLoadedDone();
        }
        return;
    }
    WebInspector.doLoadedDone();
}

WebInspector.doLoadedDone = function()
{
    // Install styles and themes
    WebInspector.installPortStyles();
    if (WebInspector.socket)
        document.body.addStyleClass("remote");

    if (WebInspector.queryParamsObject.toolbarColor && WebInspector.queryParamsObject.textColor)
        WebInspector.setToolbarColors(WebInspector.queryParamsObject.toolbarColor, WebInspector.queryParamsObject.textColor);

    InspectorFrontendHost.loaded();
    WebInspector.WorkerManager.loaded();

    DebuggerAgent.causesRecompilation(WebInspector._initializeCapability.bind(WebInspector, "debuggerCausesRecompilation", null));
    DebuggerAgent.supportsSeparateScriptCompilationAndExecution(WebInspector._initializeCapability.bind(WebInspector, "separateScriptCompilationAndExecutionEnabled", null));
    ProfilerAgent.causesRecompilation(WebInspector._initializeCapability.bind(WebInspector, "profilerCausesRecompilation", null));
    ProfilerAgent.isSampling(WebInspector._initializeCapability.bind(WebInspector, "samplingCPUProfiler", null));
    ProfilerAgent.hasHeapProfiler(WebInspector._initializeCapability.bind(WebInspector, "heapProfilerPresent", null));
    TimelineAgent.supportsFrameInstrumentation(WebInspector._initializeCapability.bind(WebInspector, "timelineSupportsFrameInstrumentation", null));
    PageAgent.canOverrideDeviceMetrics(WebInspector._initializeCapability.bind(WebInspector, "canOverrideDeviceMetrics", null));
    PageAgent.canOverrideGeolocation(WebInspector._initializeCapability.bind(WebInspector, "canOverrideGeolocation", null));
    PageAgent.canOverrideDeviceOrientation(WebInspector._initializeCapability.bind(WebInspector, "canOverrideDeviceOrientation", WebInspector._doLoadedDoneWithCapabilities.bind(WebInspector)));
    if ("debugLoad" in WebInspector.queryParamsObject)
        WebInspector._doLoadedDoneWithCapabilities();
}

WebInspector._doLoadedDoneWithCapabilities = function()
{
    WebInspector.shortcutsScreen = new WebInspector.ShortcutsScreen();
    this._registerShortcuts();

    // set order of some sections explicitly
    WebInspector.shortcutsScreen.section(WebInspector.UIString("Console"));
    WebInspector.shortcutsScreen.section(WebInspector.UIString("Elements Panel"));

    this.console = new WebInspector.ConsoleModel();
    this.console.addEventListener(WebInspector.ConsoleModel.Events.ConsoleCleared, this._updateErrorAndWarningCounts, this);
    this.console.addEventListener(WebInspector.ConsoleModel.Events.MessageAdded, this._updateErrorAndWarningCounts, this);
    this.console.addEventListener(WebInspector.ConsoleModel.Events.RepeatCountUpdated, this._updateErrorAndWarningCounts, this);

    this.debuggerModel = new WebInspector.DebuggerModel();

    WebInspector.CSSCompletions.requestCSSNameCompletions();

    this.drawer = new WebInspector.Drawer();
    this.consoleView = new WebInspector.ConsoleView(WebInspector.WorkerManager.isWorkerFrontend());

    this.networkManager = new WebInspector.NetworkManager();
    this.resourceTreeModel = new WebInspector.ResourceTreeModel(this.networkManager);
    this.networkLog = new WebInspector.NetworkLog();
    this.domAgent = new WebInspector.DOMAgent();
    this.javaScriptContextManager = new WebInspector.JavaScriptContextManager(this.resourceTreeModel, this.consoleView);

    this.scriptSnippetModel = new WebInspector.ScriptSnippetModel();

    InspectorBackend.registerInspectorDispatcher(this);

    this.cssModel = new WebInspector.CSSStyleModel();
    this.timelineManager = new WebInspector.TimelineManager();
    this.userAgentSupport = new WebInspector.UserAgentSupport();
    InspectorBackend.registerDatabaseDispatcher(new WebInspector.DatabaseDispatcher());
    InspectorBackend.registerDOMStorageDispatcher(new WebInspector.DOMStorageDispatcher());

    this.searchController = new WebInspector.SearchController();
    this.advancedSearchController = new WebInspector.AdvancedSearchController();
    this.settingsController = new WebInspector.SettingsController();

    this.domBreakpointsSidebarPane = new WebInspector.DOMBreakpointsSidebarPane();

    this._zoomLevel = WebInspector.settings.zoomLevel.get();
    if (this._zoomLevel)
        this._requestZoom();

    var autoselectPanel = WebInspector.UIString("a panel chosen automatically");
    var openAnchorLocationSetting = WebInspector.settings.createSetting("openLinkHandler", autoselectPanel);
    this.openAnchorLocationRegistry = new WebInspector.HandlerRegistry(openAnchorLocationSetting);
    this.openAnchorLocationRegistry.registerHandler(autoselectPanel, function() { return false; });

    this.workspace = new WebInspector.Workspace();
    this.breakpointManager = new WebInspector.BreakpointManager(WebInspector.settings.breakpoints, this.debuggerModel, this.workspace);

    this._createGlobalStatusBarItems();

    this.toolbar = new WebInspector.Toolbar();
    WebInspector._installDockToRight();

    var panelDescriptors = this._panelDescriptors();
    for (var i = 0; i < panelDescriptors.length; ++i)
        WebInspector.inspectorView.addPanel(panelDescriptors[i]);

    this.addMainEventListeners(document);

    window.addEventListener("resize", this.windowResize.bind(this), true);

    var errorWarningCount = document.getElementById("error-warning-count");
    errorWarningCount.addEventListener("click", this.showConsole.bind(this), false);
    this._updateErrorAndWarningCounts();

    this.extensionServer.initExtensions();

    this.console.enableAgent();

    function showInitialPanel()
    {
        if (!WebInspector.inspectorView.currentPanel())
            WebInspector.showPanel(WebInspector.settings.lastActivePanel.get());
    }

    InspectorAgent.enable(showInitialPanel);
    DatabaseAgent.enable();
    DOMStorageAgent.enable();

    if (WebInspector.settings.showPaintRects.get())
        PageAgent.setShowPaintRects(true);

    if (WebInspector.settings.javaScriptDisabled.get())
        PageAgent.setScriptExecutionDisabled(true);

    this.domAgent._emulateTouchEventsChanged();

    WebInspector.WorkerManager.loadCompleted();
    InspectorFrontendAPI.loadCompleted();
}

WebInspector._installDockToRight = function()
{
    // Re-use Settings infrastructure for the dock-to-right settings UI
    WebInspector.settings.dockToRight.set(WebInspector.queryParamsObject.dockSide === "right");

    if (WebInspector.settings.dockToRight.get())
        document.body.addStyleClass("dock-to-right");

    if (WebInspector.attached)
        WebInspector._setCompactMode(!WebInspector.settings.dockToRight.get());

    WebInspector.settings.dockToRight.addChangeListener(listener.bind(this));

    function listener(event)
    {
        var value = WebInspector.settings.dockToRight.get();
        if (value) {
            InspectorFrontendHost.requestSetDockSide("right");
            document.body.addStyleClass("dock-to-right");
        } else {
            InspectorFrontendHost.requestSetDockSide("bottom");
            document.body.removeStyleClass("dock-to-right");
        }
        if (WebInspector.attached)
            WebInspector._setCompactMode(!value);
        else
            WebInspector._updateDockButtonState();
    }
}

var windowLoaded = function()
{
    var localizedStringsURL = InspectorFrontendHost.localizedStringsURL();
    if (localizedStringsURL) {
        var localizedStringsScriptElement = document.createElement("script");
        localizedStringsScriptElement.addEventListener("load", WebInspector.loaded.bind(WebInspector), false);
        localizedStringsScriptElement.type = "text/javascript";
        localizedStringsScriptElement.src = localizedStringsURL;
        document.head.appendChild(localizedStringsScriptElement);
    } else
        WebInspector.loaded();

    WebInspector.attached = WebInspector.queryParamsObject.docked === "true";

    window.removeEventListener("DOMContentLoaded", windowLoaded, false);
    delete windowLoaded;
};

window.addEventListener("DOMContentLoaded", windowLoaded, false);

// We'd like to enforce asynchronous interaction between the inspector controller and the frontend.
// It is needed to prevent re-entering the backend code.
// Also, native dispatches do not guarantee setTimeouts to be serialized, so we
// enforce serialization using 'messagesToDispatch' queue. It is also important that JSC debugger
// tests require that each command was dispatch within individual timeout callback, so we don't batch them.

var messagesToDispatch = [];

WebInspector.dispatchQueueIsEmpty = function() {
    return messagesToDispatch.length == 0;
}

WebInspector.dispatch = function(message) {
    messagesToDispatch.push(message);
    setTimeout(function() {
        InspectorBackend.dispatch(messagesToDispatch.shift());
    }, 0);
}

WebInspector.dispatchMessageFromBackend = function(messageObject)
{
    WebInspector.dispatch(messageObject);
}

WebInspector.windowResize = function(event)
{
    WebInspector.inspectorView.doResize();
    WebInspector.drawer.resize();
    WebInspector.toolbar.resize();
    WebInspector.settingsController.resize();
}

WebInspector.setDockingUnavailable = function(unavailable)
{
    this._isDockingUnavailable = unavailable;
    this._updateDockButtonState();
}

WebInspector.close = function(event)
{
    if (this._isClosing)
        return;
    this._isClosing = true;
    this.notifications.dispatchEventToListeners(WebInspector.Events.InspectorClosing);
    InspectorFrontendHost.closeWindow();
}

WebInspector.documentClick = function(event)
{
    var anchor = event.target.enclosingNodeOrSelfWithNodeName("a");
    if (!anchor || anchor.target === "_blank")
        return;

    // Prevent the link from navigating, since we don't do any navigation by following links normally.
    event.consume(true);

    function followLink()
    {
        if (WebInspector.isBeingEdited(event.target) || WebInspector._showAnchorLocation(anchor))
            return;

        const profileMatch = WebInspector.ProfileType.URLRegExp.exec(anchor.href);
        if (profileMatch) {
            WebInspector.showProfileForURL(anchor.href);
            return;
        }

        var parsedURL = anchor.href.asParsedURL();
        if (parsedURL && parsedURL.scheme === "webkit-link-action") {
            if (parsedURL.host === "show-panel") {
                var panel = parsedURL.path.substring(1);
                if (WebInspector.panel(panel))
                    WebInspector.showPanel(panel);
            }
            return;
        }

        InspectorFrontendHost.openInNewTab(anchor.href);
    }

    if (WebInspector.followLinkTimeout)
        clearTimeout(WebInspector.followLinkTimeout);

    if (anchor.preventFollowOnDoubleClick) {
        // Start a timeout if this is the first click, if the timeout is canceled
        // before it fires, then a double clicked happened or another link was clicked.
        if (event.detail === 1)
            WebInspector.followLinkTimeout = setTimeout(followLink, 333);
        return;
    }

    followLink();
}

WebInspector.openResource = function(resourceURL, inResourcesPanel)
{
    var resource = WebInspector.resourceForURL(resourceURL);
    if (inResourcesPanel && resource)
        WebInspector.showPanel("resources").showResource(resource);
    else
        InspectorFrontendHost.openInNewTab(resourceURL);
}

WebInspector._registerShortcuts = function()
{
    var shortcut = WebInspector.KeyboardShortcut;
    var section = WebInspector.shortcutsScreen.section(WebInspector.UIString("All Panels"));
    var keys = [
        shortcut.shortcutToString("]", shortcut.Modifiers.CtrlOrMeta),
        shortcut.shortcutToString("[", shortcut.Modifiers.CtrlOrMeta)
    ];
    section.addRelatedKeys(keys, WebInspector.UIString("Go to the panel to the left/right"));

    var keys = [
        shortcut.shortcutToString("[", shortcut.Modifiers.CtrlOrMeta | shortcut.Modifiers.Alt),
        shortcut.shortcutToString("]", shortcut.Modifiers.CtrlOrMeta | shortcut.Modifiers.Alt)
    ];
    section.addRelatedKeys(keys, WebInspector.UIString("Go back/forward in panel history"));

    section.addKey(shortcut.shortcutToString(shortcut.Keys.Esc), WebInspector.UIString("Toggle console"));
    section.addKey(shortcut.shortcutToString("f", shortcut.Modifiers.CtrlOrMeta), WebInspector.UIString("Search"));

    var advancedSearchShortcut = WebInspector.AdvancedSearchController.createShortcut();
    section.addKey(advancedSearchShortcut.name, WebInspector.UIString("Search across all sources"));

    var openResourceShortcut = WebInspector.OpenResourceDialog.createShortcut();
    section.addKey(openResourceShortcut.name, WebInspector.UIString("Go to source"));

    if (WebInspector.isMac()) {
        keys = [
            shortcut.shortcutToString("g", shortcut.Modifiers.Meta),
            shortcut.shortcutToString("g", shortcut.Modifiers.Meta | shortcut.Modifiers.Shift)
        ];
        section.addRelatedKeys(keys, WebInspector.UIString("Find next/previous"));
    }

    var goToShortcut = WebInspector.GoToLineDialog.createShortcut();
    section.addKey(goToShortcut.name, WebInspector.UIString("Go to line"));
}

WebInspector.documentKeyDown = function(event)
{
    const helpKey = WebInspector.isMac() ? "U+003F" : "U+00BF"; // "?" for both platforms

    if (event.keyIdentifier === "F1" ||
        (event.keyIdentifier === helpKey && event.shiftKey && (!WebInspector.isBeingEdited(event.target) || event.metaKey))) {
        this.settingsController.showSettingsScreen(WebInspector.SettingsScreen.Tabs.Shortcuts);
        event.consume(true);
        return;
    }

    if (WebInspector.currentFocusElement() && WebInspector.currentFocusElement().handleKeyEvent) {
        WebInspector.currentFocusElement().handleKeyEvent(event);
        if (event.handled) {
            event.consume(true);
            return;
        }
    }

    if (WebInspector.inspectorView.currentPanel()) {
        WebInspector.inspectorView.currentPanel().handleShortcut(event);
        if (event.handled) {
            event.consume(true);
            return;
        }
    }

    if (WebInspector.searchController.handleShortcut(event))
        return;
    if (WebInspector.advancedSearchController.handleShortcut(event))
        return;

    switch (event.keyIdentifier) {
        case "U+004F": // O key
            if (!event.shiftKey && !event.altKey && WebInspector.KeyboardShortcut.eventHasCtrlOrMeta(event)) {
                WebInspector.panels.scripts.showGoToSourceDialog();
                event.consume(true);
            }
            break;
        case "U+0052": // R key
            if (WebInspector.KeyboardShortcut.eventHasCtrlOrMeta(event)) {
                PageAgent.reload(event.shiftKey);
                event.consume(true);
            }
            break;
        case "F5":
            if (!WebInspector.isMac()) {
                PageAgent.reload(event.ctrlKey || event.shiftKey);
                event.consume(true);
            }
            break;
    }

    var isValidZoomShortcut = WebInspector.KeyboardShortcut.eventHasCtrlOrMeta(event) &&
        !event.altKey &&
        !InspectorFrontendHost.isStub;
    switch (event.keyCode) {
        case 107: // +
        case 187: // +
            if (isValidZoomShortcut) {
                WebInspector._zoomIn();
                event.consume(true);
            }
            break;
        case 109: // -
        case 189: // -
            if (isValidZoomShortcut) {
                WebInspector._zoomOut();
                event.consume(true);
            }
            break;
        case 48: // 0
            // Zoom reset shortcut does not allow "Shift" when handled by the browser.
            if (isValidZoomShortcut && !event.shiftKey) {
                WebInspector._resetZoom();
                event.consume(true);
            }
            break;
    }

    // Cmd/Control + Shift + C should be a shortcut to clicking the Node Search Button.
    // This shortcut matches Firebug.
    if (event.keyIdentifier === "U+0043") { // C key
        if (WebInspector.isMac())
            var isNodeSearchKey = event.metaKey && !event.ctrlKey && !event.altKey && event.shiftKey;
        else
            var isNodeSearchKey = event.ctrlKey && !event.metaKey && !event.altKey && event.shiftKey;

        if (isNodeSearchKey) {
            this._toggleSearchingForNode();
            event.consume(true);
            return;
        }
        return;
    }
}

WebInspector.postDocumentKeyDown = function(event)
{
    if (event.handled)
        return;

    if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Esc.code) {
        // If drawer is open with some view other than console then close it.
        if (!this._toggleConsoleButton.toggled && WebInspector.drawer.visible)
            this.closeViewInDrawer();
        else
            this._toggleConsoleButtonClicked();
    }
}

WebInspector.documentCanCopy = function(event)
{
    if (WebInspector.inspectorView.currentPanel() && WebInspector.inspectorView.currentPanel().handleCopyEvent)
        event.preventDefault();
}

WebInspector.documentCopy = function(event)
{
    if (WebInspector.inspectorView.currentPanel() && WebInspector.inspectorView.currentPanel().handleCopyEvent)
        WebInspector.inspectorView.currentPanel().handleCopyEvent(event);
    WebInspector.documentCopyEventFired(event);
}

WebInspector.documentCopyEventFired = function(event)
{
}

WebInspector.contextMenuEventFired = function(event)
{
    if (event.handled || event.target.hasStyleClass("popup-glasspane"))
        event.preventDefault();
}

WebInspector.showConsole = function()
{
    if (WebInspector._toggleConsoleButton && !WebInspector._toggleConsoleButton.toggled) {
        if (WebInspector.drawer.visible)
            this._closePreviousDrawerView();
        WebInspector._toggleConsoleButtonClicked();
    }
}

WebInspector.showPanel = function(panel)
{
    return WebInspector.inspectorView.showPanel(panel);
}

WebInspector.panel = function(panel)
{
    return WebInspector.inspectorView.panel(panel);
}

WebInspector.bringToFront = function()
{
    InspectorFrontendHost.bringToFront();
}

/**
 * @param {string=} messageLevel
 * @param {boolean=} showConsole
 */
WebInspector.log = function(message, messageLevel, showConsole)
{
    // remember 'this' for setInterval() callback
    var self = this;

    // return indication if we can actually log a message
    function isLogAvailable()
    {
        return WebInspector.ConsoleMessage && WebInspector.RemoteObject && self.console;
    }

    // flush the queue of pending messages
    function flushQueue()
    {
        var queued = WebInspector.log.queued;
        if (!queued)
            return;

        for (var i = 0; i < queued.length; ++i)
            logMessage(queued[i]);

        delete WebInspector.log.queued;
    }

    // flush the queue if it console is available
    // - this function is run on an interval
    function flushQueueIfAvailable()
    {
        if (!isLogAvailable())
            return;

        clearInterval(WebInspector.log.interval);
        delete WebInspector.log.interval;

        flushQueue();
    }

    // actually log the message
    function logMessage(message)
    {
        // post the message
        var msg = WebInspector.ConsoleMessage.create(
            WebInspector.ConsoleMessage.MessageSource.Other,
            messageLevel || WebInspector.ConsoleMessage.MessageLevel.Debug,
            message);

        self.console.addMessage(msg);
        if (showConsole)
            WebInspector.showConsole();
    }

    // if we can't log the message, queue it
    if (!isLogAvailable()) {
        if (!WebInspector.log.queued)
            WebInspector.log.queued = [];

        WebInspector.log.queued.push(message);

        if (!WebInspector.log.interval)
            WebInspector.log.interval = setInterval(flushQueueIfAvailable, 1000);

        return;
    }

    // flush the pending queue if any
    flushQueue();

    // log the message
    logMessage(message);
}

WebInspector.inspect = function(payload, hints)
{
    var object = WebInspector.RemoteObject.fromPayload(payload);
    if (object.subtype === "node") {
        function callback(nodeId)
        {
            WebInspector._updateFocusedNode(nodeId);
            object.release();
        }
        object.pushNodeToFrontend(callback);
        return;
    }

    if (hints.databaseId)
        WebInspector.showPanel("resources").selectDatabase(hints.databaseId);
    else if (hints.domStorageId)
        WebInspector.showPanel("resources").selectDOMStorage(hints.domStorageId);

    object.release();
}

WebInspector._updateFocusedNode = function(nodeId)
{
    if (WebInspector._nodeSearchButton.toggled) {
        InspectorFrontendHost.bringToFront();
        WebInspector._nodeSearchButton.toggled = false;
    }
    WebInspector.showPanel("elements").revealAndSelectNode(nodeId);
}

WebInspector._showAnchorLocation = function(anchor)
{
    if (WebInspector.openAnchorLocationRegistry.dispatch({ url: anchor.href, lineNumber: anchor.lineNumber}))
        return true;
    var preferredPanel = this.panels[anchor.preferredPanel];
    if (preferredPanel && WebInspector._showAnchorLocationInPanel(anchor, preferredPanel))
        return true;
    if (WebInspector._showAnchorLocationInPanel(anchor, this.panels.scripts))
        return true;
    if (WebInspector._showAnchorLocationInPanel(anchor, this.panel("resources")))
        return true;
    if (WebInspector._showAnchorLocationInPanel(anchor, this.panels.network))
        return true;
    return false;
}

WebInspector._showAnchorLocationInPanel = function(anchor, panel)
{
    if (!panel || !panel.canShowAnchorLocation(anchor))
        return false;

    // FIXME: support webkit-html-external-link links here.
    if (anchor.hasStyleClass("webkit-html-external-link")) {
        anchor.removeStyleClass("webkit-html-external-link");
        anchor.addStyleClass("webkit-html-resource-link");
    }

    this.showPanelForAnchorNavigation(panel);
    panel.showAnchorLocation(anchor);
    return true;
}

WebInspector.showPanelForAnchorNavigation = function(panel)
{
    WebInspector.searchController.disableSearchUntilExplicitAction();
    WebInspector.inspectorView.setCurrentPanel(panel);
}

WebInspector.showProfileForURL = function(url)
{
    WebInspector.showPanel("profiles").showProfileForURL(url);
}

WebInspector.evaluateInConsole = function(expression, showResultOnly)
{
    this.showConsole();
    this.consoleView.evaluateUsingTextPrompt(expression, showResultOnly);
}

WebInspector.addMainEventListeners = function(doc)
{
    doc.addEventListener("keydown", this.documentKeyDown.bind(this), true);
    doc.addEventListener("keydown", this.postDocumentKeyDown.bind(this), false);
    doc.addEventListener("beforecopy", this.documentCanCopy.bind(this), true);
    doc.addEventListener("copy", this.documentCopy.bind(this), true);
    doc.addEventListener("contextmenu", this.contextMenuEventFired.bind(this), true);
    doc.addEventListener("click", this.documentClick.bind(this), true);
}

WebInspector.frontendReused = function()
{
    this.resourceTreeModel.frontendReused();
}
