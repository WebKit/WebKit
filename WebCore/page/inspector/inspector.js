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
    showInheritedComputedStyleProperties: false
}

var WebInspector = {
    resources: [],
    resourceURLMap: {},
    backForwardList: [],

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
        }

        this._currentFocusElement = x;

        if (x) {
            x.addStyleClass("focused");
            x.removeStyleClass("blurred");
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
        this.updateViewButtons();

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
            WebInspector.animateStyle([{element: list, end: {bottom: 99}}, {element: status, end: {bottom: 21}}], 250);
        } else {
            statusButton.removeStyleClass("hide");
            WebInspector.animateStyle([{element: list, end: {bottom: 21}}, {element: status, end: {bottom: -57}}], 250);
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
        if (x) {
            var animations = [
                {element: resultsContainer, end: {top: 28}},
                {element: document.getElementById("main"), end: {top: 129}}
            ];
            WebInspector.animateStyle(animations, 250);
        } else {
            var animations = [
                {element: resultsContainer, end: {top: -73}},
                {element: document.getElementById("main"), end: {top: 28}}
            ];
            WebInspector.animateStyle(animations, 250, function() { resultsContainer.removeChildren(); delete this.searchResultsTree; });
        }
    }
}

WebInspector.loaded = function(event)
{
    this.fileOutline = new TreeOutline(document.getElementById("list"));
    this.statusOutline = new TreeOutline(document.getElementById("status"));

    this.resourceCategories = {
        documents: new WebInspector.ResourceCategory("documents"),
        stylesheets: new WebInspector.ResourceCategory("stylesheets"),
        images: new WebInspector.ResourceCategory("images"),
        scripts: new WebInspector.ResourceCategory("scripts"),
        other: new WebInspector.ResourceCategory("other")
    };

    this.consoleListItem = new WebInspector.ConsoleStatusTreeElement();
    this.consoleListItem.item.onselect = function(element) { WebInspector.StatusTreeElement.selected(element); WebInspector.navigateToPanel(WebInspector.consolePanel) };
    this.consoleListItem.item.ondeselect = function(element) { WebInspector.consolePanel.hide() };
    this.statusOutline.appendChild(this.consoleListItem.item);

    this.networkListItem = new WebInspector.StatusTreeElement("Network");
    this.networkListItem.onselect = function(element) { WebInspector.StatusTreeElement.selected(element); WebInspector.navigateToPanel(WebInspector.networkPanel); };
    this.networkListItem.ondeselect = function(element) { WebInspector.networkPanel.hide() };
    this.statusOutline.appendChild(this.networkListItem);

    this.resourceCategories.documents.listItem.expand();

    this.currentFocusElement = document.getElementById("sidebar");

    this.addMainEventListeners(document);

    window.addEventListener("unload", function(event) { WebInspector.windowUnload(event) }, true);
    document.addEventListener("mousedown", function(event) { WebInspector.changeFocus(event) }, true);
    document.addEventListener("focus", function(event) { WebInspector.changeFocus(event) }, true);
    document.addEventListener("keypress", function(event) { WebInspector.documentKeypress(event) }, true);

    document.getElementById("back").title = "Show previous panel.";
    document.getElementById("forward").title = "Show next panel.";

    document.getElementById("back").addEventListener("click", function(event) { WebInspector.back() }, true);
    document.getElementById("forward").addEventListener("click", function(event) { WebInspector.forward() }, true);
    this.updateBackForwardButtons();

    document.getElementById("attachToggle").addEventListener("click", function(event) { WebInspector.toggleAttach() }, true);
    document.getElementById("statusToggle").addEventListener("click", function(event) { WebInspector.toggleStatusArea() }, true);
    document.getElementById("sidebarResizeWidget").addEventListener("mousedown", WebInspector.sidebarResizerDragStart, true);

    document.body.addStyleClass("detached");

    window.removeEventListener("load", this.loaded, false);
    delete this.loaded;

    InspectorController.loaded();
}

window.addEventListener("load", function(event) { WebInspector.loaded(event) }, false);

