/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
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

var Preferences = {
    ignoreWhitespace: true,
    showUserAgentStyles: true,
    maxInlineTextChildLength: 80,
    maxTextSearchResultLength: 80,
    minConsoleHeight: 75,
    minSidebarWidth: 100,
    minElementsSidebarWidth: 200,
    minScriptsSidebarWidth: 200,
    showInheritedComputedStyleProperties: false,
    showMissingLocalizedStrings: false
}

var WebInspector = {
    resources: [],
    resourceURLMap: {},
    searchResultsHeight: 100,
    missingLocalizedStrings: {},

    get previousFocusElement()
    {
        return this._previousFocusElement;
    },

    get currentFocusElement()
    {
        return this._currentFocusElement;
    },

    set currentFocusElement(x)
    {
        if (!x || this._currentFocusElement === x)
            return;

        this._previousFocusElement = this._currentFocusElement;
        this._currentFocusElement = x;

        if (this._currentFocusElement) {
            this._currentFocusElement.focus();

            // Make a caret selection inside the new element if there isn't a range selection and
            // there isn't already a caret selection inside.
            var selection = window.getSelection();
            if (selection.isCollapsed && !this._currentFocusElement.isInsertionCaretInside()) {
                var selectionRange = document.createRange();
                selectionRange.setStart(this._currentFocusElement, 0);
                selectionRange.setEnd(this._currentFocusElement, 0);

                selection.removeAllRanges();
                selection.addRange(selectionRange);
            }
        } else if (this._previousFocusElement)
            this._previousFocusElement.blur();
    },

    get currentPanel()
    {
        return this._currentPanel;
    },

    set currentPanel(x)
    {
        if (this._currentPanel === x)
            return;

        if (this._currentPanel)
            this._currentPanel.hide();

        this._currentPanel = x;

        if (x)
            x.show();
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

        var dockToggleButton = document.getElementById("dock-status-bar-item");
        var body = document.body;

        if (x) {
            InspectorController.attach();
            body.removeStyleClass("detached");
            body.addStyleClass("attached");
            dockToggleButton.title = WebInspector.UIString("Undock into separate window.");
        } else {
            InspectorController.detach();
            body.removeStyleClass("attached");
            body.addStyleClass("detached");
            dockToggleButton.title = WebInspector.UIString("Dock to main window.");
        }
    },

    get showingSearchResults()
    {
        return this._showingSearchResults;
    },

    set showingSearchResults(x)
    {
        if (this._showingSearchResults === x)
            return;

        this._showingSearchResults = x;

        var resultsContainer = document.getElementById("searchResults");
        var searchResultsResizer = document.getElementById("searchResultsResizer");

        if (x) {
            resultsContainer.removeStyleClass("hidden");
            searchResultsResizer.removeStyleClass("hidden");

            var animations = [
                {element: resultsContainer, end: {top: 0}},
                {element: searchResultsResizer, end: {top: WebInspector.searchResultsHeight - 3}},
                {element: document.getElementById("main-panels"), end: {top: WebInspector.searchResultsHeight}}
            ];

            WebInspector.animateStyle(animations, 250);
        } else {
            searchResultsResizer.addStyleClass("hidden");

            var animations = [
                {element: resultsContainer, end: {top: -WebInspector.searchResultsHeight}},
                {element: searchResultsResizer, end: {top: 0}},
                {element: document.getElementById("main-panels"), end: {top: 0}}
            ];

            var animationFinished = function()
            {
                resultsContainer.addStyleClass("hidden");
                resultsContainer.removeChildren();
                delete this.searchResultsTree;
            };

            WebInspector.animateStyle(animations, 250, animationFinished);
        }
    },

    get errors()
    {
        return this._errors || 0;
    },

    set errors(x)
    {
        x = Math.max(x, 0);

        if (this._errors === x)
            return;
        this._errors = x;
        this._updateErrorAndWarningCounts();
    },

    get warnings()
    {
        return this._warnings || 0;
    },

    set warnings(x)
    {
        x = Math.max(x, 0);

        if (this._warnings === x)
            return;
        this._warnings = x;
        this._updateErrorAndWarningCounts();
    },

    _updateErrorAndWarningCounts: function()
    {
        var errorWarningElement = document.getElementById("error-warning-count");
        if (!errorWarningElement)
            return;

        if (!this.errors && !this.warnings) {
            errorWarningElement.addStyleClass("hidden");
            return;
        }

        errorWarningElement.removeStyleClass("hidden");

        errorWarningElement.removeChildren();

        if (this.errors) {
            var errorElement = document.createElement("span");
            errorElement.id = "error-count";
            errorElement.textContent = this.errors;
            errorWarningElement.appendChild(errorElement);
        }

        if (this.warnings) {
            var warningsElement = document.createElement("span");
            warningsElement.id = "warning-count";
            warningsElement.textContent = this.warnings;
            errorWarningElement.appendChild(warningsElement);
        }

        if (this.errors) {
            if (this.warnings) {
                if (this.errors == 1) {
                    if (this.warnings == 1)
                        errorWarningElement.title = WebInspector.UIString("%d error, %d warning", this.errors, this.warnings);
                    else
                        errorWarningElement.title = WebInspector.UIString("%d error, %d warnings", this.errors, this.warnings);
                } else if (this.warnings == 1)
                    errorWarningElement.title = WebInspector.UIString("%d errors, %d warning", this.errors, this.warnings);
                else
                    errorWarningElement.title = WebInspector.UIString("%d errors, %d warnings", this.errors, this.warnings);
            } else if (this.errors == 1)
                errorWarningElement.title = WebInspector.UIString("%d error", this.errors);
            else
                errorWarningElement.title = WebInspector.UIString("%d errors", this.errors);
        } else if (this.warnings == 1)
            errorWarningElement.title = WebInspector.UIString("%d warning", this.warnings);
        else if (this.warnings)
            errorWarningElement.title = WebInspector.UIString("%d warnings", this.warnings);
        else
            errorWarningElement.title = null;
    }
}

