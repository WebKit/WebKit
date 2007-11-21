/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
    showInheritedComputedStyleProperties: false,
    showMissingLocalizedStrings: false,
    toolbarHeight: 28
}

var WebInspector = {
    resources: [],
    resourceURLMap: {},
    backForwardList: [],
    searchResultsHeight: 100,
    localizedStrings: {},
    missingLocalizedStrings: {},

    get consolePanel()
    {
        if (!this._consolePanel)
            this._consolePanel = new WebInspector.ConsolePanel();

        return this._consolePanel;
    },

    get networkPanel()
    {
        if (!this._networkPanel)
            this._networkPanel = new WebInspector.NetworkPanel();

        return this._networkPanel;
    },

    get currentBackForwardIndex()
    {
        if (this._currentBackForwardIndex === undefined)
            this._currentBackForwardIndex = -1;

        return this._currentBackForwardIndex;
    },

    set currentBackForwardIndex(x)
    {
        if (this._currentBackForwardIndex === x)
            return;

        this._currentBackForwardIndex = x;
        this.updateBackForwardButtons();
    },

    get currentFocusElement()
    {
        return this._currentFocusElement;
    },

    set currentFocusElement(x)
    {
        if (!x || this._currentFocusElement === x)
            return;

        if (this._currentFocusElement) {
            this._currentFocusElement.removeStyleClass("focused");
            this._currentFocusElement.addStyleClass("blurred");
            if (this._currentFocusElement.blurred)
                this._currentFocusElement.blurred();
        }

        this._currentFocusElement = x;

        if (x) {
            x.addStyleClass("focused");
            x.removeStyleClass("blurred");
            if (this._currentFocusElement.focused)
                this._currentFocusElement.focused();
        }
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

        var body = document.body;
        if (x) {
            InspectorController.attach();
            body.removeStyleClass("detached");
            body.addStyleClass("attached");
        } else {
            InspectorController.detach();
            body.removeStyleClass("attached");
            body.addStyleClass("detached");
        }
    },

    get showingStatusArea()
    {
        return this._showingStatusArea;
    },

    set showingStatusArea(x)
    {
        if (this._showingStatusArea === x)
            return;

        this._showingStatusArea = x;

        var list = document.getElementById("list");
        var status = document.getElementById("status");
        var statusButton = document.getElementById("statusToggle");

        if (x) {
            statusButton.addStyleClass("hide");
            list.addStyleClass("status-visible");
            status.addStyleClass("visible");
        } else {
            statusButton.removeStyleClass("hide");
            list.removeStyleClass("status-visible");
            status.removeStyleClass("visible");
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
            searchResultsResizer.style.display = null;
            var animations = [
                {element: resultsContainer, end: {top: Preferences.toolbarHeight}},
                {element: searchResultsResizer, end: {top: WebInspector.searchResultsHeight + Preferences.toolbarHeight - 2}},
                {element: document.getElementById("main"), end: {top: WebInspector.searchResultsHeight + Preferences.toolbarHeight + 1}}
            ];
            WebInspector.animateStyle(animations, 250);
        } else {
            searchResultsResizer.style.display = "none";
            var animations = [
                {element: resultsContainer, end: {top: Preferences.toolbarHeight - WebInspector.searchResultsHeight - 1}},
                {element: searchResultsResizer, end: {top: 0}},
                {element: document.getElementById("main"), end: {top: Preferences.toolbarHeight}}
            ];
            WebInspector.animateStyle(animations, 250, function() { resultsContainer.removeChildren(); delete this.searchResultsTree; });
        }
    }
}

WebInspector.setupLocalizedString = function(event)
{
    var localizedStringsURL = InspectorController.localizedStringsURL();
    if (localizedStringsURL) {
        var localizedStringsScriptElement = document.createElement("script");
        localizedStringsScriptElement.type = "text/javascript";
        localizedStringsScriptElement.src = localizedStringsURL;
        document.getElementsByTagName("head").item(0).appendChild(localizedStringsScriptElement);
    }
}

