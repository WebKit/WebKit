/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.DebuggerSidebarPanel = function()
{
    WebInspector.NavigationSidebarPanel.call(this, "debugger", WebInspector.UIString("Debugger"), "Images/NavigationItemBug.svg", "3", true);

    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceChanged, this);
    WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceAdded, this);

    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointsEnabledDidChange, this._breakpointsEnabledDidChange, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.CallFramesDidChange, this._debuggerCallFramesDidChange, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointAdded, this._breakpointAdded, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointRemoved, this._breakpointRemoved, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptAdded, this._scriptAdded, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptsCleared, this._scriptsCleared, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
    WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ActiveCallFrameDidChange, this._debuggerActiveCallFrameDidChange, this);

    this._toggleBreakpointsKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "Y", this._breakpointsToggleButtonClicked.bind(this));
    this.pauseOrResumeKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Control | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "Y", this._debuggerPauseResumeButtonClicked.bind(this));
    this._stepOverKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.F6, this._debuggerStepOverButtonClicked.bind(this));
    this._stepIntoKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.F7, this._debuggerStepIntoButtonClicked.bind(this));
    this._stepOutKeyboardShortcut = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.F8, this._debuggerStepOutButtonClicked.bind(this));

    this.pauseOrResumeAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Backslash, this._debuggerPauseResumeButtonClicked.bind(this));
    this._stepOverAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.SingleQuote, this._debuggerStepOverButtonClicked.bind(this));
    this._stepIntoAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Semicolon, this._debuggerStepIntoButtonClicked.bind(this));
    this._stepOutAlternateKeyboardShortcut = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.Shift | WebInspector.KeyboardShortcut.Modifier.CommandOrControl, WebInspector.KeyboardShortcut.Key.Semicolon, this._debuggerStepOutButtonClicked.bind(this));

    this._navigationBar = new WebInspector.NavigationBar;
    this.element.appendChild(this._navigationBar.element);

    var breakpointsImage, pauseImage, resumeImage, stepOverImage, stepIntoImage, stepOutImage;
    if (WebInspector.Platform.isLegacyMacOS) {
        breakpointsImage = {src: "Images/Legacy/Breakpoints.svg", width: 16, height: 16};
        pauseImage = {src: "Images/Legacy/Pause.svg", width: 16, height: 16};
        resumeImage = {src: "Images/Legacy/Resume.svg", width: 16, height: 16};
        stepOverImage = {src: "Images/Legacy/StepOver.svg", width: 16, height: 16};
        stepIntoImage = {src: "Images/Legacy/StepInto.svg", width: 16, height: 16};
        stepOutImage = {src: "Images/Legacy/StepOut.svg", width: 16, height: 16};
    } else {
        breakpointsImage = {src: "Images/Breakpoints.svg", width: 15, height: 15};
        pauseImage = {src: "Images/Pause.svg", width: 15, height: 15};
        resumeImage = {src: "Images/Resume.svg", width: 15, height: 15};
        stepOverImage = {src: "Images/StepOver.svg", width: 15, height: 15};
        stepIntoImage = {src: "Images/StepInto.svg", width: 15, height: 15};
        stepOutImage = {src: "Images/StepOut.svg", width: 15, height: 15};
    }

    var toolTip = WebInspector.UIString("Enable all breakpoints (%s)").format(this._toggleBreakpointsKeyboardShortcut.displayName);
    var altToolTip = WebInspector.UIString("Disable all breakpoints (%s)").format(this._toggleBreakpointsKeyboardShortcut.displayName);

    this._debuggerBreakpointsButtonItem = new WebInspector.ActivateButtonNavigationItem("debugger-breakpoints", toolTip, altToolTip, breakpointsImage.src, breakpointsImage.width, breakpointsImage.height);
    this._debuggerBreakpointsButtonItem.activated = WebInspector.debuggerManager.breakpointsEnabled;
    this._debuggerBreakpointsButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._breakpointsToggleButtonClicked, this);
    this._navigationBar.addNavigationItem(this._debuggerBreakpointsButtonItem);

    toolTip = WebInspector.UIString("Pause script execution (%s or %s)").format(this.pauseOrResumeKeyboardShortcut.displayName, this.pauseOrResumeAlternateKeyboardShortcut.displayName);
    altToolTip = WebInspector.UIString("Continue script execution (%s or %s)").format(this.pauseOrResumeKeyboardShortcut.displayName, this.pauseOrResumeAlternateKeyboardShortcut.displayName);

    this._debuggerPauseResumeButtonItem = new WebInspector.ToggleButtonNavigationItem("debugger-pause-resume", toolTip, altToolTip, pauseImage.src, resumeImage.src, pauseImage.width, pauseImage.height);
    this._debuggerPauseResumeButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._debuggerPauseResumeButtonClicked, this);
    this._navigationBar.addNavigationItem(this._debuggerPauseResumeButtonItem);

    this._debuggerStepOverButtonItem = new WebInspector.ButtonNavigationItem("debugger-step-over", WebInspector.UIString("Step over (%s or %s)").format(this._stepOverKeyboardShortcut.displayName, this._stepOverAlternateKeyboardShortcut.displayName), stepOverImage.src, stepOverImage.width, stepOverImage.height);
    this._debuggerStepOverButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._debuggerStepOverButtonClicked, this);
    this._debuggerStepOverButtonItem.enabled = false;
    this._navigationBar.addNavigationItem(this._debuggerStepOverButtonItem);

    this._debuggerStepIntoButtonItem = new WebInspector.ButtonNavigationItem("debugger-step-into", WebInspector.UIString("Step into (%s or %s)").format(this._stepIntoKeyboardShortcut.displayName, this._stepIntoAlternateKeyboardShortcut.displayName), stepIntoImage.src, stepIntoImage.width, stepIntoImage.height);
    this._debuggerStepIntoButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._debuggerStepIntoButtonClicked, this);
    this._debuggerStepIntoButtonItem.enabled = false;
    this._navigationBar.addNavigationItem(this._debuggerStepIntoButtonItem);

    this._debuggerStepOutButtonItem = new WebInspector.ButtonNavigationItem("debugger-step-out", WebInspector.UIString("Step out (%s or %s)").format(this._stepOutKeyboardShortcut.displayName, this._stepOutAlternateKeyboardShortcut.displayName), stepOutImage.src, stepOutImage.width, stepOutImage.height);
    this._debuggerStepOutButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._debuggerStepOutButtonClicked, this);
    this._debuggerStepOutButtonItem.enabled = false;
    this._navigationBar.addNavigationItem(this._debuggerStepOutButtonItem);

    // Add this offset-sections class name so the sticky headers don't overlap the navigation bar.
    this.element.classList.add(WebInspector.DebuggerSidebarPanel.OffsetSectionsStyleClassName);

    this._globalBreakpointsFolderTreeElement = new WebInspector.FolderTreeElement(WebInspector.UIString("Global Breakpoints"), null, WebInspector.DebuggerSidebarPanel.GlobalIconStyleClassName);
    this._allExceptionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.allExceptionsBreakpoint, WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName, WebInspector.UIString("All Exceptions"));
    this._allUncaughtExceptionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.allUncaughtExceptionsBreakpoint, WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName, WebInspector.UIString("All Uncaught Exceptions"));

    this.filterBar.placeholder = WebInspector.UIString("Filter Breakpoint List");

    this._breakpointsContentTreeOutline = this.contentTreeOutline;
    this._breakpointsContentTreeOutline.onselect = this._treeElementSelected.bind(this);
    this._breakpointsContentTreeOutline.ondelete = this._breakpointTreeOutlineDeleteTreeElement.bind(this);
    this._breakpointsContentTreeOutline.oncontextmenu = this._breakpointTreeOutlineContextMenuTreeElement.bind(this);

    this._breakpointsContentTreeOutline.appendChild(this._globalBreakpointsFolderTreeElement);
    this._globalBreakpointsFolderTreeElement.appendChild(this._allExceptionsBreakpointTreeElement);
    this._globalBreakpointsFolderTreeElement.appendChild(this._allUncaughtExceptionsBreakpointTreeElement);
    this._globalBreakpointsFolderTreeElement.expand();

    var breakpointsRow = new WebInspector.DetailsSectionRow;
    breakpointsRow.element.appendChild(this._breakpointsContentTreeOutline.element);

    var breakpointsGroup = new WebInspector.DetailsSectionGroup([breakpointsRow]);
    var breakpointsSection = new WebInspector.DetailsSection("scripts", WebInspector.UIString("Scripts"), [breakpointsGroup]);
    this.contentElement.appendChild(breakpointsSection.element);

    this._callStackContentTreeOutline = this.createContentTreeOutline(true);
    this._callStackContentTreeOutline.onselect = this._treeElementSelected.bind(this);

    this._callStackRow = new WebInspector.DetailsSectionRow(WebInspector.UIString("No Call Frames"));
    this._callStackRow.showEmptyMessage();

    var callStackGroup = new WebInspector.DetailsSectionGroup([this._callStackRow]);
    this._callStackSection = new WebInspector.DetailsSection("call-stack", WebInspector.UIString("Call Stack"), [callStackGroup]);

    this._pauseReasonTreeOutline = null;

    this._pauseReasonLinkContainerElement = document.createElement("span");
    this._pauseReasonTextRow = new WebInspector.DetailsSectionTextRow;
    this._pauseReasonGroup = new WebInspector.DetailsSectionGroup([this._pauseReasonTextRow]);
    this._pauseReasonSection = new WebInspector.DetailsSection("paused-reason", null, [this._pauseReasonGroup], this._pauseReasonLinkContainerElement);
    this._pauseReasonSection.title = WebInspector.UIString("Pause Reason");

    WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.DisplayLocationDidChange, this._breakpointDisplayLocationDidChange, this);
};