WebInspector.loaded = function()
{
    var platform = InspectorController.platform();
    document.body.addStyleClass("platform-" + platform);

    this.console = new WebInspector.Console();
    this.panels = {
        elements: new WebInspector.ElementsPanel(),
        resources: new WebInspector.ResourcesPanel(),
        scripts: new WebInspector.ScriptsPanel(),
        profiles: new WebInspector.ProfilesPanel(),
        databases: new WebInspector.DatabasesPanel()
    };

    var toolbarElement = document.getElementById("toolbar");
    for (var panelName in this.panels) {
        var panel = this.panels[panelName];
        var panelToolbarItem = panel.toolbarItem;
        panelToolbarItem.addEventListener("click", this._toolbarItemClicked.bind(this));
        if (previousToolbarItem)
            toolbarElement.insertBefore(panelToolbarItem, previousToolbarItem.nextSibling);
        else
            toolbarElement.insertBefore(panelToolbarItem, toolbarElement.firstChild);
        var previousToolbarItem = panelToolbarItem;
    }

    this.currentPanel = this.panels.elements;

    this.resourceCategories = {
        documents: new WebInspector.ResourceCategory(WebInspector.UIString("Documents"), "documents"),
        stylesheets: new WebInspector.ResourceCategory(WebInspector.UIString("Stylesheets"), "stylesheets"),
        images: new WebInspector.ResourceCategory(WebInspector.UIString("Images"), "images"),
        scripts: new WebInspector.ResourceCategory(WebInspector.UIString("Scripts"), "scripts"),
        xhr: new WebInspector.ResourceCategory(WebInspector.UIString("XHR"), "xhr"),
        fonts: new WebInspector.ResourceCategory(WebInspector.UIString("Fonts"), "fonts"),
        other: new WebInspector.ResourceCategory(WebInspector.UIString("Other"), "other")
    };

    this.Tips = {
        ResourceNotCompressed: {id: 0, message: WebInspector.UIString("You could save bandwidth by having your web server compress this transfer with gzip or zlib.")}
    };

    this.Warnings = {
        IncorrectMIMEType: {id: 0, message: WebInspector.UIString("Resource interpreted as %s but transferred with MIME type %s.")}
    };

    this.addMainEventListeners(document);

    window.addEventListener("unload", this.windowUnload.bind(this), true);
    window.addEventListener("resize", this.windowResize.bind(this), true);

    document.addEventListener("focus", this.focusChanged.bind(this), true);
    document.addEventListener("keydown", this.documentKeyDown.bind(this), true);
    document.addEventListener("keyup", this.documentKeyUp.bind(this), true);
    document.addEventListener("beforecopy", this.documentCanCopy.bind(this), true);
    document.addEventListener("copy", this.documentCopy.bind(this), true);

    document.getElementById("searchResultsResizer").addEventListener("mousedown", this.searchResultsResizerDragStart, true);

    var mainPanelsElement = document.getElementById("main-panels");
    mainPanelsElement.handleKeyEvent = this.mainKeyDown.bind(this);
    mainPanelsElement.handleKeyUpEvent = this.mainKeyUp.bind(this);
    mainPanelsElement.handleCopyEvent = this.mainCopy.bind(this);

    // Focus the mainPanelsElement in a timeout so it happens after the initial focus,
    // so it doesn't get reset to the first toolbar button. This initial focus happens
    // on Mac when the window is made key and the WebHTMLView becomes the first responder.
    setTimeout(function() { WebInspector.currentFocusElement = mainPanelsElement }, 0);

    var dockToggleButton = document.getElementById("dock-status-bar-item");
    dockToggleButton.addEventListener("click", this.toggleAttach.bind(this), false);

    if (this.attached)
        dockToggleButton.title = WebInspector.UIString("Undock into separate window.");
    else
        dockToggleButton.title = WebInspector.UIString("Dock to main window.");

    var errorWarningCount = document.getElementById("error-warning-count");
    errorWarningCount.addEventListener("click", this.console.show.bind(this.console), false);
    this._updateErrorAndWarningCounts();

    document.getElementById("search-toolbar-label").textContent = WebInspector.UIString("Search");

    if (platform === "mac-leopard")
        document.getElementById("toolbar").addEventListener("mousedown", this.toolbarDragStart, true);

    InspectorController.loaded();
}