WebInspector.loaded = function()
{
    this.fileOutline = new TreeOutline(document.getElementById("list"));
    this.fileOutline.expandTreeElementsWhenArrowing = true;

    this.statusOutline = new TreeOutline(document.getElementById("status"));
    this.statusOutline.expandTreeElementsWhenArrowing = true;

    this.resourceCategories = {
        documents: new WebInspector.ResourceCategory(WebInspector.UIString("documents"), "documents"),
        stylesheets: new WebInspector.ResourceCategory(WebInspector.UIString("stylesheets"), "stylesheets"),
        images: new WebInspector.ResourceCategory(WebInspector.UIString("images"), "images"),
        scripts: new WebInspector.ResourceCategory(WebInspector.UIString("scripts"), "scripts"),
        fonts: new WebInspector.ResourceCategory(WebInspector.UIString("fonts"), "fonts"),
        databases: new WebInspector.ResourceCategory(WebInspector.UIString("databases"), "databases"),
        other: new WebInspector.ResourceCategory(WebInspector.UIString("other"), "other")
    };

    this.Tips = {
        ResourceNotCompressed: {id: 0, message: WebInspector.UIString("You could save bandwidth by having your web server compress this transfer with gzip or zlib.")}
    };

    this.Warnings = {
        IncorrectMIMEType: {id: 0, message: WebInspector.UIString("Resource interpreted as %@ but transferred with MIME type %@.")}
    };

    this.consoleListItem = new WebInspector.ConsoleStatusTreeElement(WebInspector.consolePanel);
    this.statusOutline.appendChild(this.consoleListItem);

    this.networkListItem = new WebInspector.StatusTreeElement(WebInspector.UIString("Network"), "network", WebInspector.networkPanel);
    this.statusOutline.appendChild(this.networkListItem);

    this.resourceCategories.documents.listItem.expand();

    this.currentFocusElement = document.getElementById("sidebar");

    this.addMainEventListeners(document);

    window.addEventListener("unload", function(event) { WebInspector.windowUnload(event) }, true);
    window.addEventListener("resize", function(event) { WebInspector.windowResize(event) }, true);

    document.addEventListener("mousedown", function(event) { WebInspector.changeFocus(event) }, true);
    document.addEventListener("focus", function(event) { WebInspector.changeFocus(event) }, true);
    document.addEventListener("keypress", function(event) { WebInspector.documentKeypress(event) }, true);
    document.addEventListener("beforecopy", function(event) { WebInspector.documentCanCopy(event) }, true);
    document.addEventListener("copy", function(event) { WebInspector.documentCopy(event) }, true);

    document.getElementById("back").title = WebInspector.UIString("Show previous panel.");
    document.getElementById("forward").title = WebInspector.UIString("Show next panel.");

    document.getElementById("search").setAttribute("placeholder", WebInspector.UIString("Search"));

    document.getElementById("back").addEventListener("click", function(event) { WebInspector.back() }, true);
    document.getElementById("forward").addEventListener("click", function(event) { WebInspector.forward() }, true);
    this.updateBackForwardButtons();

    document.getElementById("attachToggle").addEventListener("click", function(event) { WebInspector.toggleAttach() }, true);
    document.getElementById("statusToggle").addEventListener("click", function(event) { WebInspector.toggleStatusArea() }, true);
    document.getElementById("sidebarResizeWidget").addEventListener("mousedown", WebInspector.sidebarResizerDragStart, true);
    document.getElementById("sidebarResizer").addEventListener("mousedown", WebInspector.sidebarResizerDragStart, true);
    document.getElementById("searchResultsResizer").addEventListener("mousedown", WebInspector.searchResultsResizerDragStart, false);

    document.body.addStyleClass("detached");

    InspectorController.loaded();
}

