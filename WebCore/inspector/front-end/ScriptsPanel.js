/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

WebInspector.ScriptsPanel = function()
{
    WebInspector.Panel.call(this);

    this.element.addStyleClass("scripts");

    this.topStatusBar = document.createElement("div");
    this.topStatusBar.className = "status-bar";
    this.topStatusBar.id = "scripts-status-bar";
    this.element.appendChild(this.topStatusBar);

    this.backButton = document.createElement("button");
    this.backButton.className = "status-bar-item";
    this.backButton.id = "scripts-back";
    this.backButton.title = WebInspector.UIString("Show the previous script resource.");
    this.backButton.disabled = true;
    this.backButton.appendChild(document.createElement("img"));
    this.backButton.addEventListener("click", this._goBack.bind(this), false);
    this.topStatusBar.appendChild(this.backButton);

    this.forwardButton = document.createElement("button");
    this.forwardButton.className = "status-bar-item";
    this.forwardButton.id = "scripts-forward";
    this.forwardButton.title = WebInspector.UIString("Show the next script resource.");
    this.forwardButton.disabled = true;
    this.forwardButton.appendChild(document.createElement("img"));
    this.forwardButton.addEventListener("click", this._goForward.bind(this), false);
    this.topStatusBar.appendChild(this.forwardButton);

    this.filesSelectElement = document.createElement("select");
    this.filesSelectElement.className = "status-bar-item";
    this.filesSelectElement.id = "scripts-files";
    this.filesSelectElement.addEventListener("change", this._changeVisibleFile.bind(this), false);
    this.topStatusBar.appendChild(this.filesSelectElement);

    this.functionsSelectElement = document.createElement("select");
    this.functionsSelectElement.className = "status-bar-item";
    this.functionsSelectElement.id = "scripts-functions";

    // FIXME: append the functions select element to the top status bar when it is implemented.
    // this.topStatusBar.appendChild(this.functionsSelectElement);

    this.sidebarButtonsElement = document.createElement("div");
    this.sidebarButtonsElement.id = "scripts-sidebar-buttons";
    this.topStatusBar.appendChild(this.sidebarButtonsElement);

    this.pauseButton = document.createElement("button");
    this.pauseButton.className = "status-bar-item";
    this.pauseButton.id = "scripts-pause";
    this.pauseButton.title = WebInspector.UIString("Pause script execution.");
    this.pauseButton.disabled = true;
    this.pauseButton.appendChild(document.createElement("img"));
    this.pauseButton.addEventListener("click", this._togglePause.bind(this), false);
    this.sidebarButtonsElement.appendChild(this.pauseButton);

    this.stepOverButton = document.createElement("button");
    this.stepOverButton.className = "status-bar-item";
    this.stepOverButton.id = "scripts-step-over";
    this.stepOverButton.title = WebInspector.UIString("Step over next function call.");
    this.stepOverButton.disabled = true;
    this.stepOverButton.addEventListener("click", this._stepOverClicked.bind(this), false);
    this.stepOverButton.appendChild(document.createElement("img"));
    this.sidebarButtonsElement.appendChild(this.stepOverButton);

    this.stepIntoButton = document.createElement("button");
    this.stepIntoButton.className = "status-bar-item";
    this.stepIntoButton.id = "scripts-step-into";
    this.stepIntoButton.title = WebInspector.UIString("Step into next function call.");
    this.stepIntoButton.disabled = true;
    this.stepIntoButton.addEventListener("click", this._stepIntoClicked.bind(this), false);
    this.stepIntoButton.appendChild(document.createElement("img"));
    this.sidebarButtonsElement.appendChild(this.stepIntoButton);

    this.stepOutButton = document.createElement("button");
    this.stepOutButton.className = "status-bar-item";
    this.stepOutButton.id = "scripts-step-out";
    this.stepOutButton.title = WebInspector.UIString("Step out of current function.");
    this.stepOutButton.disabled = true;
    this.stepOutButton.addEventListener("click", this._stepOutClicked.bind(this), false);
    this.stepOutButton.appendChild(document.createElement("img"));
    this.sidebarButtonsElement.appendChild(this.stepOutButton);

    this.debuggerStatusElement = document.createElement("div");
    this.debuggerStatusElement.id = "scripts-debugger-status";
    this.sidebarButtonsElement.appendChild(this.debuggerStatusElement);

    this.viewsContainerElement = document.createElement("div");
    this.viewsContainerElement.id = "script-resource-views";

    this.sidebarElement = document.createElement("div");
    this.sidebarElement.id = "scripts-sidebar";

    this.sidebarResizeElement = document.createElement("div");
    this.sidebarResizeElement.className = "sidebar-resizer-vertical";
    this.sidebarResizeElement.addEventListener("mousedown", this._startSidebarResizeDrag.bind(this), false);

    this.sidebarResizeWidgetElement = document.createElement("div");
    this.sidebarResizeWidgetElement.id = "scripts-sidebar-resizer-widget";
    this.sidebarResizeWidgetElement.addEventListener("mousedown", this._startSidebarResizeDrag.bind(this), false);
    this.topStatusBar.appendChild(this.sidebarResizeWidgetElement);

    this.sidebarPanes = {};
    this.sidebarPanes.callstack = new WebInspector.CallStackSidebarPane();
    this.sidebarPanes.scopechain = new WebInspector.ScopeChainSidebarPane();
    this.sidebarPanes.breakpoints = new WebInspector.BreakpointsSidebarPane();

    for (var pane in this.sidebarPanes)
        this.sidebarElement.appendChild(this.sidebarPanes[pane].element);

    // FIXME: remove the following line of code when the Breakpoints pane has content.
    this.sidebarElement.removeChild(this.sidebarPanes.breakpoints.element);

    this.sidebarPanes.callstack.expanded = true;
    this.sidebarPanes.callstack.addEventListener("call frame selected", this._callFrameSelected, this);

    this.sidebarPanes.scopechain.expanded = true;

    var panelEnablerHeading = WebInspector.UIString("You need to enable debugging before you can use the Scripts panel.");
    var panelEnablerDisclaimer = WebInspector.UIString("Enabling debugging will make scripts run slower.");
    var panelEnablerButton = WebInspector.UIString("Enable Debugging");

    this.panelEnablerView = new WebInspector.PanelEnablerView("scripts", panelEnablerHeading, panelEnablerDisclaimer, panelEnablerButton);
    this.panelEnablerView.addEventListener("enable clicked", this._enableDebugging, this);

    this.element.appendChild(this.panelEnablerView.element);
    this.element.appendChild(this.viewsContainerElement);
    this.element.appendChild(this.sidebarElement);
    this.element.appendChild(this.sidebarResizeElement);

    this.enableToggleButton = document.createElement("button");
    this.enableToggleButton.className = "enable-toggle-status-bar-item status-bar-item";
    this.enableToggleButton.addEventListener("click", this._toggleDebugging.bind(this), false);

    this.pauseOnExceptionButton = document.createElement("button");
    this.pauseOnExceptionButton.id = "scripts-pause-on-exceptions-status-bar-item";
    this.pauseOnExceptionButton.className = "status-bar-item";
    this.pauseOnExceptionButton.addEventListener("click", this._togglePauseOnExceptions.bind(this), false);

    this._breakpointsURLMap = {};

    this._shortcuts = {};

    var isMac = InspectorController.platform().indexOf("mac-") === 0;
    var platformSpecificModifier = isMac ? WebInspector.KeyboardShortcut.Modifiers.Meta : WebInspector.KeyboardShortcut.Modifiers.Ctrl;

    // Continue.
    var handler = this.pauseButton.click.bind(this.pauseButton);
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.F8);
    this._shortcuts[shortcut] = handler;
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.Slash, platformSpecificModifier);
    this._shortcuts[shortcut] = handler;

    // Step over.
    var handler = this.stepOverButton.click.bind(this.stepOverButton);
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.F10);
    this._shortcuts[shortcut] = handler;
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.SingleQuote, platformSpecificModifier);
    this._shortcuts[shortcut] = handler;

    // Step into.
    var handler = this.stepIntoButton.click.bind(this.stepIntoButton);
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.F11);
    this._shortcuts[shortcut] = handler;
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.Semicolon, platformSpecificModifier);
    this._shortcuts[shortcut] = handler;

    // Step out.
    var handler = this.stepOutButton.click.bind(this.stepOutButton);
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.F11, WebInspector.KeyboardShortcut.Modifiers.Shift);
    this._shortcuts[shortcut] = handler;
    var shortcut = WebInspector.KeyboardShortcut.makeKey(WebInspector.KeyboardShortcut.KeyCodes.Semicolon, WebInspector.KeyboardShortcut.Modifiers.Shift, platformSpecificModifier);
    this._shortcuts[shortcut] = handler;

    this.reset();
}