var windowLoaded = function()
{
    var localizedStringsURL = InspectorController.localizedStringsURL();
    if (localizedStringsURL) {
        var localizedStringsScriptElement = document.createElement("script");
        localizedStringsScriptElement.addEventListener("load", WebInspector.loaded.bind(WebInspector), false);
        localizedStringsScriptElement.type = "text/javascript";
        localizedStringsScriptElement.src = localizedStringsURL;
        document.getElementsByTagName("head").item(0).appendChild(localizedStringsScriptElement);
    } else
        WebInspector.loaded();

    window.removeEventListener("load", windowLoaded, false);
    delete windowLoaded;
};

window.addEventListener("load", windowLoaded, false);

WebInspector.windowUnload = function(event)
{
    InspectorController.windowUnloading();
}

WebInspector.windowResize = function(event)
{
    if (this.currentPanel && this.currentPanel.resize)
        this.currentPanel.resize();
}

WebInspector.windowFocused = function(event)
{
    if (event.target.nodeType === Node.DOCUMENT_NODE)
        document.body.removeStyleClass("inactive");
}

WebInspector.windowBlured = function(event)
{
    if (event.target.nodeType === Node.DOCUMENT_NODE)
        document.body.addStyleClass("inactive");
}

WebInspector.focusChanged = function(event)
{
    this.currentFocusElement = event.target;
}

WebInspector.documentClick = function(event)
{
    var anchor = event.target.enclosingNodeOrSelfWithNodeName("a");
    if (!anchor)
        return;

    if (anchor.followOnAltClick && !event.altKey) {
        event.preventDefault();
        return;
    }

    if (!anchor.hasStyleClass("webkit-html-resource-link"))
        return;

    if (WebInspector.showResourceForURL(anchor.href, anchor.lineNumber, anchor.preferredPanel)) {
        event.preventDefault();
        event.stopPropagation();
    }
}

WebInspector.documentKeyDown = function(event)
{
    if (!this.currentFocusElement)
        return;
    if (this.currentFocusElement.handleKeyEvent)
        this.currentFocusElement.handleKeyEvent(event);
    else if (this.currentFocusElement.id && this.currentFocusElement.id.length && WebInspector[this.currentFocusElement.id + "KeyDown"])
        WebInspector[this.currentFocusElement.id + "KeyDown"](event);

    if (!event.handled) {
        switch (event.keyIdentifier) {
            case "U+001B": // Escape key
                this.console.visible = !this.console.visible;
                event.preventDefault();
                break;
        }
    }
}

WebInspector.documentKeyUp = function(event)
{
    if (!this.currentFocusElement || !this.currentFocusElement.handleKeyUpEvent)
        return;
    this.currentFocusElement.handleKeyUpEvent(event);
}

WebInspector.documentCanCopy = function(event)
{
    if (!this.currentFocusElement)
        return;
    // Calling preventDefault() will say "we support copying, so enable the Copy menu".
    if (this.currentFocusElement.handleCopyEvent)
        event.preventDefault();
    else if (this.currentFocusElement.id && this.currentFocusElement.id.length && WebInspector[this.currentFocusElement.id + "Copy"])
        event.preventDefault();
}

WebInspector.documentCopy = function(event)
{
    if (!this.currentFocusElement)
        return;
    if (this.currentFocusElement.handleCopyEvent)
        this.currentFocusElement.handleCopyEvent(event);
    else if (this.currentFocusElement.id && this.currentFocusElement.id.length && WebInspector[this.currentFocusElement.id + "Copy"])
        WebInspector[this.currentFocusElement.id + "Copy"](event);
}