WebInspector.windowUnload = function(event)
{
    InspectorController.windowUnloading();
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
    if (!this.currentFocusElement || !this.currentFocusElement.id || !this.currentFocusElement.id.length)
        return;
    if (this.currentFocusElement.id + "Keypress" in WebInspector)
        WebInspector[this.currentFocusElement.id + "Keypress"](event);
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

WebInspector.mainKeypress = function(event)
{
    if (this.currentPanel && this.currentPanel.handleKeyEvent)
        this.currentPanel.handleKeyEvent(event);
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
            else if (key == "current")
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
        var main = document.getElementById("main");

        var x = event.clientX + window.scrollX;
        var delta = sidebar.dragLastX - x;
        var newWidth = WebInspector.constrainedWidthFromElement(x, main);

        if (x == newWidth)
            sidebar.dragLastX = x;

        sidebar.style.width = newWidth + "px";
        main.style.left = newWidth + "px";
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

WebInspector.constrainedWidthFromElement = function(width, element, constrainLeft, constrainRight) 
{
    if (constrainLeft === undefined) constrainLeft = 0.25;
    if (constrainRight === undefined) constrainRight = 0.75;

    if (width < element.clientWidth * constrainLeft)
        width = 200;
    else if (width > element.clientWidth * constrainRight)
        width = element.clientWidth * constrainRight;

    return width;
}

WebInspector.back = function()
{
    if (this.currentBackForwardIndex <= 0) {
        alert("Can't go back from index " + this.currentBackForwardIndex);
        return;
    }

    this.navigateToPanel(this.backForwardList[--this.currentBackForwardIndex], true);
}

WebInspector.forward = function()
{
    if (this.currentBackForwardIndex >= this.backForwardList.length - 1) {
        alert("Can't go forward from index " + this.currentBackForwardIndex);
        return;
    }

    this.navigateToPanel(this.backForwardList[++this.currentBackForwardIndex], true);
}

WebInspector.updateBackForwardButtons = function()
{
    var index = this.currentBackForwardIndex;

    document.getElementById("back").disabled = index <= 0;
    document.getElementById("forward").disabled = index >= this.backForwardList.length - 1;
}

WebInspector.updateViewButtons = function()
{
    var buttonContainer = document.getElementById("viewbuttons");
    buttonContainer.removeChildren();

    if (!this.currentPanel || !this.currentPanel.viewButtons)
        return;

    var buttons = this.currentPanel.viewButtons;
    if (buttons.length < 2)
        return;

    for (var i = 0; i < buttons.length; ++i) {
        var button = buttons[i];

        if (i === 0)
            button.addStyleClass("first");
        else if (i === (buttons.length - 1))
            button.addStyleClass("last");

        if (i) {
            var divider = document.createElement("img");
            divider.className = "split-button-divider";
            buttonContainer.appendChild(divider);
        }

        button.addStyleClass("split-button");
        button.addStyleClass("view-button-" + button.title.toLowerCase());

        buttonContainer.appendChild(button);
    }
}

WebInspector.addResource = function(resource)
{
    this.resources.push(resource);
    this.resourceURLMap[resource.url] = resource;

    if (resource.mainResource)
        this.mainResource = resource;

    this.networkPanel.addResourceToTimeline(resource);
}

WebInspector.removeResource = function(resource)
{
    resource.detach();

    resource.category.removeResource(resource);

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

        resource.panel.navigateToView("dom");
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

WebInspector.navigateToView = function(view)
{
    if (!view) {
        alert("Called navigateToView(null)");
        return;
    }

    view.panel.currentView = view;
    this.navigateToPanel(view.panel);
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
            resource.panel.refreshIfNeeded();
            if ("sourceFrame" in resource.panel)
                sourceResults = InspectorController.search(resource.panel.sourceFrame.contentDocument, query);
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

    var sourceResultSelected = function(element)
    {
        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(element.representedObject.range);

        element.representedObject.panel.navigateToView("source");
        element.representedObject.line.scrollIntoView(true);
        resultsContainer.scrollToElement(element._listItemNode);
    }

    var domResultSelected = function(element)
    {
        element.representedObject.panel.navigateToView("dom");
        element.representedObject.panel.focusedDOMNode = element.representedObject.node;
        resultsContainer.scrollToElement(element._listItemNode);
    }

    for (var i = 0; i < files.length; ++i) {
        var file = files[i];

        var fileItem = new TreeElement(file.resource.displayName, {}, true);
        fileItem.expanded = true;
        fileItem.selectable = false;
        this.searchResultsTree.appendChild(fileItem);

        if (file.sourceResults.length) {
            for (var j = 0; j < file.sourceResults.length; ++j) {
                var range = file.sourceResults[j];

                var line = range.startContainer;
                while (line.parentNode && line.nodeName.toLowerCase() != "tr")
                    line = line.parentNode;
                var lineRange = file.resource.panel.sourceFrame.contentDocument.createRange();
                lineRange.selectNodeContents(line);

                // Don't include any error bubbles in the search result
                var end = line.lastChild.lastChild;
                if (end.nodeName.toLowerCase() == "div" && end.hasStyleClass("webkit-html-message-bubble")) {
                    while (end && end.nodeName.toLowerCase() == "div" && end.hasStyleClass("webkit-html-message-bubble"))
                        end = end.previousSibling;
                    lineRange.setEndAfter(end);
                }

                var beforeRange = file.resource.panel.sourceFrame.contentDocument.createRange();
                beforeRange.setStart(lineRange.startContainer, lineRange.startOffset);
                beforeRange.setEnd(range.startContainer, range.startOffset);

                var afterRange = file.resource.panel.sourceFrame.contentDocument.createRange();
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
                    title += "<div class=\"search-results-section\">Source</div>";
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
                    title += "<div class=\"search-results-section\">DOM</div>";
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

WebInspector.navigateToPanel = function(panel, fromBackForwardAction)
{
    if (this.currentPanel == panel)
        return;

    if (!fromBackForwardAction) {
        var oldIndex = this.currentBackForwardIndex;
        if (oldIndex >= 0)
            this.backForwardList.splice(oldIndex + 1, this.backForwardList.length - oldIndex);
        this.currentBackForwardIndex++;
        this.backForwardList.push(panel);
    }

    this.currentPanel = panel;
}

WebInspector.Panel = function()
{
    this.element = document.createElement("div");
    this.element.className = "panel";
    this.attach();

    this._needsRefresh = true;
    this.refresh();
}

WebInspector.Panel.prototype = {
    show: function()
    {
        this.visible = true;
    },

    hide: function()
    {
        this.visible = false;
    },

    attach: function()
    {
        document.getElementById("main").appendChild(this.element);
    },

    detach: function()
    {
        if (WebInspector.currentPanel === this)
            WebInspector.currentPanel = null;
        if (this.element && this.element.parentNode)
            this.element.parentNode.removeChild(this.element);
    },

    refresh: function()
    {
    },

    refreshIfNeeded: function()
    {
        if (this.needsRefresh)
            this.refresh();
    },

    get visible()
    {
        return this._visible;
    },

    set visible(x)
    {
        if (this._visible === x)
            return;

        this._visible = x;

        if (x) {
            this.element.addStyleClass("selected");
            this.refreshIfNeeded();
        } else {
            this.element.removeStyleClass("selected");
        }
    },

    get needsRefresh()
    {
        return this._needsRefresh;
    },

    set needsRefresh(x)
    {
        if (this._needsRefresh === x)
            return;
        this._needsRefresh = x;
        if (x && this.visible)
            this.refresh();
    }
}

WebInspector.StatusTreeElement = function(title)
{
    var item = new TreeElement("<span class=\"title only\">" + title + "</span><span class=\"icon " + title.toLowerCase() + "\"></span>", {}, false);
    item.onselect = WebInspector.StatusTreeElement.selected;
    return item;
}

WebInspector.StatusTreeElement.selected = function(element)
{
    var selectedElement = WebInspector.fileOutline.selectedTreeElement;
    if (selectedElement)
        selectedElement.deselect();
}

WebInspector.ConsoleStatusTreeElement = function()
{
    this.item = WebInspector.StatusTreeElement.call(this, "Console");
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
        title += "\">Console</span><span class=\"icon console\"></span>";

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

        this.item.title = title;
    }
}

WebInspector.ConsoleStatusTreeElement.prototype.__proto__ = WebInspector.StatusTreeElement.prototype;

WebInspector.Tips = {
    ResourceNotCompressed: {id: 0, message: "You could save bandwidth by having your web server compress this transfer with gzip or zlib."}
}

WebInspector.Warnings = {
    IncorrectMIMEType: {id: 0, message: "Resource interpreted as %s but transferred with MIME type %s."}
}

// This table maps MIME types to the Resource.Types which are valid for them.
// The following line:
//    "text/html":                {0: 1},
// means that text/html is a valid MIME type for resources that have type
// WebInspector.Resource.Type.Document (which has a value of 0).
WebInspector.MIMETypes = {
    "text/html":                {0: 1},
    "text/xml":                 {0: 1},
    "text/plain":               {0: 1},
    "application/xhtml+xml":    {0: 1},
    "text/css":                 {1: 1},
    "text/xsl":                 {1: 1},
    "image/jpeg":               {2: 1},
    "image/png":                {2: 1},
    "image/gif":                {2: 1},
    "image/bmp":                {2: 1},
    "image/x-icon":             {2: 1},
    "image/x-xbitmap":          {2: 1},
    "text/javascript":          {3: 1},
    "text/ecmascript":          {3: 1},
    "application/javascript":   {3: 1},
    "application/ecmascript":   {3: 1},
    "application/x-javascript": {3: 1},
    "text/javascript1.1":       {3: 1},
    "text/javascript1.2":       {3: 1},
    "text/javascript1.3":       {3: 1},
    "text/jscript":             {3: 1},
    "text/livescript":          {3: 1},
}