var windowLoaded = function()
{
    WebInspector.setupLocalizedString(event);

    // Delay calling loaded to give time for the localized strings file to load.
    // Calling it too early will cause localized string lookups to fail.
    setTimeout(function() { WebInspector.loaded() }, 0);

    delete windowLoaded;
    window.removeEventListener("load", windowLoaded, false);
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

WebInspector.changeFocus = function(event)
{
    var nextFocusElement;

    var current = event.target;
    while (current) {
        if (current.nodeName.toLowerCase() === "input")
            nextFocusElement = current;
        current = current.parentNode;
    }

    if (!nextFocusElement)
        nextFocusElement = event.target.firstParentWithClass("focusable");

    this.currentFocusElement = nextFocusElement;
}

WebInspector.documentClick = function(event)
{
    var anchor = event.target.firstParentOrSelfWithNodeName("a");
    if (!anchor || !anchor.hasStyleClass("webkit-html-resource-link"))
        return;

    if (WebInspector.showResourceForURL(anchor.getAttribute("href"))) {
        event.preventDefault();
        event.stopPropagation();
    }
}

WebInspector.documentKeypress = function(event)
{
    if (!this.currentFocusElement)
        return;
    if (this.currentFocusElement.handleKeyEvent)
        this.currentFocusElement.handleKeyEvent(event);
    else if (this.currentFocusElement.id && this.currentFocusElement.id.length && WebInspector[this.currentFocusElement.id + "Keypress"])
        WebInspector[this.currentFocusElement.id + "Keypress"](event);
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

WebInspector.sidebarKeypress = function(event)
{
    var nextSelectedElement;

    if (this.fileOutline.selectedTreeElement) {
        if (!this.fileOutline.handleKeyEvent(event) && event.keyIdentifier === "Down" && !event.altKey && this.showingStatusArea) {
            var nextSelectedElement = this.statusOutline.children[0];
            while (nextSelectedElement && !nextSelectedElement.selectable)
                nextSelectedElement = nextSelectedElement.traverseNextTreeElement(false);
        }
    } else if (this.statusOutline.selectedTreeElement) {
        if (!this.showingStatusArea || (!this.statusOutline.handleKeyEvent(event) && event.keyIdentifier === "Up" && !event.altKey)) {
            var nextSelectedElement = this.fileOutline.children[0];
            var lastSelectable = null;

            while (nextSelectedElement) {
                if (nextSelectedElement.selectable)
                    lastSelectable = nextSelectedElement;
                nextSelectedElement = nextSelectedElement.traverseNextTreeElement(false);
            }

            nextSelectedElement = lastSelectable;
        }
    }

    if (nextSelectedElement) {
        nextSelectedElement.reveal();
        nextSelectedElement.select();

        event.preventDefault();
        event.stopPropagation();
    }
}

WebInspector.sidebarCopy = function(event)
{
    event.clipboardData.clearData();
    event.preventDefault();

    var selectedElement = this.fileOutline.selectedTreeElement;
    if (!selectedElement || !selectedElement.representedObject || !selectedElement.representedObject.url)
        return;

    event.clipboardData.setData("URL", this.fileOutline.selectedTreeElement.representedObject.url);
}

WebInspector.mainKeypress = function(event)
{
    if (this.currentPanel && this.currentPanel.handleKeyEvent)
        this.currentPanel.handleKeyEvent(event);
}

WebInspector.mainCopy = function(event)
{
    if (this.currentPanel && this.currentPanel.handleCopyEvent)
        this.currentPanel.handleCopyEvent(event);
}

WebInspector.searchResultsKeypress = function(event)
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

WebInspector.toggleStatusArea = function()
{
    this.showingStatusArea = !this.showingStatusArea;
}

WebInspector.sidebarResizerDragStart = function(event)
{
    WebInspector.dividerDragStart(document.getElementById("sidebar"), WebInspector.sidebarResizerDrag, WebInspector.sidebarResizerDragEnd, event, "col-resize");
}

WebInspector.sidebarResizerDragEnd = function(event)
{
    WebInspector.dividerDragEnd(document.getElementById("sidebar"), WebInspector.sidebarResizerDrag, WebInspector.sidebarResizerDragEnd, event);
}

WebInspector.sidebarResizerDrag = function(event)
{
    var sidebar = document.getElementById("sidebar");
    if (sidebar.dragging == true) {
        var x = event.clientX + window.scrollX;

        // FIXME: We can should come up with a better hueristic for constraining the size
        // of the sidebar.
        var newWidth = Number.constrain(x, 100, window.innerWidth - 100);

        if (x == newWidth)
            sidebar.dragLastX = x;

        sidebar.style.width = newWidth + "px";
        document.getElementById("sidebarResizer").style.left = (newWidth - 3) + "px";
        document.getElementById("main").style.left = newWidth + "px";
        document.getElementById("toolbarButtons").style.left = newWidth + "px";
        document.getElementById("searchResults").style.left = newWidth + "px";
        document.getElementById("searchResultsResizer").style.left = newWidth + "px";

        if (WebInspector.currentPanel && WebInspector.currentPanel.resize)
            WebInspector.currentPanel.resize();

        event.preventDefault();
    }
}

WebInspector.searchResultsResizerDragStart = function(event)
{
    WebInspector.dividerDragStart(document.getElementById("searchResults"), WebInspector.searchResultsResizerDrag, WebInspector.searchResultsResizerDragEnd, event, "row-resize");
}

WebInspector.searchResultsResizerDragEnd = function(event)
{
    WebInspector.dividerDragEnd(document.getElementById("searchResults"), WebInspector.searchResultsResizerDrag, WebInspector.searchResultsResizerDragEnd, event);
}

WebInspector.searchResultsResizerDrag = function(event)
{
    var searchResults = document.getElementById("searchResults");
    if (searchResults.dragging == true) {
        var y = event.clientY;
        var newHeight = Number.constrain(y, 100, window.innerHeight - 100);

        if (y == newHeight)
            searchResults.dragLastY = y;

        WebInspector.searchResultsHeight = newHeight - Preferences.toolbarHeight;
        searchResults.style.height = WebInspector.searchResultsHeight + "px";
        document.getElementById("main").style.top = (newHeight + 1) + "px";
        document.getElementById("searchResultsResizer").style.top = (newHeight - 2) + "px";
        event.preventDefault();
    }
}

WebInspector.dividerDragStart = function(element, dividerDrag, dividerDragEnd, event, cursor) 
{
    element.dragging = true;
    element.dragLastY = event.clientY + window.scrollY;
    element.dragLastX = event.clientX + window.scrollX;
    document.addEventListener("mousemove", dividerDrag, true);
    document.addEventListener("mouseup", dividerDragEnd, true);
    document.body.style.cursor = cursor;
    event.preventDefault();
}

WebInspector.dividerDragEnd = function(element, dividerDrag, dividerDragEnd, event) 
{
    element.dragging = false;
    document.removeEventListener("mousemove", dividerDrag, true);
    document.removeEventListener("mouseup", dividerDragEnd, true);
    document.body.style.removeProperty("cursor");
}

WebInspector.back = function()
{
    if (this.currentBackForwardIndex <= 0) {
        console.error("Can't go back from index " + this.currentBackForwardIndex);
        return;
    }

    this.navigateToPanel(this.backForwardList[--this.currentBackForwardIndex], null, true);
}

WebInspector.forward = function()
{
    if (this.currentBackForwardIndex >= this.backForwardList.length - 1) {
        console.error("Can't go forward from index " + this.currentBackForwardIndex);
        return;
    }

    this.navigateToPanel(this.backForwardList[++this.currentBackForwardIndex], null, true);
}

WebInspector.updateBackForwardButtons = function()
{
    var index = this.currentBackForwardIndex;

    document.getElementById("back").disabled = index <= 0;
    document.getElementById("forward").disabled = index >= this.backForwardList.length - 1;
}

WebInspector.showConsole = function()
{
    this.showingStatusArea = true;
    this.navigateToPanel(WebInspector.consolePanel);
}

WebInspector.showTimeline = function()
{
    this.showingStatusArea = true;
    this.navigateToPanel(WebInspector.networkPanel);
}

WebInspector.addResource = function(resource)
{
    this.resources.push(resource);

    if (resource.mainResource)
        this.mainResource = resource;

    if (resource.url) {
        this.resourceURLMap[resource.url] = resource;
        this.networkPanel.addResourceToTimeline(resource);
    }
}

WebInspector.removeResource = function(resource)
{
    resource.detach();

    resource.category.removeResource(resource);

    if (resource.url)
        delete this.resourceURLMap[resource.url];

    var resourcesLength = this.resources.length;
    for (var i = 0; i < resourcesLength; ++i) {
        if (this.resources[i] === resource) {
            this.resources.splice(i, 1);
            break;
        }
    }
}

WebInspector.clearResources = function()
{
    for (var category in this.resourceCategories)
        this.resourceCategories[category].removeAllResources();
    this.resources = [];
    this.backForwardList = [];
    this.currentBackForwardIndex = -1;
    delete this.mainResource;
}

WebInspector.clearDatabaseResources = function()
{
    this.resourceCategories.databases.removeAllResources();
}

WebInspector.resourceURLChanged = function(resource, oldURL)
{
    delete this.resourceURLMap[oldURL];
    this.resourceURLMap[resource.url] = resource;
}

WebInspector.addMessageToConsole = function(msg)
{
    this.consolePanel.addMessage(msg);
    switch (msg.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            ++this.consoleListItem.warnings;
            this.showingStatusArea = true;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            ++this.consoleListItem.errors;
            this.showingStatusArea = true;
            break;
    }
}

WebInspector.clearConsoleMessages = function()
{
    this.consolePanel.clearMessages();
    this.consoleListItem.warnings = this.consoleListItem.errors = 0;
}

WebInspector.clearNetworkTimeline = function()
{
    if (this._networkPanel)
        this._networkPanel.clearTimeline();
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

    for (var i = 0; i < this.resourceCategories.documents.resources.length; ++i) {
        var resource = this.resourceCategories.documents.resources[i];
        if (resource.documentNode !== node.ownerDocument)
            continue;

        this.navigateToPanel(resource.panel, "dom");
        resource.panel.focusedDOMNode = node;

        this.currentFocusElement = document.getElementById("main");

        break;
    }
}

WebInspector.resourceForURL = function(url)
{
    for (var resourceURL in this.resourceURLMap) {
        if (resourceURL.hasSubstring(url))
            return this.resourceURLMap[resourceURL];
    }

    return null;
}

WebInspector.showResourceForURL = function(url)
{
    var resource = this.resourceForURL(url);
    if (!resource)
        return false;

    this.navigateToResource(resource);
    return true;
}

WebInspector.linkifyURL = function(url, linkText, classes, isExternal)
{
    if (linkText === undefined)
        linkText = url.escapeHTML();
    classes = (classes === undefined) ? "" : classes + " ";
    classes += isExternal ? "webkit-html-external-link" : "webkit-html-resource-link";
    var link = "<a href=\"" + url + "\" class=\"" + classes + "\" target=\"_blank\">" + linkText + "</a>";
    return link;
}

WebInspector.addMainEventListeners = function(doc)
{
    doc.defaultView.addEventListener("focus", function(event) { WebInspector.windowFocused(event) }, true);
    doc.defaultView.addEventListener("blur", function(event) { WebInspector.windowBlured(event) }, true);
    doc.addEventListener("click", function(event) { WebInspector.documentClick(event) }, true);
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
        if (!isXPath && "source" in resource.panel.views) {
            resource.panel.setupSourceFrameIfNeeded();
            sourceResults = InspectorController.search(resource.panel.views.source.frameElement.contentDocument, query);
        }

        var domResults = [];
        if (resource.category === this.resourceCategories.documents) {
            try {
                var doc = resource.documentNode;
                var nodeList = doc.evaluate(xpathQuery, doc, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
                for (var j = 0; j < nodeList.snapshotLength; ++j)
                    domResults.push(nodeList.snapshotItem(i));
            } catch(err) {
                // ignore any exceptions. the query might be malformed, but we allow that.
            }
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

        WebInspector.navigateToPanel(element.representedObject.panel, "source");
        element.representedObject.line.scrollIntoView(true);
        resultsContainer.scrollToElement(element.listItemElement);
    }

    var domResultSelected = function(element)
    {
        WebInspector.navigateToPanel(element.representedObject.panel, "dom");
        element.representedObject.panel.focusedDOMNode = element.representedObject.node;
        resultsContainer.scrollToElement(element.listItemElement);
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

                var line = range.startContainer;
                while (line.parentNode && line.nodeName.toLowerCase() != "tr")
                    line = line.parentNode;
                var lineRange = file.resource.panel.views.source.frameElement.contentDocument.createRange();
                lineRange.selectNodeContents(line);

                // Don't include any error bubbles in the search result
                var end = line.lastChild.lastChild;
                if (end.nodeName.toLowerCase() == "div" && end.hasStyleClass("webkit-html-message-bubble")) {
                    while (end && end.nodeName.toLowerCase() == "div" && end.hasStyleClass("webkit-html-message-bubble"))
                        end = end.previousSibling;
                    lineRange.setEndAfter(end);
                }

                var beforeRange = file.resource.panel.views.source.frameElement.contentDocument.createRange();
                beforeRange.setStart(lineRange.startContainer, lineRange.startOffset);
                beforeRange.setEnd(range.startContainer, range.startOffset);

                var afterRange = file.resource.panel.views.source.frameElement.contentDocument.createRange();
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
                var item = new TreeElement(title, {panel: file.resource.panel, line: line, range: range}, false);
                item.onselect = sourceResultSelected;
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
                var item = new TreeElement(title, {panel: file.resource.panel, node: node}, false);
                item.onselect = domResultSelected;
                fileItem.appendChild(item);
            }
        }
    }
}

WebInspector.navigateToResource = function(resource)
{
    this.navigateToPanel(resource.panel);
}

WebInspector.navigateToPanel = function(panel, view, fromBackForwardAction)
{
    if (this.currentPanel === panel) {
        if (panel && view)
            panel.currentView = view;
        return;
    }

    if (!fromBackForwardAction) {
        var oldIndex = this.currentBackForwardIndex;
        if (oldIndex >= 0)
            this.backForwardList.splice(oldIndex + 1, this.backForwardList.length - oldIndex);
        this.currentBackForwardIndex++;
        this.backForwardList.push(panel);
    }

    this.currentPanel = panel;
    if (panel && view)
        panel.currentView = view;
}

WebInspector.UIString = function(string)
{
    if (string in this.localizedStrings)
        string = this.localizedStrings[string];
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

WebInspector.StatusTreeElement = function(title, iconClass, panel)
{
    TreeElement.call(this, "<span class=\"title only\">" + title + "</span><span class=\"icon " + iconClass + "\"></span>", null, false);
    this.panel = panel;
}

WebInspector.StatusTreeElement.prototype = {
    onselect: function()
    {
        var selectedElement = WebInspector.fileOutline.selectedTreeElement;
        if (selectedElement)
            selectedElement.deselect();
        if (this.panel)
            WebInspector.navigateToPanel(this.panel);
    },

    ondeselect: function()
    {
        if (this.panel)
            this.panel.hide();
    }
}

WebInspector.StatusTreeElement.prototype.__proto__ = TreeElement.prototype;

WebInspector.ConsoleStatusTreeElement = function(panel)
{
    WebInspector.StatusTreeElement.call(this, WebInspector.UIString("Console"), "console", panel);
}

WebInspector.ConsoleStatusTreeElement.prototype = {
    get warnings()
    {
        if (!("_warnings" in this))
            this._warnings = 0;

        return this._warnings;
    },

    set warnings(x)
    {
        if (this._warnings === x)
            return;

        this._warnings = x;

        this._updateTitle();
    },

    get errors()
    {
        if (!("_errors" in this))
            this._errors = 0;

        return this._errors;
    },

    set errors(x)
    {
        if (this._errors === x)
            return;

        this._errors = x;

        this._updateTitle();
    },

    _updateTitle: function()
    {
        var title = "<span class=\"title";
        if (!this.warnings && !this.errors)
            title += " only";
        title += "\">" + WebInspector.UIString("Console") + "</span><span class=\"icon console\"></span>";

        if (this.warnings || this.errors) {
            title += "<span class=\"info\">";
            if (this.errors) {
                title += this.errors + " error";
                if (this.errors > 1)
                    title += "s";
            }
            if (this.warnings) {
                if (this.errors)
                    title += ", ";
                title += this.warnings + " warning";
                if (this.warnings > 1)
                    title += "s";
            }
            title += "</span>";
        }

        this.title = title;
    }
}

WebInspector.ConsoleStatusTreeElement.prototype.__proto__ = WebInspector.StatusTreeElement.prototype;

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