WebInspector.mainKeyDown = function(event)
{
    if (this.currentPanel && this.currentPanel.handleKeyEvent)
        this.currentPanel.handleKeyEvent(event);
}

WebInspector.mainKeyUp = function(event)
{
    if (this.currentPanel && this.currentPanel.handleKeyUpEvent)
        this.currentPanel.handleKeyUpEvent(event);
}

WebInspector.mainCopy = function(event)
{
    if (this.currentPanel && this.currentPanel.handleCopyEvent)
        this.currentPanel.handleCopyEvent(event);
}

WebInspector.searchResultsKeyDown = function(event)
{
    if (this.searchResultsTree)
        this.searchResultsTree.handleKeyEvent(event);
}

WebInspector.animateStyle = function(animations, duration, callback, complete)
{
    if (complete === undefined)
        complete = 0;
    var slice = (1000 / 30); // 30 frames per second

    var defaultUnit = "px";
    var propertyUnit = {opacity: ""};

    for (var i = 0; i < animations.length; ++i) {
        var animation = animations[i];
        var element = null;
        var start = null;
        var current = null;
        var end = null;
        for (key in animation) {
            if (key === "element")
                element = animation[key];
            else if (key === "start")
                start = animation[key];
            else if (key === "current")
                current = animation[key];
            else if (key === "end")
                end = animation[key];
        }

        if (!element || !end)
            continue;

        var computedStyle = element.ownerDocument.defaultView.getComputedStyle(element);
        if (!start) {
            start = {};
            for (key in end)
                start[key] = parseInt(computedStyle.getPropertyValue(key));
            animation.start = start;
        } else if (complete == 0)
            for (key in start)
                element.style.setProperty(key, start[key] + (key in propertyUnit ? propertyUnit[key] : defaultUnit));

        if (!current) {
            current = {};
            for (key in start)
                current[key] = start[key];
            animation.current = current;
        }

        function cubicInOut(t, b, c, d)
        {
            if ((t/=d/2) < 1) return c/2*t*t*t + b;
            return c/2*((t-=2)*t*t + 2) + b;
        }

        var style = element.style;
        for (key in end) {
            var startValue = start[key];
            var currentValue = current[key];
            var endValue = end[key];
            if ((complete + slice) < duration) {
                var delta = (endValue - startValue) / (duration / slice);
                var newValue = cubicInOut(complete, startValue, endValue - startValue, duration);
                style.setProperty(key, newValue + (key in propertyUnit ? propertyUnit[key] : defaultUnit));
                current[key] = newValue;
            } else {
                style.setProperty(key, endValue + (key in propertyUnit ? propertyUnit[key] : defaultUnit));
            }
        }
    }

    if (complete < duration)
        setTimeout(WebInspector.animateStyle, slice, animations, duration, callback, complete + slice);
    else if (callback)
        callback();
}

WebInspector.toggleAttach = function()
{
    this.attached = !this.attached;
}

WebInspector.toolbarDragStart = function(event)
{
    if (WebInspector.attached)
        return;

    var target = event.target;
    if (target.hasStyleClass("toolbar-item") && target.hasStyleClass("toggleable"))
        return;

    var toolbar = document.getElementById("toolbar");
    if (target !== toolbar && !target.hasStyleClass("toolbar-item"))
        return;

    toolbar.lastScreenX = event.screenX;
    toolbar.lastScreenY = event.screenY;

    document.addEventListener("mousemove", WebInspector.toolbarDrag, true);
    document.addEventListener("mouseup", WebInspector.toolbarDragEnd, true);
    document.body.style.cursor = "default";

    event.preventDefault();
}

WebInspector.toolbarDragEnd = function(event)
{
    var toolbar = document.getElementById("toolbar");
    delete toolbar.lastScreenX;
    delete toolbar.lastScreenY;

    document.removeEventListener("mousemove", WebInspector.toolbarDrag, true);
    document.removeEventListener("mouseup", WebInspector.toolbarDragEnd, true);
    document.body.style.removeProperty("cursor");

    event.preventDefault();
}

WebInspector.toolbarDrag = function(event)
{
    var toolbar = document.getElementById("toolbar");

    var x = event.screenX - toolbar.lastScreenX;
    var y = event.screenY - toolbar.lastScreenY;

    toolbar.lastScreenX = event.screenX;
    toolbar.lastScreenY = event.screenY;

    // We cannot call window.moveBy here because it restricts the movement of the window
    // at the edges.
    InspectorController.moveByUnrestricted(x, y);

    event.preventDefault();
}