WebInspector.DebuggerSidebarPanel.OffsetSectionsStyleClassName = "offset-sections";
WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName = "paused";
WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName = "breakpoint-exception-icon";
WebInspector.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName = "breakpoint-paused-icon";
WebInspector.DebuggerSidebarPanel.GlobalIconStyleClassName = "global-breakpoints-icon";

WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey = "debugger-sidebar-panel-all-exceptions-breakpoint";
WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey = "debugger-sidebar-panel-all-uncaught-exceptions-breakpoint";

WebInspector.DebuggerSidebarPanel.prototype = {
    constructor: WebInspector.DebuggerSidebarPanel,
    __proto__: WebInspector.NavigationSidebarPanel.prototype,

    // Public

    get hasSelectedElement()
    {
        return !!this._breakpointsContentTreeOutline.selectedTreeElement
            || !!this._callStackContentTreeOutline.selectedTreeElement
            || (this._pauseReasonTreeOutline && !!this._pauseReasonTreeOutline.selectedTreeElement);
    },

    showDefaultContentView: function()
    {
        WebInspector.resourceSidebarPanel.showDefaultContentView();
    },

    treeElementForRepresentedObject: function(representedObject)
    {
        // The main resource is used as the representedObject instead of Frame in our tree.
        if (representedObject instanceof WebInspector.Frame)
            representedObject = representedObject.mainResource;

        return this.contentTreeOutline.getCachedTreeElement(representedObject);
    },

    // Protected

    saveStateToCookie: function(cookie)
    {
        console.assert(cookie);

        var selectedTreeElement = this._breakpointsContentTreeOutline.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        var representedObject = selectedTreeElement.representedObject;

        if (representedObject === WebInspector.debuggerManager.allExceptionsBreakpoint)
            cookie[WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey] = true;

        if (representedObject === WebInspector.debuggerManager.allUncaughtExceptionsBreakpoint)
            cookie[WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey] = true;

        WebInspector.NavigationSidebarPanel.prototype.saveStateToCookie.call(this, cookie);
    },

    restoreStateFromCookie: function(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // Eagerly resolve the special breakpoints; otherwise, use the default behavior.
        if (cookie[WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey])
            this._allExceptionsBreakpointTreeElement.revealAndSelect();
        else if (cookie[WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey])
            this._allUncaughtExceptionsBreakpointTreeElement.revealAndSelect();
        else
            WebInspector.NavigationSidebarPanel.prototype.restoreStateFromCookie.call(this, cookie, relaxedMatchDelay);
    },

    // Private

    _debuggerPauseResumeButtonClicked: function(event)
    {
        if (WebInspector.debuggerManager.paused)
            WebInspector.debuggerManager.resume();
        else {
            this._debuggerPauseResumeButtonItem.enabled = false;
            WebInspector.debuggerManager.pause();
        }
    },

    _debuggerStepOverButtonClicked: function(event)
    {
        WebInspector.debuggerManager.stepOver();
    },

    _debuggerStepIntoButtonClicked: function(event)
    {
        WebInspector.debuggerManager.stepInto();
    },

    _debuggerStepOutButtonClicked: function(event)
    {
        WebInspector.debuggerManager.stepOut();
    },

    _debuggerDidPause: function(event)
    {
        this.contentElement.insertBefore(this._callStackSection.element, this.contentElement.firstChild);
        if (this._updatePauseReason())
            this.contentElement.insertBefore(this._pauseReasonSection.element, this.contentElement.firstChild);

        this._debuggerPauseResumeButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.toggled = true;
        this._debuggerStepOverButtonItem.enabled = true;
        this._debuggerStepIntoButtonItem.enabled = true;

        this.element.classList.add(WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName);
    },

    _debuggerDidResume: function(event)
    {
        this._callStackSection.element.remove();
        this._pauseReasonSection.element.remove();

        this._debuggerPauseResumeButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.toggled = false;
        this._debuggerStepOverButtonItem.enabled = false;
        this._debuggerStepIntoButtonItem.enabled = false;
        this._debuggerStepOutButtonItem.enabled = false;

        this.element.classList.remove(WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName);
    },

    _breakpointsEnabledDidChange: function(event)
    {
        this._debuggerBreakpointsButtonItem.activated = WebInspector.debuggerManager.breakpointsEnabled;
    },

    _breakpointsToggleButtonClicked: function(event)
    {
        WebInspector.debuggerManager.breakpointsEnabled = !this._debuggerBreakpointsButtonItem.activated;
    },

    _addBreakpoint: function(breakpoint, sourceCode)
    {
        var sourceCode = breakpoint.sourceCodeLocation.displaySourceCode;
        if (!sourceCode)
            return null;

        var parentTreeElement = this._addTreeElementForSourceCodeToContentTreeOutline(sourceCode);

        // Mark disabled breakpoints as resolved if there is source code loaded with that URL.
        // This gives the illusion the breakpoint was resolved, but since we don't send disabled
        // breakpoints to the backend we don't know for sure. If the user enables the breakpoint
        // it will be resolved properly.
        if (breakpoint.disabled)
            breakpoint.resolved = true;

        var breakpointTreeElement = new WebInspector.BreakpointTreeElement(breakpoint);
        parentTreeElement.insertChild(breakpointTreeElement, insertionIndexForObjectInListSortedByFunction(breakpointTreeElement, parentTreeElement.children, this._compareBreakpointTreeElements));
        if (parentTreeElement.children.length === 1)
            parentTreeElement.expand();
        return breakpointTreeElement;
    },

    _addBreakpointsForSourceCode: function(sourceCode)
    {
        var breakpoints = WebInspector.debuggerManager.breakpointsForSourceCode(sourceCode);
        for (var i = 0; i < breakpoints.length; ++i)
            this._addBreakpoint(breakpoints[i], sourceCode);
    },

    _addTreeElementForSourceCodeToContentTreeOutline: function(sourceCode)
    {
        var treeElement = this._breakpointsContentTreeOutline.getCachedTreeElement(sourceCode);
        if (!treeElement) {
            if (sourceCode instanceof WebInspector.SourceMapResource)
                treeElement = new WebInspector.SourceMapResourceTreeElement(sourceCode);
            else if (sourceCode instanceof WebInspector.Resource)
                treeElement = new WebInspector.ResourceTreeElement(sourceCode);
            else if (sourceCode instanceof WebInspector.Script)
                treeElement = new WebInspector.ScriptTreeElement(sourceCode);
        }

        if (!treeElement.parent) {
            treeElement.hasChildren = false;
            treeElement.expand();

            this._breakpointsContentTreeOutline.insertChild(treeElement, insertionIndexForObjectInListSortedByFunction(treeElement, this._breakpointsContentTreeOutline.children, this._compareTopLevelTreeElements.bind(this)));
        }

        return treeElement;
    },

    _resourceAdded: function(event)
    {
        var resource = event.data.resource;

        if (![WebInspector.Resource.Type.Document, WebInspector.Resource.Type.Script].contains(resource.type))
            return;

        this._addTreeElementForSourceCodeToContentTreeOutline(resource);
        this._addBreakpointsForSourceCode(resource);
    },

    _mainResourceChanged: function(event)
    {
        var resource = event.target.mainResource;
        this._addTreeElementForSourceCodeToContentTreeOutline(resource);
        this._addBreakpointsForSourceCode(resource);
    },

    _scriptAdded: function(event)
    {
        var script = event.data.script;

        // FIXME: Allow for scripts generated by eval statements to appear, but filter out JSC internals
        // and other WebInspector internals lacking __WebInspector in the url attribute.
        if (!script.url)
            return;

        // Exclude inspector scripts.
        if (script.url && script.url.indexOf("__WebInspector") === 0)
            return;

        // Don't add breakpoints if the script is represented by a Resource. They were
        // already added by _resourceAdded.
        if (script.resource)
            return;

        this._addTreeElementForSourceCodeToContentTreeOutline(script);
        this._addBreakpointsForSourceCode(script);
    },

    _scriptsCleared: function(event)
    {
        for (var i = this._breakpointsContentTreeOutline.children.length - 1; i >= 0; --i) {
            var treeElement = this._breakpointsContentTreeOutline.children[i];
            if (!(treeElement instanceof WebInspector.ScriptTreeElement))
                continue;

            this._breakpointsContentTreeOutline.removeChildAtIndex(i, true, true);
        }
    },

    _breakpointAdded: function(event)
    {
        var breakpoint = event.data.breakpoint;
        this._addBreakpoint(breakpoint);
    },

    _breakpointRemoved: function(event)
    {
        var breakpoint = event.data.breakpoint;

        if (this._pauseReasonTreeOutline) {
            var pauseReasonBreakpointTreeElement = this._pauseReasonTreeOutline.getCachedTreeElement(breakpoint);
            if (pauseReasonBreakpointTreeElement)
                pauseReasonBreakpointTreeElement.removeStatusImage();
        }

        var breakpointTreeElement = this._breakpointsContentTreeOutline.getCachedTreeElement(breakpoint);
        console.assert(breakpointTreeElement);
        if (!breakpointTreeElement)
            return;

        this._removeBreakpointTreeElement(breakpointTreeElement);
    },

    _breakpointDisplayLocationDidChange: function(event)
    {
        var breakpoint = event.target;
        if (event.data.oldDisplaySourceCode === breakpoint.displaySourceCode)
            return;

        var breakpointTreeElement = this._breakpointsContentTreeOutline.getCachedTreeElement(breakpoint);
        if (!breakpointTreeElement)
            return;

        // A known breakpoint moved between resources, remove the old tree element
        // and create a new tree element with the updated file.

        var wasSelected = breakpointTreeElement.selected;

        this._removeBreakpointTreeElement(breakpointTreeElement);
        var newBreakpointTreeElement = this._addBreakpoint(breakpoint);

        if (newBreakpointTreeElement && wasSelected)
            newBreakpointTreeElement.revealAndSelect(true, false, true, true);
    },

    _removeBreakpointTreeElement: function(breakpointTreeElement)
    {
        var parentTreeElement = breakpointTreeElement.parent;
        parentTreeElement.removeChild(breakpointTreeElement);

        console.assert(parentTreeElement.parent === this._breakpointsContentTreeOutline);
    },

    _debuggerCallFramesDidChange: function()
    {
        this._callStackContentTreeOutline.removeChildren();

        var callFrames = WebInspector.debuggerManager.callFrames;
        if (!callFrames || !callFrames.length) {
            this._callStackRow.showEmptyMessage();
            return;
        }

        this._callStackRow.hideEmptyMessage();
        this._callStackRow.element.appendChild(this._callStackContentTreeOutline.element);

        var treeElementToSelect = null;

        var activeCallFrame = WebInspector.debuggerManager.activeCallFrame;
        for (var i = 0; i < callFrames.length; ++i) {
            var callFrameTreeElement = new WebInspector.CallFrameTreeElement(callFrames[i]);
            if (callFrames[i] === activeCallFrame)
                treeElementToSelect = callFrameTreeElement;
            this._callStackContentTreeOutline.appendChild(callFrameTreeElement);
        }

        if (treeElementToSelect)
            treeElementToSelect.select(true, true);
    },

    _debuggerActiveCallFrameDidChange: function()
    {
        var callFrames = WebInspector.debuggerManager.callFrames;
        if (!callFrames)
            return;

        var indexOfActiveCallFrame = callFrames.indexOf(WebInspector.debuggerManager.activeCallFrame);
        // It is useful to turn off the step out button when there is no call frame to go through
        // since there might be call frames in the backend that were removed when processing the call
        // frame payload.
        this._debuggerStepOutButtonItem.enabled = indexOfActiveCallFrame < callFrames.length - 1;
    },

    _breakpointsBeneathTreeElement: function(treeElement)
    {
        console.assert(treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement);
        if (!(treeElement instanceof WebInspector.ResourceTreeElement) && !(treeElement instanceof WebInspector.ScriptTreeElement))
            return [];

        var breakpoints = [];
        var breakpointTreeElements = treeElement.children;
        for (var i = 0; i < breakpointTreeElements.length; ++i) {
            console.assert(breakpointTreeElements[i] instanceof WebInspector.BreakpointTreeElement);
            console.assert(breakpointTreeElements[i].breakpoint);
            var breakpoint = breakpointTreeElements[i].breakpoint;
            if (breakpoint)
                breakpoints.push(breakpoint);
        }

        return breakpoints;
    },

    _removeAllBreakpoints: function(breakpoints)
    {
        for (var i = 0; i < breakpoints.length; ++i) {
            var breakpoint = breakpoints[i];
            if (WebInspector.debuggerManager.isBreakpointRemovable(breakpoint))
                WebInspector.debuggerManager.removeBreakpoint(breakpoint);
        }
    },

    _toggleAllBreakpoints: function(breakpoints, disabled)
    {
        for (var i = 0; i < breakpoints.length; ++i)
            breakpoints[i].disabled = disabled;
    },

    _breakpointTreeOutlineDeleteTreeElement: function(treeElement)
    {
        console.assert(treeElement.selected);
        console.assert(treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement);
        if (!(treeElement instanceof WebInspector.ResourceTreeElement) && !(treeElement instanceof WebInspector.ScriptTreeElement))
            return false;

        var wasTopResourceTreeElement = treeElement.previousSibling === this._allUncaughtExceptionsBreakpointTreeElement;
        var nextSibling = treeElement.nextSibling;

        var breakpoints = this._breakpointsBeneathTreeElement(treeElement);
        this._removeAllBreakpoints(breakpoints);

        if (wasTopResourceTreeElement && nextSibling)
            nextSibling.select(true, true);

        return true;
    },

    _breakpointTreeOutlineContextMenuTreeElement: function(event, treeElement)
    {
        console.assert(treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement || treeElement.constructor === WebInspector.FolderTreeElement);
        if (!(treeElement instanceof WebInspector.ResourceTreeElement) && !(treeElement instanceof WebInspector.ScriptTreeElement))
            return;

        var breakpoints = this._breakpointsBeneathTreeElement(treeElement);
        var shouldDisable = false;
        for (var i = 0; i < breakpoints.length; ++i) {
            if (!breakpoints[i].disabled) {
                shouldDisable = true;
                break;
            }
        }

        function removeAllResourceBreakpoints()
        {
            this._removeAllBreakpoints(breakpoints);
        }

        function toggleAllResourceBreakpoints()
        {
            this._toggleAllBreakpoints(breakpoints, shouldDisable);
        }

        var contextMenu = new WebInspector.ContextMenu(event);
        if (shouldDisable)
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoints"), toggleAllResourceBreakpoints.bind(this));
        else
            contextMenu.appendItem(WebInspector.UIString("Enable Breakpoints"), toggleAllResourceBreakpoints.bind(this));
        contextMenu.appendItem(WebInspector.UIString("Delete Breakpoints"), removeAllResourceBreakpoints.bind(this));
        contextMenu.show();
    },

    _treeElementSelected: function(treeElement, selectedByUser)
    {
        function deselectCallStackContentTreeElements()
        {
            var selectedTreeElement = this._callStackContentTreeOutline.selectedTreeElement;
            if (selectedTreeElement)
                selectedTreeElement.deselect();
        }

        function deselectBreakpointContentTreeElements()
        {
            var selectedTreeElement = this._breakpointsContentTreeOutline.selectedTreeElement;
            if (selectedTreeElement)
                selectedTreeElement.deselect();
        }

        function deselectPauseReasonContentTreeElements()
        {
            if (!this._pauseReasonTreeOutline)
                return;

            var selectedTreeElement = this._pauseReasonTreeOutline.selectedTreeElement;
            if (selectedTreeElement)
                selectedTreeElement.deselect();
        }

        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            deselectCallStackContentTreeElements.call(this);
            deselectPauseReasonContentTreeElements.call(this);
            WebInspector.resourceSidebarPanel.showSourceCode(treeElement.representedObject);
            return;
        }

        if (treeElement instanceof WebInspector.CallFrameTreeElement) {
            // Deselect any tree element in the breakpoint / pause reason content tree outlines to prevent two selections in the sidebar.
            deselectBreakpointContentTreeElements.call(this);
            deselectPauseReasonContentTreeElements.call(this);

            var callFrame = treeElement.callFrame;
            WebInspector.debuggerManager.activeCallFrame = callFrame;
            WebInspector.resourceSidebarPanel.showSourceCodeLocation(callFrame.sourceCodeLocation);
            return;
        }

        if (!(treeElement instanceof WebInspector.BreakpointTreeElement) || treeElement.parent.constructor === WebInspector.FolderTreeElement)
            return;

        // Deselect any other tree elements to prevent two selections in the sidebar.
        deselectCallStackContentTreeElements.call(this);

        if (treeElement.treeOutline === this._pauseReasonTreeOutline)
            deselectBreakpointContentTreeElements.call(this);
        else
            deselectPauseReasonContentTreeElements.call(this);

        var breakpoint = treeElement.breakpoint;
        if (treeElement.treeOutline === this._pauseReasonTreeOutline) {
            WebInspector.resourceSidebarPanel.showSourceCodeLocation(breakpoint.sourceCodeLocation);
            return;
        }

        if (!treeElement.parent.representedObject)
            return;

        console.assert(treeElement.parent.representedObject instanceof WebInspector.SourceCode);
        if (!(treeElement.parent.representedObject instanceof WebInspector.SourceCode))
            return;

        WebInspector.resourceSidebarPanel.showSourceCodeLocation(breakpoint.sourceCodeLocation);
    },

    _compareTopLevelTreeElements: function(a, b)
    {
        if (a === this._globalBreakpointsFolderTreeElement)
            return -1;
        if (b === this._globalBreakpointsFolderTreeElement)
            return 1;

        return a.mainTitle.localeCompare(b.mainTitle);
    },

    _compareBreakpointTreeElements: function(a, b)
    {
        var aLocation = a.breakpoint.sourceCodeLocation;
        var bLocation = b.breakpoint.sourceCodeLocation;

        var comparisonResult = aLocation.displayLineNumber - bLocation.displayLineNumber;
        if (comparisonResult !== 0)
            return comparisonResult;

        return aLocation.displayColumnNumber - bLocation.displayColumnNumber;
    },

    _updatePauseReason: function()
    {
        this._pauseReasonTreeOutline = null;

        this._updatePauseReasonGotoArrow();
        return this._updatePauseReasonSection();
    },

    _updatePauseReasonSection: function()
    {
        var pauseData = WebInspector.debuggerManager.pauseData;

        switch (WebInspector.debuggerManager.pauseReason) {
        case WebInspector.DebuggerManager.PauseReason.Assertion:
            // FIXME: We should include the assertion condition string.
            console.assert(pauseData, "Expected data with an assertion, but found none.");
            if (pauseData && pauseData.message) {
                this._pauseReasonTextRow.text = WebInspector.UIString("Assertion with message: %s").format(pauseData.message);
                return true;
            }

            this._pauseReasonTextRow.text = WebInspector.UIString("Assertion Failed");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WebInspector.DebuggerManager.PauseReason.Breakpoint:
            console.assert(pauseData, "Expected breakpoint identifier, but found none.");
            if (pauseData && pauseData.breakpointId) {
                var breakpoint = WebInspector.debuggerManager.breakpointForIdentifier(pauseData.breakpointId);
                var breakpointTreeOutline = this.createContentTreeOutline(true, true);
                breakpointTreeOutline.onselect = this._treeElementSelected.bind(this);
                var breakpointTreeElement = new WebInspector.BreakpointTreeElement(breakpoint, WebInspector.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName, WebInspector.UIString("Triggered Breakpoint"));
                var breakpointDetailsSection = new WebInspector.DetailsSectionRow;
                breakpointTreeOutline.appendChild(breakpointTreeElement);
                breakpointDetailsSection.element.appendChild(breakpointTreeOutline.element);

                this._pauseReasonGroup.rows = [breakpointDetailsSection];
                this._pauseReasonTreeOutline = breakpointTreeOutline;
                return true;
            }
            break;

        case WebInspector.DebuggerManager.PauseReason.CSPViolation:
            console.assert(pauseData, "Expected data with a CSP Violation, but found none.");
            if (pauseData) {
                // COMPATIBILITY (iOS 8): 'directive' was 'directiveText'.
                this._pauseReasonTextRow.text = WebInspector.UIString("Content Security Policy violation of directive: %s").format(pauseData.directive || pauseData.directiveText);
                this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
                return true;
            }
            break;

        case WebInspector.DebuggerManager.PauseReason.DebuggerStatement:
            this._pauseReasonTextRow.text = WebInspector.UIString("Debugger Statement");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WebInspector.DebuggerManager.PauseReason.Exception:
            console.assert(pauseData, "Expected data with an exception, but found none.");
            if (pauseData) {
                // FIXME: We should improve the appearance of thrown objects. This works well for exception strings.
                var data = WebInspector.RemoteObject.fromPayload(pauseData);
                this._pauseReasonTextRow.text = WebInspector.UIString("Exception with thrown value: %s").format(data.description);
                this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
                return true;
            }
            break;

        case WebInspector.DebuggerManager.PauseReason.PauseOnNextStatement:
            this._pauseReasonTextRow.text = WebInspector.UIString("Immediate Pause Requested");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WebInspector.DebuggerManager.PauseReason.Other:
            console.error("Paused for unknown reason. We should always have a reason.");
            break;
        }

        return false;
    },

    _updatePauseReasonGotoArrow: function()
    {
        this._pauseReasonLinkContainerElement.removeChildren();

        var activeCallFrame = WebInspector.debuggerManager.activeCallFrame;
        if (!activeCallFrame)
            return;

        var sourceCodeLocation = activeCallFrame.sourceCodeLocation;
        if (!sourceCodeLocation)
            return;

        var linkElement = WebInspector.createSourceCodeLocationLink(sourceCodeLocation, false, true);
        this._pauseReasonLinkContainerElement.appendChild(linkElement);
    }
};
