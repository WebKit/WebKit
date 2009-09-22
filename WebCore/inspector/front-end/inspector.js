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

var Preferences = {
    ignoreWhitespace: true,
    showUserAgentStyles: true,
    maxInlineTextChildLength: 80,
    minConsoleHeight: 75,
    minSidebarWidth: 100,
    minElementsSidebarWidth: 200,
    minScriptsSidebarWidth: 200,
    showInheritedComputedStyleProperties: false,
    styleRulesExpandedState: {},
    showMissingLocalizedStrings: false,
    heapProfilerPresent: false,
    samplingCPUProfiler: false,
    showColorNicknames: true,
    colorFormat: "hex"
}

var WebInspector = {
    resources: {},
    resourceURLMap: {},
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
        if (this._currentFocusElement !== x)
            this._previousFocusElement = this._currentFocusElement;
        this._currentFocusElement = x;

        if (this._currentFocusElement) {
            this._currentFocusElement.focus();

            // Make a caret selection inside the new element if there isn't a range selection and
            // there isn't already a caret selection inside.
            var selection = window.getSelection();
            if (selection.isCollapsed && !this._currentFocusElement.isInsertionCaretInside()) {
                var selectionRange = this._currentFocusElement.ownerDocument.createRange();
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

        this.updateSearchLabel();

        if (x) {
            x.show();

            if (this.currentQuery) {
                if (x.performSearch) {
                    function performPanelSearch()
                    {
                        this.updateSearchMatchesCount();

                        x.currentQuery = this.currentQuery;
                        x.performSearch(this.currentQuery);
                    }

                    // Perform the search on a timeout so the panel switches fast.
                    setTimeout(performPanelSearch.bind(this), 0);
                } else {
                    // Update to show Not found for panels that can't be searched.
                    this.updateSearchMatchesCount();
                }
            }
        }

        for (var panelName in WebInspector.panels) {
            if (WebInspector.panels[panelName] == x)
                InspectorController.storeLastActivePanel(panelName);
        }
    },

    _createPanels: function()
    {
        var hiddenPanels = (InspectorController.hiddenPanels() || "").split(',');
        if (hiddenPanels.indexOf("elements") === -1)
            this.panels.elements = new WebInspector.ElementsPanel();
        if (hiddenPanels.indexOf("resources") === -1)
            this.panels.resources = new WebInspector.ResourcesPanel();
        if (hiddenPanels.indexOf("scripts") === -1)
            this.panels.scripts = new WebInspector.ScriptsPanel();
        if (hiddenPanels.indexOf("profiles") === -1)
            this.panels.profiles = new WebInspector.ProfilesPanel();
        if (hiddenPanels.indexOf("storage") === -1 && hiddenPanels.indexOf("databases") === -1)
            this.panels.storage = new WebInspector.StoragePanel();      
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

        this.updateSearchLabel();

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
    },

    get styleChanges()
    {
        return this._styleChanges;
    },

    set styleChanges(x)
    {
        x = Math.max(x, 0);

        if (this._styleChanges === x)
            return;
        this._styleChanges = x;
        this._updateChangesCount();
    },

    _updateChangesCount: function()
    {
        // TODO: Remove immediate return when enabling the Changes Panel
        return;

        var changesElement = document.getElementById("changes-count");
        if (!changesElement)
            return;

        if (!this.styleChanges) {
            changesElement.addStyleClass("hidden");
            return;
        }

        changesElement.removeStyleClass("hidden");
        changesElement.removeChildren();

        if (this.styleChanges) {
            var styleChangesElement = document.createElement("span");
            styleChangesElement.id = "style-changes-count";
            styleChangesElement.textContent = this.styleChanges;
            changesElement.appendChild(styleChangesElement);
        }

        if (this.styleChanges) {
            if (this.styleChanges === 1)
                changesElement.title = WebInspector.UIString("%d style change", this.styleChanges);
            else
                changesElement.title = WebInspector.UIString("%d style changes", this.styleChanges);
        }
    },

    get hoveredDOMNode()
    {
        return this._hoveredDOMNode;
    },

    set hoveredDOMNode(x)
    {
        if (this._hoveredDOMNode === x)
            return;

        this._hoveredDOMNode = x;

        if (this._hoveredDOMNode)
            this._updateHoverHighlightSoon(this.showingDOMNodeHighlight ? 50 : 500);
        else
            this._updateHoverHighlight();
    },

    _updateHoverHighlightSoon: function(delay)
    {
        if ("_updateHoverHighlightTimeout" in this)
            clearTimeout(this._updateHoverHighlightTimeout);
        this._updateHoverHighlightTimeout = setTimeout(this._updateHoverHighlight.bind(this), delay);
    },

    _updateHoverHighlight: function()
    {
        if ("_updateHoverHighlightTimeout" in this) {
            clearTimeout(this._updateHoverHighlightTimeout);
            delete this._updateHoverHighlightTimeout;
        }

        if (this._hoveredDOMNode) {
            InspectorController.highlightDOMNode(this._hoveredDOMNode.id);
            this.showingDOMNodeHighlight = true;
        } else {
            InspectorController.hideDOMNodeHighlight();
            this.showingDOMNodeHighlight = false;
        }
    }
}

WebInspector.loaded = function()
{
    var platform = InspectorController.platform();
    document.body.addStyleClass("platform-" + platform);

    var colorFormat = InspectorController.setting("color-format");
    if (colorFormat)
        Preferences.colorFormat = colorFormat;

    this.drawer = new WebInspector.Drawer();
    this.console = new WebInspector.ConsoleView(this.drawer);
    // TODO: Uncomment when enabling the Changes Panel
    // this.changes = new WebInspector.ChangesView(this.drawer);
    // TODO: Remove class="hidden" from inspector.html on button#changes-status-bar-item
    this.drawer.visibleView = this.console;
    this.domAgent = new WebInspector.DOMAgent();

    this.resourceCategories = {
        documents: new WebInspector.ResourceCategory(WebInspector.UIString("Documents"), "documents"),
        stylesheets: new WebInspector.ResourceCategory(WebInspector.UIString("Stylesheets"), "stylesheets"),
        images: new WebInspector.ResourceCategory(WebInspector.UIString("Images"), "images"),
        scripts: new WebInspector.ResourceCategory(WebInspector.UIString("Scripts"), "scripts"),
        xhr: new WebInspector.ResourceCategory(WebInspector.UIString("XHR"), "xhr"),
        fonts: new WebInspector.ResourceCategory(WebInspector.UIString("Fonts"), "fonts"),
        other: new WebInspector.ResourceCategory(WebInspector.UIString("Other"), "other")
    };

    this.panels = {};
    this._createPanels();

    var toolbarElement = document.getElementById("toolbar");
    var previousToolbarItem = toolbarElement.children[0];

    this.panelOrder = [];
    for (var panelName in this.panels) {
        var panel = this.panels[panelName];
        var panelToolbarItem = panel.toolbarItem;
        this.panelOrder.push(panel);
        panelToolbarItem.addEventListener("click", this._toolbarItemClicked.bind(this));
        if (previousToolbarItem)
            toolbarElement.insertBefore(panelToolbarItem, previousToolbarItem.nextSibling);
        else
            toolbarElement.insertBefore(panelToolbarItem, toolbarElement.firstChild);
        previousToolbarItem = panelToolbarItem;
    }

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
    document.addEventListener("contextmenu", this.contextMenu.bind(this), true);

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
    errorWarningCount.addEventListener("click", this.showConsole.bind(this), false);
    this._updateErrorAndWarningCounts();

    this.styleChanges = 0;
    // TODO: Uncomment when enabling the Changes Panel
    // var changesElement = document.getElementById("changes-count");
    // changesElement.addEventListener("click", this.showChanges.bind(this), false);
    // this._updateErrorAndWarningCounts();

    var searchField = document.getElementById("search");
    searchField.addEventListener("keydown", this.searchKeyDown.bind(this), false);
    searchField.addEventListener("keyup", this.searchKeyUp.bind(this), false);
    searchField.addEventListener("search", this.performSearch.bind(this), false); // when the search is emptied

    document.getElementById("toolbar").addEventListener("mousedown", this.toolbarDragStart, true);
    document.getElementById("close-button").addEventListener("click", this.close, true);

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

WebInspector.dispatch = function() {
    var methodName = arguments[0];
    var parameters = Array.prototype.slice.call(arguments, 1);
    WebInspector[methodName].apply(this, parameters);
}

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

WebInspector.setAttachedWindow = function(attached)
{
    this.attached = attached;
}

WebInspector.close = function(event)
{
    InspectorController.closeWindow();
}

WebInspector.documentClick = function(event)
{
    var anchor = event.target.enclosingNodeOrSelfWithNodeName("a");
    if (!anchor)
        return;

    // Prevent the link from navigating, since we don't do any navigation by following links normally.
    event.preventDefault();

    function followLink()
    {
        // FIXME: support webkit-html-external-link links here.
        if (anchor.href in WebInspector.resourceURLMap) {
            if (anchor.hasStyleClass("webkit-html-external-link")) {
                anchor.removeStyleClass("webkit-html-external-link");
                anchor.addStyleClass("webkit-html-resource-link");
            }

            WebInspector.showResourceForURL(anchor.href, anchor.lineNumber, anchor.preferredPanel);
        } else {
            var profileStringRegEx = new RegExp("webkit-profile://.+/([0-9]+)");
            var profileString = profileStringRegEx.exec(anchor.href);
            if (profileString)
                WebInspector.showProfileById(profileString[1])
        }
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

WebInspector.documentKeyDown = function(event)
{
    if (!this.currentFocusElement)
        return;
    if (this.currentFocusElement.handleKeyEvent)
        this.currentFocusElement.handleKeyEvent(event);
    else if (this.currentFocusElement.id && this.currentFocusElement.id.length && WebInspector[this.currentFocusElement.id + "KeyDown"])
        WebInspector[this.currentFocusElement.id + "KeyDown"](event);

    if (!event.handled) {
        var isMac = InspectorController.platform().indexOf("mac-") === 0;

        switch (event.keyIdentifier) {
            case "U+001B": // Escape key
                this.drawer.visible = !this.drawer.visible;
                event.preventDefault();
                break;

            case "U+0046": // F key
                if (isMac)
                    var isFindKey = event.metaKey && !event.ctrlKey && !event.altKey && !event.shiftKey;
                else
                    var isFindKey = event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey;

                if (isFindKey) {
                    var searchField = document.getElementById("search");
                    searchField.focus();
                    searchField.select();
                    event.preventDefault();
                }

                break;

            case "U+0047": // G key
                if (isMac)
                    var isFindAgainKey = event.metaKey && !event.ctrlKey && !event.altKey;
                else
                    var isFindAgainKey = event.ctrlKey && !event.metaKey && !event.altKey;

                if (isFindAgainKey) {
                    if (event.shiftKey) {
                        if (this.currentPanel.jumpToPreviousSearchResult)
                            this.currentPanel.jumpToPreviousSearchResult();
                    } else if (this.currentPanel.jumpToNextSearchResult)
                        this.currentPanel.jumpToNextSearchResult();
                    event.preventDefault();
                }

                break;

            case "U+005B": // [ key
                if (isMac)
                    var isRotateLeft = event.metaKey && !event.shiftKey && !event.ctrlKey && !event.altKey;
                else
                    var isRotateLeft = event.ctrlKey && !event.shiftKey && !event.metaKey && !event.altKey;

                if (isRotateLeft) {
                    var index = this.panelOrder.indexOf(this.currentPanel);
                    index = (index === 0) ? this.panelOrder.length - 1 : index - 1;
                    this.panelOrder[index].toolbarItem.click();
                    event.preventDefault();
                }

                break;

            case "U+005D": // ] key
                if (isMac)
                    var isRotateRight = event.metaKey && !event.shiftKey && !event.ctrlKey && !event.altKey;
                else
                    var isRotateRight = event.ctrlKey && !event.shiftKey && !event.metaKey && !event.altKey;

                if (isRotateRight) {
                    var index = this.panelOrder.indexOf(this.currentPanel);
                    index = (index + 1) % this.panelOrder.length;
                    this.panelOrder[index].toolbarItem.click();
                    event.preventDefault();
                }

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

WebInspector.contextMenu = function(event)
{
    if (event.handled || event.target.hasStyleClass("popup-glasspane"))
        event.preventDefault();
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
        var key = null;
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

WebInspector.updateSearchLabel = function()
{
    if (!this.currentPanel)
        return;

    var newLabel = WebInspector.UIString("Search %s", this.currentPanel.toolbarItemLabel);
    if (this.attached)
        document.getElementById("search").setAttribute("placeholder", newLabel);
    else {
        document.getElementById("search").removeAttribute("placeholder");
        document.getElementById("search-toolbar-label").textContent = newLabel;
    }
}

WebInspector.toggleAttach = function()
{
    this.attached = !this.attached;
}

WebInspector.toolbarDragStart = function(event)
{
    if ((!WebInspector.attached && InspectorController.platform() !== "mac-leopard") || InspectorController.platform() == "qt")
        return;

    var target = event.target;
    if (target.hasStyleClass("toolbar-item") && target.hasStyleClass("toggleable"))
        return;

    var toolbar = document.getElementById("toolbar");
    if (target !== toolbar && !target.hasStyleClass("toolbar-item"))
        return;

    toolbar.lastScreenX = event.screenX;
    toolbar.lastScreenY = event.screenY;

    WebInspector.elementDragStart(toolbar, WebInspector.toolbarDrag, WebInspector.toolbarDragEnd, event, (WebInspector.attached ? "row-resize" : "default"));
}

WebInspector.toolbarDragEnd = function(event)
{
    var toolbar = document.getElementById("toolbar");

    WebInspector.elementDragEnd(event);

    delete toolbar.lastScreenX;
    delete toolbar.lastScreenY;
}

WebInspector.toolbarDrag = function(event)
{
    var toolbar = document.getElementById("toolbar");

    if (WebInspector.attached) {
        var height = window.innerHeight - (event.screenY - toolbar.lastScreenY);

        InspectorController.setAttachedWindowHeight(height);
    } else {
        var x = event.screenX - toolbar.lastScreenX;
        var y = event.screenY - toolbar.lastScreenY;

        // We cannot call window.moveBy here because it restricts the movement
        // of the window at the edges.
        InspectorController.moveByUnrestricted(x, y);
    }

    toolbar.lastScreenX = event.screenX;
    toolbar.lastScreenY = event.screenY;

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
    this.drawer.showView(this.console);
}

WebInspector.showChanges = function()
{
    this.drawer.showView(this.changes);
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

WebInspector.showStoragePanel = function()
{
    this.currentPanel = this.panels.storage;
}

WebInspector.addResource = function(identifier, payload)
{
    var resource = new WebInspector.Resource(
        payload.requestHeaders,
        payload.requestURL,
        payload.host,
        payload.path,
        payload.lastPathComponent,
        identifier,
        payload.isMainResource,
        payload.cached,
        payload.requestMethod,
        payload.requestFormData);
    this.resources[identifier] = resource;
    this.resourceURLMap[resource.url] = resource;

    if (resource.mainResource) {
        this.mainResource = resource;
        this.panels.elements.reset();
    }

    if (this.panels.resources)
        this.panels.resources.addResource(resource);
}

WebInspector.clearConsoleMessages = function()
{
    WebInspector.console.clearMessages(false);
}

WebInspector.selectDatabase = function(o)
{
    WebInspector.showStoragePanel();
    WebInspector.panels.storage.selectDatabase(o);
}

WebInspector.selectDOMStorage = function(o)
{
    WebInspector.showStoragePanel();
    WebInspector.panels.storage.selectDOMStorage(o);
}

WebInspector.updateResource = function(identifier, payload)
{
    var resource = this.resources[identifier];
    if (!resource)
        return;

    if (payload.didRequestChange) {
        resource.url = payload.url;
        resource.domain = payload.domain;
        resource.path = payload.path;
        resource.lastPathComponent = payload.lastPathComponent;
        resource.requestHeaders = payload.requestHeaders;
        resource.mainResource = payload.mainResource;
        resource.requestMethod = payload.requestMethod;
        resource.requestFormData = payload.requestFormData;
    }

    if (payload.didResponseChange) {
        resource.mimeType = payload.mimeType;
        resource.suggestedFilename = payload.suggestedFilename;
        resource.expectedContentLength = payload.expectedContentLength;
        resource.statusCode = payload.statusCode;
        resource.suggestedFilename = payload.suggestedFilename;
        resource.responseHeaders = payload.responseHeaders;
    }

    if (payload.didTypeChange) {
        resource.type = payload.type;
    }

    if (payload.didLengthChange) {
        resource.contentLength = payload.contentLength;
    }

    if (payload.didCompletionChange) {
        resource.failed = payload.failed;
        resource.finished = payload.finished;
    }

    if (payload.didTimingChange) {
        if (payload.startTime)
            resource.startTime = payload.startTime;
        if (payload.responseReceivedTime)
            resource.responseReceivedTime = payload.responseReceivedTime;
        if (payload.endTime)
            resource.endTime = payload.endTime;
    }
}

WebInspector.removeResource = function(identifier)
{
    var resource = this.resources[identifier];
    if (!resource)
        return;

    resource.category.removeResource(resource);
    delete this.resourceURLMap[resource.url];
    delete this.resources[identifier];

    if (this.panels.resources)
        this.panels.resources.removeResource(resource);
}

WebInspector.addDatabase = function(payload)
{
    var database = new WebInspector.Database(
        payload.database,
        payload.domain,
        payload.name,
        payload.version);
    this.panels.storage.addDatabase(database);
}

WebInspector.addDOMStorage = function(payload)
{
    var domStorage = new WebInspector.DOMStorage(
        payload.domStorage,
        payload.host,
        payload.isLocalStorage);
    this.panels.storage.addDOMStorage(domStorage);
}

WebInspector.resourceTrackingWasEnabled = function()
{
    this.panels.resources.resourceTrackingWasEnabled();
}

WebInspector.resourceTrackingWasDisabled = function()
{
    this.panels.resources.resourceTrackingWasDisabled();
}

WebInspector.attachDebuggerWhenShown = function()
{
    this.panels.scripts.attachDebuggerWhenShown();
}

WebInspector.debuggerWasEnabled = function()
{
    this.panels.scripts.debuggerWasEnabled();
}

WebInspector.debuggerWasDisabled = function()
{
    this.panels.scripts.debuggerWasDisabled();
}

WebInspector.profilerWasEnabled = function()
{
    this.panels.profiles.profilerWasEnabled();
}

WebInspector.profilerWasDisabled = function()
{
    this.panels.profiles.profilerWasDisabled();
}

WebInspector.parsedScriptSource = function(sourceID, sourceURL, source, startingLine)
{
    this.panels.scripts.addScript(sourceID, sourceURL, source, startingLine);
}

WebInspector.failedToParseScriptSource = function(sourceURL, source, startingLine, errorLine, errorMessage)
{
    this.panels.scripts.addScript(null, sourceURL, source, startingLine, errorLine, errorMessage);
}

WebInspector.pausedScript = function(callFrames)
{
    this.panels.scripts.debuggerPaused(callFrames);
}

WebInspector.resumedScript = function()
{
    this.panels.scripts.debuggerResumed();
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

    this.resources = {};
    this.resourceURLMap = {};
    this.hoveredDOMNode = null;

    delete this.mainResource;

    this.console.clearMessages();
}

WebInspector.resourceURLChanged = function(resource, oldURL)
{
    delete this.resourceURLMap[oldURL];
    this.resourceURLMap[resource.url] = resource;
}

WebInspector.addMessageToConsole = function(payload)
{
    var consoleMessage = new WebInspector.ConsoleMessage(
        payload.source,
        payload.type,
        payload.level,
        payload.line,
        payload.url,
        payload.groupLevel,
        payload.repeatCount);
    consoleMessage.setMessageBody(Array.prototype.slice.call(arguments, 1));
    this.console.addMessage(consoleMessage);
}

WebInspector.log = function(message)
{
    // remember 'this' for setInterval() callback
    var self = this;
    
    // return indication if we can actually log a message
    function isLogAvailable()
    {
        return WebInspector.ConsoleMessage && WebInspector.ObjectProxy && self.console;
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
        var repeatCount = 1;
        if (message == WebInspector.log.lastMessage)
            repeatCount = WebInspector.log.repeatCount + 1;
    
        WebInspector.log.lastMessage = message;
        WebInspector.log.repeatCount = repeatCount;
        
        // ConsoleMessage expects a proxy object
        message = new WebInspector.ObjectProxy(null, [], 0, message, false);
        
        // post the message
        var msg = new WebInspector.ConsoleMessage(
            WebInspector.ConsoleMessage.MessageSource.Other,
            WebInspector.ConsoleMessage.MessageType.Log,
            WebInspector.ConsoleMessage.MessageLevel.Debug,
            -1,
            null,
            null,
            repeatCount,
            message);
    
        self.console.addMessage(msg);
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

WebInspector.addProfile = function(profile)
{
    this.panels.profiles.addProfile(profile);
}

WebInspector.setRecordingProfile = function(isProfiling)
{
    this.panels.profiles.setRecordingProfile(isProfiling);
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

WebInspector.updateFocusedNode = function(nodeId)
{
    var node = WebInspector.domAgent.nodeForId(nodeId);
    if (!node)
        // FIXME: Should we deselect if null is passed in?
        return;

    this.currentPanel = this.panels.elements;
    this.panels.elements.focusedDOMNode = node;
}

WebInspector.displayNameForURL = function(url)
{
    if (!url)
        return "";
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
    if (!this.currentPanel)
        return false;
    this.currentPanel.showResource(resource, line);
    return true;
}

WebInspector.linkifyStringAsFragment = function(string)
{
    var container = document.createDocumentFragment();
    var linkStringRegEx = new RegExp("(?:[a-zA-Z][a-zA-Z0-9+.-]{2,}://|www\\.)[\\w$\\-_+*'=\\|/\\\\(){}[\\]%@&#~,:;.!?]{2,}[\\w$\\-_+*=\\|/\\\\({%@&#~]");

    while (string) {
        var linkString = linkStringRegEx.exec(string);
        if (!linkString)
            break;

        linkString = linkString[0];
        var title = linkString;
        var linkIndex = string.indexOf(linkString);
        var nonLink = string.substring(0, linkIndex);
        container.appendChild(document.createTextNode(nonLink));

        var profileStringRegEx = new RegExp("webkit-profile://(.+)/[0-9]+");
        var profileStringMatches = profileStringRegEx.exec(title);
        var profileTitle;
        if (profileStringMatches)
            profileTitle = profileStringMatches[1];
        if (profileTitle)
            title = WebInspector.panels.profiles.displayTitleForProfileLink(profileTitle);

        var realURL = (linkString.indexOf("www.") === 0 ? "http://" + linkString : linkString);
        container.appendChild(WebInspector.linkifyURLAsNode(realURL, title, null, (realURL in WebInspector.resourceURLMap)));
        string = string.substring(linkIndex + linkString.length, string.length);
    }

    if (string)
        container.appendChild(document.createTextNode(string));

    return container;
}

WebInspector.showProfileById = function(uid) {
    WebInspector.showProfilesPanel();
    WebInspector.panels.profiles.showProfileById(uid);
}

WebInspector.linkifyURLAsNode = function(url, linkText, classes, isExternal)
{
    if (!linkText)
        linkText = url;
    classes = (classes ? classes + " " : "");
    classes += isExternal ? "webkit-html-external-link" : "webkit-html-resource-link";

    var a = document.createElement("a");
    a.href = url;
    a.className = classes;
    a.title = url;
    a.target = "_blank";
    a.textContent = linkText;

    return a;
}

WebInspector.linkifyURL = function(url, linkText, classes, isExternal)
{
    // Use the DOM version of this function so as to avoid needing to escape attributes.
    // FIXME:  Get rid of linkifyURL entirely.
    return WebInspector.linkifyURLAsNode(url, linkText, classes, isExternal).outerHTML;
}

WebInspector.addMainEventListeners = function(doc)
{
    doc.defaultView.addEventListener("focus", this.windowFocused.bind(this), true);
    doc.defaultView.addEventListener("blur", this.windowBlured.bind(this), true);
    doc.addEventListener("click", this.documentClick.bind(this), true);
}

WebInspector.searchKeyDown = function(event)
{
    if (event.keyIdentifier !== "Enter")
        return;

    // Call preventDefault since this was the Enter key. This prevents a "search" event
    // from firing for key down. We handle the Enter key on key up in searchKeyUp. This
    // stops performSearch from being called twice in a row.
    event.preventDefault();
}

WebInspector.searchKeyUp = function(event)
{
    if (event.keyIdentifier !== "Enter")
        return;

    // Select all of the text so the user can easily type an entirely new query.
    event.target.select();

    // Only call performSearch if the Enter key was pressed. Otherwise the search
    // performance is poor because of searching on every key. The search field has
    // the incremental attribute set, so we still get incremental searches.
    this.performSearch(event);
}

WebInspector.performSearch = function(event)
{
    var query = event.target.value;
    var forceSearch = event.keyIdentifier === "Enter";

    if (!query || !query.length || (!forceSearch && query.length < 3)) {
        delete this.currentQuery;

        for (var panelName in this.panels) {
            var panel = this.panels[panelName];
            if (panel.currentQuery && panel.searchCanceled)
                panel.searchCanceled();
            delete panel.currentQuery;
        }

        this.updateSearchMatchesCount();

        return;
    }

    if (query === this.currentPanel.currentQuery && this.currentPanel.currentQuery === this.currentQuery) {
        // When this is the same query and a forced search, jump to the next
        // search result for a good user experience.
        if (forceSearch && this.currentPanel.jumpToNextSearchResult)
            this.currentPanel.jumpToNextSearchResult();
        return;
    }

    this.currentQuery = query;

    this.updateSearchMatchesCount();

    if (!this.currentPanel.performSearch)
        return;

    this.currentPanel.currentQuery = query;
    this.currentPanel.performSearch(query);
}

WebInspector.addNodesToSearchResult = function(nodeIds)
{
    WebInspector.panels.elements.addNodesToSearchResult(nodeIds);
}

WebInspector.updateSearchMatchesCount = function(matches, panel)
{
    if (!panel)
        panel = this.currentPanel;

    panel.currentSearchMatches = matches;

    if (panel !== this.currentPanel)
        return;

    if (!this.currentPanel.currentQuery) {
        document.getElementById("search-results-matches").addStyleClass("hidden");
        return;
    }

    if (matches) {
        if (matches === 1)
            var matchesString = WebInspector.UIString("1 match");
        else
            var matchesString = WebInspector.UIString("%d matches", matches);
    } else
        var matchesString = WebInspector.UIString("Not Found");

    var matchesToolbarElement = document.getElementById("search-results-matches");
    matchesToolbarElement.removeStyleClass("hidden");
    matchesToolbarElement.textContent = matchesString;
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

    var oldText = getContent(element);
    var oldHandleKeyEvent = element.handleKeyEvent;
    var moveDirection = "";

    element.addStyleClass("editing");

    var oldTabIndex = element.tabIndex;
    if (element.tabIndex < 0)
        element.tabIndex = 0;

    function blurEventListener() {
        editingCommitted.call(element);
    }

    function getContent(element) {
        if (element.tagName === "INPUT" && element.type === "text")
            return element.value;
        else
            return element.textContent;
    }

    function cleanUpAfterEditing() {
        delete this.__editing;

        this.removeStyleClass("editing");
        this.tabIndex = oldTabIndex;
        this.scrollTop = 0;
        this.scrollLeft = 0;

        this.handleKeyEvent = oldHandleKeyEvent;
        element.removeEventListener("blur", blurEventListener, false);

        if (element === WebInspector.currentFocusElement || element.isAncestor(WebInspector.currentFocusElement))
            WebInspector.currentFocusElement = WebInspector.previousFocusElement;
    }

    function editingCancelled() {
        if (this.tagName === "INPUT" && this.type === "text")
            this.value = oldText;
        else
            this.textContent = oldText;

        cleanUpAfterEditing.call(this);

        if (cancelledCallback)
            cancelledCallback(this, context);
    }

    function editingCommitted() {
        cleanUpAfterEditing.call(this);

        if (committedCallback)
            committedCallback(this, getContent(this), oldText, context, moveDirection);
    }

    element.handleKeyEvent = function(event) {
        if (oldHandleKeyEvent)
            oldHandleKeyEvent(event);
        if (event.handled)
            return;

        if (event.keyIdentifier === "Enter") {
            editingCommitted.call(element);
            event.preventDefault();
        } else if (event.keyCode === 27) { // Escape key
            editingCancelled.call(element);
            event.preventDefault();
            event.handled = true;
        } else if (event.keyIdentifier === "U+0009") // Tab key
            moveDirection = (event.shiftKey ? "backward" : "forward");
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
    "image/vnd.microsoft.icon":    {2: true},
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