WebInspector.searchResultsResizerDragStart = function(event)
{
    WebInspector.elementDragStart(document.getElementById("searchResults"), WebInspector.searchResultsResizerDrag, WebInspector.searchResultsResizerDragEnd, event, "row-resize");
}

WebInspector.searchResultsResizerDragEnd = function(event)
{
    WebInspector.elementDragEnd(event);
}

WebInspector.searchResultsResizerDrag = function(event)
{
    var y = event.pageY - document.getElementById("main").offsetTop;
    var newHeight = Number.constrain(y, 100, window.innerHeight - 100);

    WebInspector.searchResultsHeight = newHeight;

    document.getElementById("searchResults").style.height = WebInspector.searchResultsHeight + "px";
    document.getElementById("main-panels").style.top = newHeight + "px";
    document.getElementById("searchResultsResizer").style.top = (newHeight - 3) + "px";

    event.preventDefault();
}

WebInspector.elementDragStart = function(element, dividerDrag, elementDragEnd, event, cursor) 
{
    if (this._elementDraggingEventListener || this._elementEndDraggingEventListener)
        this.elementDragEnd(event);

    this._elementDraggingEventListener = dividerDrag;
    this._elementEndDraggingEventListener = elementDragEnd;

    document.addEventListener("mousemove", dividerDrag, true);
    document.addEventListener("mouseup", elementDragEnd, true);

    document.body.style.cursor = cursor;

    event.preventDefault();
}

WebInspector.elementDragEnd = function(event)
{
    document.removeEventListener("mousemove", this._elementDraggingEventListener, true);
    document.removeEventListener("mouseup", this._elementEndDraggingEventListener, true);

    document.body.style.removeProperty("cursor");

    delete this._elementDraggingEventListener;
    delete this._elementEndDraggingEventListener;

    event.preventDefault();
}

WebInspector.showConsole = function()
{
    this.console.show();
}

WebInspector.showElementsPanel = function()
{
    this.currentPanel = this.panels.elements;
}

WebInspector.showResourcesPanel = function()
{
    this.currentPanel = this.panels.resources;
}

WebInspector.showScriptsPanel = function()
{
    this.currentPanel = this.panels.scripts;
}

WebInspector.showProfilesPanel = function()
{
    this.currentPanel = this.panels.profiles;
}

WebInspector.showDatabasesPanel = function()
{
    this.currentPanel = this.panels.databases;
}

WebInspector.addResource = function(resource)
{
    this.resources.push(resource);
    this.resourceURLMap[resource.url] = resource;

    if (resource.mainResource) {
        this.mainResource = resource;
        this.panels.elements.reset();
    }

    this.panels.resources.addResource(resource);
}

WebInspector.removeResource = function(resource)
{
    resource.category.removeResource(resource);
    delete this.resourceURLMap[resource.url];

    var resourcesLength = this.resources.length;
    for (var i = 0; i < resourcesLength; ++i) {
        if (this.resources[i] === resource) {
            this.resources.splice(i, 1);
            break;
        }
    }

    this.panels.resources.removeResource(resource);
}

WebInspector.addDatabase = function(database)
{
    this.panels.databases.addDatabase(database);
}

WebInspector.debuggerAttached = function()
{
    this.panels.scripts.debuggerAttached();
}

WebInspector.debuggerDetached = function()
{
    this.panels.scripts.debuggerDetached();
}

WebInspector.parsedScriptSource = function(sourceID, sourceURL, source, startingLine)
{
    this.panels.scripts.addScript(sourceID, sourceURL, source, startingLine);
}

WebInspector.failedToParseScriptSource = function(sourceURL, source, startingLine, errorLine, errorMessage)
{
    this.panels.scripts.addScript(null, sourceURL, source, startingLine, errorLine, errorMessage);
}

WebInspector.pausedScript = function()
{
    this.panels.scripts.debuggerPaused();
}

WebInspector.populateInterface = function()
{
    for (var panelName in this.panels) {
        var panel = this.panels[panelName];
        if ("populateInterface" in panel)
            panel.populateInterface();
    }
}

WebInspector.reset = function()
{
    for (var panelName in this.panels) {
        var panel = this.panels[panelName];
        if ("reset" in panel)
            panel.reset();
    }

    for (var category in this.resourceCategories)
        this.resourceCategories[category].removeAllResources();

    this.resources = [];
    this.resourceURLMap = {};

    delete this.mainResource;

    this.console.clearMessages();
}

WebInspector.resourceURLChanged = function(resource, oldURL)
{
    delete this.resourceURLMap[oldURL];
    this.resourceURLMap[resource.url] = resource;
}

WebInspector.addMessageToConsole = function(msg)
{
    this.console.addMessage(msg);
}