WebInspector.ScriptsPanel.prototype = {
    toolbarItemClass: "scripts",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Scripts");
    },

    get statusBarItems()
    {
        return [this.enableToggleButton, this.pauseOnExceptionButton];
    },

    get paused()
    {
        return this._paused;
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        this.sidebarResizeElement.style.right = (this.sidebarElement.offsetWidth - 3) + "px";

        if (this.visibleView) {
            if (this.visibleView instanceof WebInspector.ResourceView)
                this.visibleView.headersVisible = false;
            this.visibleView.show(this.viewsContainerElement);
        }

        // Hide any views that are visible that are not this panel's current visible view.
        // This can happen when a ResourceView is visible in the Resources panel then switched
        // to the this panel.
        for (var sourceID in this._sourceIDMap) {
            var scriptOrResource = this._sourceIDMap[sourceID];
            var view = this._sourceViewForScriptOrResource(scriptOrResource);
            if (!view || view === this.visibleView)
                continue;
            view.visible = false;
        }
        if (this._attachDebuggerWhenShown) {
            InspectorController.enableDebuggerFromFrontend(false);
            delete this._attachDebuggerWhenShown;
        }
    },

    get searchableViews()
    {
        var views = [];

        const visibleView = this.visibleView;
        if (visibleView && visibleView.performSearch) {
            visibleView.alreadySearching = true;
            views.push(visibleView);
        }

        for (var sourceID in this._sourceIDMap) {
            var scriptOrResource = this._sourceIDMap[sourceID];
            var view = this._sourceViewForScriptOrResource(scriptOrResource);
            if (!view || !view.performSearch || view.alreadySearching)
                continue;

            view.alreadySearching = true;
            views.push(view);
        }

        for (var i = 0; i < views.length; ++i)
            delete views[i].alreadySearching;

        return views;
    },

    addScript: function(sourceID, sourceURL, source, startingLine, errorLine, errorMessage)
    {
        var script = new WebInspector.Script(sourceID, sourceURL, source, startingLine, errorLine, errorMessage);

        if (sourceURL in WebInspector.resourceURLMap) {
            var resource = WebInspector.resourceURLMap[sourceURL];
            resource.addScript(script);
        }

        if (sourceURL in this._breakpointsURLMap && sourceID) {
            var breakpoints = this._breakpointsURLMap[sourceURL];
            var breakpointsLength = breakpoints.length;
            for (var i = 0; i < breakpointsLength; ++i) {
                var breakpoint = breakpoints[i];
                if (startingLine <= breakpoint.line) {
                    breakpoint.sourceID = sourceID;
                    if (breakpoint.enabled)
                        InspectorController.addBreakpoint(breakpoint.sourceID, breakpoint.line);
                }
            }
        }

        if (sourceID)
            this._sourceIDMap[sourceID] = (resource || script);

        this._addScriptToFilesMenu(script);
    },

    addBreakpoint: function(breakpoint)
    {
        this.sidebarPanes.breakpoints.addBreakpoint(breakpoint);

        var sourceFrame;
        if (breakpoint.url) {
            if (!(breakpoint.url in this._breakpointsURLMap))
                this._breakpointsURLMap[breakpoint.url] = [];
            this._breakpointsURLMap[breakpoint.url].unshift(breakpoint);

            if (breakpoint.url in WebInspector.resourceURLMap) {
                var resource = WebInspector.resourceURLMap[breakpoint.url];
                sourceFrame = this._sourceFrameForScriptOrResource(resource);
            }
        }

        if (breakpoint.sourceID && !sourceFrame) {
            var object = this._sourceIDMap[breakpoint.sourceID]
            sourceFrame = this._sourceFrameForScriptOrResource(object);
        }

        if (sourceFrame)
            sourceFrame.addBreakpoint(breakpoint);
    },

    removeBreakpoint: function(breakpoint)
    {
        this.sidebarPanes.breakpoints.removeBreakpoint(breakpoint);

        var sourceFrame;
        if (breakpoint.url && breakpoint.url in this._breakpointsURLMap) {
            var breakpoints = this._breakpointsURLMap[breakpoint.url];
            breakpoints.remove(breakpoint);
            if (!breakpoints.length)
                delete this._breakpointsURLMap[breakpoint.url];

            if (breakpoint.url in WebInspector.resourceURLMap) {
                var resource = WebInspector.resourceURLMap[breakpoint.url];
                sourceFrame = this._sourceFrameForScriptOrResource(resource);
            }
        }

        if (breakpoint.sourceID && !sourceFrame) {
            var object = this._sourceIDMap[breakpoint.sourceID]
            sourceFrame = this._sourceFrameForScriptOrResource(object);
        }

        if (sourceFrame)
            sourceFrame.removeBreakpoint(breakpoint);
    },

    evaluateInSelectedCallFrame: function(code, updateInterface, callback)
    {
        var selectedCallFrame = this.sidebarPanes.callstack.selectedCallFrame;
        if (!this._paused || !selectedCallFrame)
            return;

        if (typeof updateInterface === "undefined")
            updateInterface = true;

        var self = this;
        function updatingCallbackWrapper(result)
        {
            callback(result);
            if (updateInterface)
                self.sidebarPanes.scopechain.update(selectedCallFrame);
        }        
        this.doEvalInCallFrame(selectedCallFrame, code, updatingCallbackWrapper);
    },

    doEvalInCallFrame: function(callFrame, code, callback)
    {
        var panel = this;
        function delayedEvaluation()
        {
            if (!code) {
                // Evaluate into properties in scope of the selected call frame.
                callback(panel._variablesInScope(callFrame));
                return;
            }
            try {
                callback(callFrame.evaluate(code));
            } catch (e) {
                callback(e, true);
            }
        }
        setTimeout(delayedEvaluation, 0);
    },

    _variablesInScope: function(callFrame)
    {
        var result = {};
        var scopeChain = callFrame.scopeChain;
        for (var i = 0; i < scopeChain.length; ++i) {
            var scopeObject = scopeChain[i];
            for (var property in scopeObject)
                result[property] = true;
        }
        return result;
    },

    debuggerPaused: function()
    {
        this._paused = true;
        this._waitingToPause = false;
        this._stepping = false;

        this._updateDebuggerButtons();

        var callStackPane = this.sidebarPanes.callstack;
        var currentFrame = InspectorController.currentCallFrame();
        callStackPane.update(currentFrame, this._sourceIDMap);
        callStackPane.selectedCallFrame = currentFrame;

        WebInspector.currentPanel = this;
        window.focus();
    },

    debuggerResumed: function()
    {
        this._paused = false;
        this._waitingToPause = false;
        this._stepping = false;

        this._clearInterface();
    },

    attachDebuggerWhenShown: function()
    {
        if (this.element.parentElement) {
            InspectorController.enableDebuggerFromFrontend(false);
        } else {
            this._attachDebuggerWhenShown = true;
        }
    },

    debuggerWasEnabled: function()
    {
        this.reset();
    },

    debuggerWasDisabled: function()
    {
        this.reset();
    },

    reset: function()
    {
        this.visibleView = null;

        delete this.currentQuery;
        this.searchCanceled();

        if (!InspectorController.debuggerEnabled()) {
            this._paused = false;
            this._waitingToPause = false;
            this._stepping = false;
        }

        this._clearInterface();

        this._backForwardList = [];
        this._currentBackForwardIndex = -1;
        this._updateBackAndForwardButtons();

        this._scriptsForURLsInFilesSelect = {};
        this.filesSelectElement.removeChildren();
        this.functionsSelectElement.removeChildren();
        this.viewsContainerElement.removeChildren();

        if (this._sourceIDMap) {
            for (var sourceID in this._sourceIDMap) {
                var object = this._sourceIDMap[sourceID];
                if (object instanceof WebInspector.Resource)
                    object.removeAllScripts();
            }
        }

        this._sourceIDMap = {};
    },

    get visibleView()
    {
        return this._visibleView;
    },

    set visibleView(x)
    {
        if (this._visibleView === x)
            return;

        if (this._visibleView)
            this._visibleView.hide();

        this._visibleView = x;

        if (x)
            x.show(this.viewsContainerElement);
    },

    canShowResource: function(resource)
    {
        return resource && resource.scripts.length && InspectorController.debuggerEnabled();
    },

    showScript: function(script, line)
    {
        this._showScriptOrResource(script, line, true);
    },

    showResource: function(resource, line)
    {
        this._showScriptOrResource(resource, line, true);
    },

    showView: function(view)
    {
        if (!view)
            return;
        this._showScriptOrResource((view.resource || view.script));
    },

    handleKeyEvent: function(event)
    {
        var shortcut = WebInspector.KeyboardShortcut.makeKeyFromEvent(event);
        var handler = this._shortcuts[shortcut];
        if (handler) {
            handler(event);
            event.preventDefault();
            event.handled = true;
        } else {
            this.sidebarPanes.callstack.handleKeyEvent(event);
        }
    },

    scriptViewForScript: function(script)
    {
        if (!script)
            return null;
        if (!script._scriptView)
            script._scriptView = new WebInspector.ScriptView(script);
        return script._scriptView;
    },

    sourceFrameForScript: function(script)
    {
        var view = this.scriptViewForScript(script);
        if (!view)
            return null;

        // Setting up the source frame requires that we be attached.
        if (!this.element.parentNode)
            this.attach();

        view.setupSourceFrameIfNeeded();
        return view.sourceFrame;
    },

    _sourceViewForScriptOrResource: function(scriptOrResource)
    {
        if (scriptOrResource instanceof WebInspector.Resource) {
            if (!WebInspector.panels.resources)
                return null;
            return WebInspector.panels.resources.resourceViewForResource(scriptOrResource);
        }
        if (scriptOrResource instanceof WebInspector.Script)
            return this.scriptViewForScript(scriptOrResource);
    },

    _sourceFrameForScriptOrResource: function(scriptOrResource)
    {
        if (scriptOrResource instanceof WebInspector.Resource) {
            if (!WebInspector.panels.resources)
                return null;
            return WebInspector.panels.resources.sourceFrameForResource(scriptOrResource);
        }
        if (scriptOrResource instanceof WebInspector.Script)
            return this.sourceFrameForScript(scriptOrResource);
    },

    _showScriptOrResource: function(scriptOrResource, line, shouldHighlightLine, fromBackForwardAction)
    {
        if (!scriptOrResource)
            return;

        var view;
        if (scriptOrResource instanceof WebInspector.Resource) {
            if (!WebInspector.panels.resources)
                return null;
            view = WebInspector.panels.resources.resourceViewForResource(scriptOrResource);
            view.headersVisible = false;

            if (scriptOrResource.url in this._breakpointsURLMap) {
                var sourceFrame = this._sourceFrameForScriptOrResource(scriptOrResource);
                if (sourceFrame && !sourceFrame.breakpoints.length) {
                    var breakpoints = this._breakpointsURLMap[scriptOrResource.url];
                    var breakpointsLength = breakpoints.length;
                    for (var i = 0; i < breakpointsLength; ++i)
                        sourceFrame.addBreakpoint(breakpoints[i]);
                }
            }
        } else if (scriptOrResource instanceof WebInspector.Script)
            view = this.scriptViewForScript(scriptOrResource);

        if (!view)
            return;

        if (!fromBackForwardAction) {
            var oldIndex = this._currentBackForwardIndex;
            if (oldIndex >= 0)
                this._backForwardList.splice(oldIndex + 1, this._backForwardList.length - oldIndex);

            // Check for a previous entry of the same object in _backForwardList.
            // If one is found, remove it and update _currentBackForwardIndex to match.
            var previousEntryIndex = this._backForwardList.indexOf(scriptOrResource);
            if (previousEntryIndex !== -1) {
                this._backForwardList.splice(previousEntryIndex, 1);
                --this._currentBackForwardIndex;
            }

            this._backForwardList.push(scriptOrResource);
            ++this._currentBackForwardIndex;

            this._updateBackAndForwardButtons();
        }

        this.visibleView = view;

        if (line) {
            if (view.revealLine)
                view.revealLine(line);
            if (view.highlightLine && shouldHighlightLine)
                view.highlightLine(line);
        }

        var option;
        if (scriptOrResource instanceof WebInspector.Script) {
            option = scriptOrResource.filesSelectOption;
            console.assert(option);
        } else {
            var url = scriptOrResource.url;
            var script = this._scriptsForURLsInFilesSelect[url];
            if (script)
               option = script.filesSelectOption;
        }

        if (option)
            this.filesSelectElement.selectedIndex = option.index;
    },

    _addScriptToFilesMenu: function(script)
    {
        if (script.resource && this._scriptsForURLsInFilesSelect[script.sourceURL])
            return;

        this._scriptsForURLsInFilesSelect[script.sourceURL] = script;

        var select = this.filesSelectElement;

        var option = document.createElement("option");
        option.representedObject = (script.resource || script);
        option.text = (script.sourceURL ? WebInspector.displayNameForURL(script.sourceURL) : WebInspector.UIString("(program)"));

        function optionCompare(a, b)
        {
            var aTitle = a.text.toLowerCase();
            var bTitle = b.text.toLowerCase();
            if (aTitle < bTitle)
                return -1;
            else if (aTitle > bTitle)
                return 1;

            var aSourceID = a.representedObject.sourceID;
            var bSourceID = b.representedObject.sourceID;
            if (aSourceID < bSourceID)
                return -1;
            else if (aSourceID > bSourceID)
                return 1;
            return 0;
        }

        var insertionIndex = insertionIndexForObjectInListSortedByFunction(option, select.childNodes, optionCompare);
        if (insertionIndex < 0)
            select.appendChild(option);
        else
            select.insertBefore(option, select.childNodes.item(insertionIndex));

        script.filesSelectOption = option;

        // Call _showScriptOrResource if the option we just appended ended up being selected.
        // This will happen for the first item added to the menu.
        if (select.options[select.selectedIndex] === option)
            this._showScriptOrResource(option.representedObject);
    },

    _clearCurrentExecutionLine: function()
    {
        if (this._executionSourceFrame)
            this._executionSourceFrame.executionLine = 0;
        delete this._executionSourceFrame;
    },

    _callFrameSelected: function()
    {
        this._clearCurrentExecutionLine();

        var callStackPane = this.sidebarPanes.callstack;
        var currentFrame = callStackPane.selectedCallFrame;
        if (!currentFrame)
            return;

        this.sidebarPanes.scopechain.update(currentFrame);

        var scriptOrResource = this._sourceIDMap[currentFrame.sourceID];
        this._showScriptOrResource(scriptOrResource, currentFrame.line);

        this._executionSourceFrame = this._sourceFrameForScriptOrResource(scriptOrResource);
        if (this._executionSourceFrame)
            this._executionSourceFrame.executionLine = currentFrame.line;
    },

    _changeVisibleFile: function(event)
    {
        var select = this.filesSelectElement;
        this._showScriptOrResource(select.options[select.selectedIndex].representedObject);
    },

    _startSidebarResizeDrag: function(event)
    {
        WebInspector.elementDragStart(this.sidebarElement, this._sidebarResizeDrag.bind(this), this._endSidebarResizeDrag.bind(this), event, "col-resize");

        if (event.target === this.sidebarResizeWidgetElement)
            this._dragOffset = (event.target.offsetWidth - (event.pageX - event.target.totalOffsetLeft));
        else
            this._dragOffset = 0;
    },

    _endSidebarResizeDrag: function(event)
    {
        WebInspector.elementDragEnd(event);

        delete this._dragOffset;
    },

    _sidebarResizeDrag: function(event)
    {
        var x = event.pageX + this._dragOffset;
        var newWidth = Number.constrain(window.innerWidth - x, Preferences.minScriptsSidebarWidth, window.innerWidth * 0.66);

        this.sidebarElement.style.width = newWidth + "px";
        this.sidebarButtonsElement.style.width = newWidth + "px";
        this.viewsContainerElement.style.right = newWidth + "px";
        this.sidebarResizeWidgetElement.style.right = newWidth + "px";
        this.sidebarResizeElement.style.right = (newWidth - 3) + "px";

        event.preventDefault();
    },

    _updatePauseOnExceptionsButton: function()
    {
        if (InspectorController.pauseOnExceptions()) {
            this.pauseOnExceptionButton.title = WebInspector.UIString("Don't pause on exceptions.");
            this.pauseOnExceptionButton.addStyleClass("toggled-on");
        } else {
            this.pauseOnExceptionButton.title = WebInspector.UIString("Pause on exceptions.");
            this.pauseOnExceptionButton.removeStyleClass("toggled-on");
        }
    },

    _updateDebuggerButtons: function()
    {
        if (InspectorController.debuggerEnabled()) {
            this.enableToggleButton.title = WebInspector.UIString("Debugging enabled. Click to disable.");
            this.enableToggleButton.addStyleClass("toggled-on");
            this.pauseOnExceptionButton.removeStyleClass("hidden");
            this.panelEnablerView.visible = false;
        } else {
            this.enableToggleButton.title = WebInspector.UIString("Debugging disabled. Click to enable.");
            this.enableToggleButton.removeStyleClass("toggled-on");
            this.pauseOnExceptionButton.addStyleClass("hidden");
            this.panelEnablerView.visible = true;
        }

        this._updatePauseOnExceptionsButton();

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

    _updateBackAndForwardButtons: function()
    {
        this.backButton.disabled = this._currentBackForwardIndex <= 0;
        this.forwardButton.disabled = this._currentBackForwardIndex >= (this._backForwardList.length - 1);
    },

    _clearInterface: function()
    {
        this.sidebarPanes.callstack.update(null);
        this.sidebarPanes.scopechain.update(null);

        this._clearCurrentExecutionLine();
        this._updateDebuggerButtons();
    },

    _goBack: function()
    {
        if (this._currentBackForwardIndex <= 0) {
            console.error("Can't go back from index " + this._currentBackForwardIndex);
            return;
        }

        this._showScriptOrResource(this._backForwardList[--this._currentBackForwardIndex], null, false, true);
        this._updateBackAndForwardButtons();
    },

    _goForward: function()
    {
        if (this._currentBackForwardIndex >= this._backForwardList.length - 1) {
            console.error("Can't go forward from index " + this._currentBackForwardIndex);
            return;
        }

        this._showScriptOrResource(this._backForwardList[++this._currentBackForwardIndex], null, false, true);
        this._updateBackAndForwardButtons();
    },

    _enableDebugging: function()
    {
        if (InspectorController.debuggerEnabled())
            return;
        this._toggleDebugging(this.panelEnablerView.alwaysEnabled);
    },

    _toggleDebugging: function(optionalAlways)
    {
        this._paused = false;
        this._waitingToPause = false;
        this._stepping = false;

        if (InspectorController.debuggerEnabled())
            InspectorController.disableDebugger(true);
        else
            InspectorController.enableDebuggerFromFrontend(!!optionalAlways);
    },

    _togglePauseOnExceptions: function()
    {
        InspectorController.setPauseOnExceptions(!InspectorController.pauseOnExceptions());
        this._updatePauseOnExceptionsButton();
    },

    _togglePause: function()
    {
        if (this._paused) {
            this._paused = false;
            this._waitingToPause = false;
            InspectorController.resumeDebugger();
        } else {
            this._stepping = false;
            this._waitingToPause = true;
            InspectorController.pauseInDebugger();
        }

        this._clearInterface();
    },

    _stepOverClicked: function()
    {
        this._paused = false;
        this._stepping = true;

        this._clearInterface();

        InspectorController.stepOverStatementInDebugger();
    },

    _stepIntoClicked: function()
    {
        this._paused = false;
        this._stepping = true;

        this._clearInterface();

        InspectorController.stepIntoStatementInDebugger();
    },

    _stepOutClicked: function()
    {
        this._paused = false;
        this._stepping = true;

        this._clearInterface();

        InspectorController.stepOutOfFunctionInDebugger();
    }
}

WebInspector.ScriptsPanel.prototype.__proto__ = WebInspector.Panel.prototype;