WebInspector.addProfile = function(profile)
{
    this.panels.profiles.addProfile(profile);
}

WebInspector.drawLoadingPieChart = function(canvas, percent) {
    var g = canvas.getContext("2d");
    var darkColor = "rgb(122, 168, 218)";
    var lightColor = "rgb(228, 241, 251)";
    var cx = 8;
    var cy = 8;
    var r = 7;

    g.beginPath();
    g.arc(cx, cy, r, 0, Math.PI * 2, false); 
    g.closePath();

    g.lineWidth = 1;
    g.strokeStyle = darkColor;
    g.fillStyle = lightColor;
    g.fill();
    g.stroke();

    var startangle = -Math.PI / 2;
    var endangle = startangle + (percent * Math.PI * 2);

    g.beginPath();
    g.moveTo(cx, cy);
    g.arc(cx, cy, r, startangle, endangle, false); 
    g.closePath();

    g.fillStyle = darkColor;
    g.fill();
}

WebInspector.updateFocusedNode = function(node)
{
    if (!node)
        // FIXME: Should we deselect if null is passed in?
        return;

    this.currentPanel = this.panels.elements;
    this.panels.elements.focusedDOMNode = node;
}

WebInspector.displayNameForURL = function(url)
{
    var resource = this.resourceURLMap[url];
    if (resource)
        return resource.displayName;
    return url.trimURL(WebInspector.mainResource ? WebInspector.mainResource.domain : "");
}

WebInspector.resourceForURL = function(url)
{
    if (url in this.resourceURLMap)
        return this.resourceURLMap[url];

    // No direct match found. Search for resources that contain
    // a substring of the URL.
    for (var resourceURL in this.resourceURLMap) {
        if (resourceURL.hasSubstring(url))
            return this.resourceURLMap[resourceURL];
    }

    return null;
}

WebInspector.showResourceForURL = function(url, line, preferredPanel)
{
    var resource = this.resourceForURL(url);
    if (!resource)
        return false;

    if (preferredPanel && preferredPanel in WebInspector.panels) {
        var panel = this.panels[preferredPanel];
        if (!("showResource" in panel))
            panel = null;
        else if ("canShowResource" in panel && !panel.canShowResource(resource))
            panel = null;
    }

    this.currentPanel = panel || this.panels.resources;
    this.currentPanel.showResource(resource, line);
    return true;
}

WebInspector.linkifyURL = function(url, linkText, classes, isExternal)
{
    if (linkText === undefined)
        linkText = url.escapeHTML();
    classes = (classes === undefined) ? "" : classes + " ";
    classes += isExternal ? "webkit-html-external-link" : "webkit-html-resource-link";
    var link = "<a href=\"" + url + "\" class=\"" + classes + "\" title=\"" + url + "\" target=\"_blank\">" + linkText + "</a>";
    return link;
}

WebInspector.addMainEventListeners = function(doc)
{
    doc.defaultView.addEventListener("focus", this.windowFocused.bind(this), true);
    doc.defaultView.addEventListener("blur", this.windowBlured.bind(this), true);
    doc.addEventListener("click", this.documentClick.bind(this), true);
}

WebInspector.performSearch = function(query)
{
    if (!query || !query.length) {
        this.showingSearchResults = false;
        return;
    }

    var resultsContainer = document.getElementById("searchResults");
    resultsContainer.removeChildren();

    var isXPath = query.indexOf("/") !== -1;

    var xpathQuery;
    if (isXPath)
        xpathQuery = query;
    else {
        var escapedQuery = query.escapeCharacters("'");
        xpathQuery = "//*[contains(name(),'" + escapedQuery + "') or contains(@*,'" + escapedQuery + "')] | //text()[contains(.,'" + escapedQuery + "')] | //comment()[contains(.,'" + escapedQuery + "')]";
    }

    var resourcesToSearch = [].concat(this.resourceCategories.documents.resources, this.resourceCategories.stylesheets.resources, this.resourceCategories.scripts.resources, this.resourceCategories.other.resources);

    var files = [];
    for (var i = 0; i < resourcesToSearch.length; ++i) {
        var resource = resourcesToSearch[i];

        var sourceResults = [];
        if (!isXPath) {
            var sourceFrame = this.panels.resources.sourceFrameForResource(resource);
            if (sourceFrame)
                sourceResults = InspectorController.search(sourceFrame.element.contentDocument, query);
        }

        var domResults = [];
        const searchResultsProperty = "__includedInInspectorSearchResults";
        function addNodesToDOMResults(nodes, length, getItem)
        {
            for (var i = 0; i < length; ++i) {
                var node = getItem(nodes, i);
                if (searchResultsProperty in node)
                    continue;
                node[searchResultsProperty] = true;
                domResults.push(node);
            }
        }

        function cleanUpDOMResultsNodes()
        {
            for (var i = 0; i < domResults.length; ++i)
                delete domResults[i][searchResultsProperty];
        }

        if (resource.category === this.resourceCategories.documents) {
            var doc = resource.documentNode;
            try {
                var result = InspectorController.inspectedWindow().Document.prototype.evaluate.call(doc, xpathQuery, doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
                addNodesToDOMResults(result, result.snapshotLength, function(l, i) { return l.snapshotItem(i); });
            } catch(err) {
                // ignore any exceptions. the query might be malformed, but we allow that.
            }

            var result = InspectorController.inspectedWindow().Document.prototype.querySelectorAll.call(doc, query);
            addNodesToDOMResults(result, result.length, function(l, i) { return l.item(i); });

            cleanUpDOMResultsNodes();
        }

        if ((!sourceResults || !sourceResults.length) && !domResults.length)
            continue;

        files.push({resource: resource, sourceResults: sourceResults, domResults: domResults});
    }

    if (!files.length)
        return;

    this.showingSearchResults = true;

    var fileList = document.createElement("ol");
    fileList.className = "outline-disclosure";
    resultsContainer.appendChild(fileList);

    this.searchResultsTree = new TreeOutline(fileList);
    this.searchResultsTree.expandTreeElementsWhenArrowing = true;

    var sourceResultSelected = function(element)
    {
        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(element.representedObject.range);

        var oldFocusElement = this.currentFocusElement;
        this.currentPanel = this.panels.resources;
        this.currentFocusElement = oldFocusElement;

        this.panels.resources.showResource(element.representedObject.resource);

        element.representedObject.line.scrollIntoViewIfNeeded(true);
        element.listItemElement.scrollIntoViewIfNeeded(false);
    }

    var domResultSelected = function(element)
    {
        var oldFocusElement = this.currentFocusElement;
        this.currentPanel = this.panels.elements;
        this.currentFocusElement = oldFocusElement;

        this.panels.elements.focusedDOMNode = element.representedObject.node;
        element.listItemElement.scrollIntoViewIfNeeded(false);
    }

    for (var i = 0; i < files.length; ++i) {
        var file = files[i];

        var fileItem = new TreeElement(file.resource.displayName, {}, true);
        fileItem.expanded = true;
        fileItem.selectable = false;
        this.searchResultsTree.appendChild(fileItem);

        if (file.sourceResults && file.sourceResults.length) {
            for (var j = 0; j < file.sourceResults.length; ++j) {
                var range = file.sourceResults[j];
                var sourceDocument = range.startContainer.ownerDocument;

                var line = range.startContainer;
                while (line.parentNode && line.nodeName.toLowerCase() != "tr")
                    line = line.parentNode;
                var lineRange = sourceDocument.createRange();
                lineRange.selectNodeContents(line);

                // Don't include any error bubbles in the search result
                var end = line.lastChild.lastChild;
                if (end.nodeName.toLowerCase() == "div" && end.hasStyleClass("webkit-html-message-bubble")) {
                    while (end && end.nodeName.toLowerCase() == "div" && end.hasStyleClass("webkit-html-message-bubble"))
                        end = end.previousSibling;
                    lineRange.setEndAfter(end);
                }

                var beforeRange = sourceDocument.createRange();
                beforeRange.setStart(lineRange.startContainer, lineRange.startOffset);
                beforeRange.setEnd(range.startContainer, range.startOffset);

                var afterRange = sourceDocument.createRange();
                afterRange.setStart(range.endContainer, range.endOffset);
                afterRange.setEnd(lineRange.endContainer, lineRange.endOffset);

                var beforeText = beforeRange.toString().trimLeadingWhitespace();
                var text = range.toString();
                var afterText = afterRange.toString().trimTrailingWhitespace();

                var length = beforeText.length + text.length + afterText.length;
                if (length > Preferences.maxTextSearchResultLength) {
                    var beforeAfterLength = (Preferences.maxTextSearchResultLength - text.length) / 2;
                    if (beforeText.length > beforeAfterLength)
                        beforeText = "\u2026" + beforeText.substr(-beforeAfterLength);
                    if (afterText.length > beforeAfterLength)
                        afterText = afterText.substr(0, beforeAfterLength) + "\u2026";
                }

                var title = "<div class=\"selection selected\"></div>";
                if (j == 0)
                    title += "<div class=\"search-results-section\">" + WebInspector.UIString("Source") + "</div>";
                title += beforeText.escapeHTML() + "<span class=\"search-matched-string\">" + text.escapeHTML() + "</span>" + afterText.escapeHTML();
                var item = new TreeElement(title, {resource: file.resource, line: line, range: range}, false);
                item.onselect = sourceResultSelected.bind(this);
                fileItem.appendChild(item);
            }
        }

        if (file.domResults.length) {
            for (var j = 0; j < file.domResults.length; ++j) {
                var node = file.domResults[j];
                var title = "<div class=\"selection selected\"></div>";
                if (j == 0)
                    title += "<div class=\"search-results-section\">" + WebInspector.UIString("DOM") + "</div>";
                title += nodeTitleInfo.call(node).title;
                var item = new TreeElement(title, {resource: file.resource, node: node}, false);
                item.onselect = domResultSelected.bind(this);
                fileItem.appendChild(item);
            }
        }
    }
}

WebInspector.UIString = function(string)
{
    if (window.localizedStrings && string in window.localizedStrings)
        string = window.localizedStrings[string];
    else {
        if (!(string in this.missingLocalizedStrings)) {
            console.error("Localized string \"" + string + "\" not found.");
            this.missingLocalizedStrings[string] = true;
        }

        if (Preferences.showMissingLocalizedStrings)
            string += " (not localized)";
    }

    return String.vsprintf(string, Array.prototype.slice.call(arguments, 1));
}

WebInspector.isBeingEdited = function(element)
{
    return element.__editing;
}

WebInspector.startEditing = function(element, committedCallback, cancelledCallback, context)
{
    if (element.__editing)
        return;
    element.__editing = true;

    var oldText = element.textContent;
    var handleKeyEvent = element.handleKeyEvent;

    element.addStyleClass("editing");

    var oldTabIndex = element.tabIndex;
    if (element.tabIndex < 0)
        element.tabIndex = 0;

    function blurEventListener() {
        editingCancelled.call(element);
    }

    function cleanUpAfterEditing() {
        delete this.__editing;

        this.removeStyleClass("editing");
        this.tabIndex = oldTabIndex;

        this.handleKeyEvent = handleKeyEvent;
        element.removeEventListener("blur", blurEventListener, false);

        if (element === WebInspector.currentFocusElement || element.isAncestor(WebInspector.currentFocusElement))
            WebInspector.currentFocusElement = WebInspector.previousFocusElement;
    }

    function editingCancelled() {
        this.innerText = oldText;

        cleanUpAfterEditing.call(this);

        cancelledCallback(this, context);
    }

    function editingCommitted() {
        cleanUpAfterEditing.call(this);

        committedCallback(this, this.textContent, oldText, context);
    }

    element.handleKeyEvent = function(event) {
        if (event.keyIdentifier === "Enter") {
            editingCommitted.call(element);
            event.preventDefault();
        } else if (event.keyCode === 27) { // Escape key
            editingCancelled.call(element);
            event.preventDefault();
            event.handled = true;
        }
    }

    element.addEventListener("blur", blurEventListener, false);

    WebInspector.currentFocusElement = element;
}

WebInspector._toolbarItemClicked = function(event)
{
    var toolbarItem = event.currentTarget;
    this.currentPanel = toolbarItem.panel;
}

// This table maps MIME types to the Resource.Types which are valid for them.
// The following line:
//    "text/html":                {0: 1},
// means that text/html is a valid MIME type for resources that have type
// WebInspector.Resource.Type.Document (which has a value of 0).
WebInspector.MIMETypes = {
    "text/html":                   {0: true},
    "text/xml":                    {0: true},
    "text/plain":                  {0: true},
    "application/xhtml+xml":       {0: true},
    "text/css":                    {1: true},
    "text/xsl":                    {1: true},
    "image/jpeg":                  {2: true},
    "image/png":                   {2: true},
    "image/gif":                   {2: true},
    "image/bmp":                   {2: true},
    "image/x-icon":                {2: true},
    "image/x-xbitmap":             {2: true},
    "font/ttf":                    {3: true},
    "font/opentype":               {3: true},
    "application/x-font-type1":    {3: true},
    "application/x-font-ttf":      {3: true},
    "application/x-truetype-font": {3: true},
    "text/javascript":             {4: true},
    "text/ecmascript":             {4: true},
    "application/javascript":      {4: true},
    "application/ecmascript":      {4: true},
    "application/x-javascript":    {4: true},
    "text/javascript1.1":          {4: true},
    "text/javascript1.2":          {4: true},
    "text/javascript1.3":          {4: true},
    "text/jscript":                {4: true},
    "text/livescript":             {4: true},
}
