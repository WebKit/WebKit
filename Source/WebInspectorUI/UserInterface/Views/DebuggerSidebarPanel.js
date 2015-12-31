/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.DebuggerSidebarPanel = class DebuggerSidebarPanel extends WebInspector.NavigationSidebarPanel
{
    constructor(contentBrowser)
    {
        super("debugger", WebInspector.UIString("Debugger"), true);

        this.contentBrowser = contentBrowser;

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceAdded, this);

        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointsEnabledDidChange, this._breakpointsEnabledDidChange, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.CallFramesDidChange, this._debuggerCallFramesDidChange, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointAdded, this._breakpointAdded, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointRemoved, this._breakpointRemoved, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptAdded, this._scriptAdded, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptRemoved, this._scriptRemoved, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptsCleared, this._scriptsCleared, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ActiveCallFrameDidChange, this._debuggerActiveCallFrameDidChange, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.WaitingToPause, this._debuggerWaitingToPause, this);

        this._navigationBar = new WebInspector.NavigationBar;
        this.addSubview(this._navigationBar);

        var breakpointsImage = {src: "Images/Breakpoints.svg", width: 15, height: 15};
        var pauseImage = {src: "Images/Pause.svg", width: 15, height: 15};
        var resumeImage = {src: "Images/Resume.svg", width: 15, height: 15};
        var stepOverImage = {src: "Images/StepOver.svg", width: 15, height: 15};
        var stepIntoImage = {src: "Images/StepInto.svg", width: 15, height: 15};
        var stepOutImage = {src: "Images/StepOut.svg", width: 15, height: 15};

        var toolTip = WebInspector.UIString("Enable all breakpoints (%s)").format(WebInspector.toggleBreakpointsKeyboardShortcut.displayName);
        var altToolTip = WebInspector.UIString("Disable all breakpoints (%s)").format(WebInspector.toggleBreakpointsKeyboardShortcut.displayName);

        this._debuggerBreakpointsButtonItem = new WebInspector.ActivateButtonNavigationItem("debugger-breakpoints", toolTip, altToolTip, breakpointsImage.src, breakpointsImage.width, breakpointsImage.height);
        this._debuggerBreakpointsButtonItem.activated = WebInspector.debuggerManager.breakpointsEnabled;
        this._debuggerBreakpointsButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.debuggerToggleBreakpoints, this);
        this._navigationBar.addNavigationItem(this._debuggerBreakpointsButtonItem);

        toolTip = WebInspector.UIString("Pause script execution (%s or %s)").format(WebInspector.pauseOrResumeKeyboardShortcut.displayName, WebInspector.pauseOrResumeAlternateKeyboardShortcut.displayName);
        altToolTip = WebInspector.UIString("Continue script execution (%s or %s)").format(WebInspector.pauseOrResumeKeyboardShortcut.displayName, WebInspector.pauseOrResumeAlternateKeyboardShortcut.displayName);

        this._debuggerPauseResumeButtonItem = new WebInspector.ToggleButtonNavigationItem("debugger-pause-resume", toolTip, altToolTip, pauseImage.src, resumeImage.src, pauseImage.width, pauseImage.height);
        this._debuggerPauseResumeButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.debuggerPauseResumeToggle, this);
        this._navigationBar.addNavigationItem(this._debuggerPauseResumeButtonItem);

        this._debuggerStepOverButtonItem = new WebInspector.ButtonNavigationItem("debugger-step-over", WebInspector.UIString("Step over (%s or %s)").format(WebInspector.stepOverKeyboardShortcut.displayName, WebInspector.stepOverAlternateKeyboardShortcut.displayName), stepOverImage.src, stepOverImage.width, stepOverImage.height);
        this._debuggerStepOverButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.debuggerStepOver, this);
        this._debuggerStepOverButtonItem.enabled = false;
        this._navigationBar.addNavigationItem(this._debuggerStepOverButtonItem);

        this._debuggerStepIntoButtonItem = new WebInspector.ButtonNavigationItem("debugger-step-into", WebInspector.UIString("Step into (%s or %s)").format(WebInspector.stepIntoKeyboardShortcut.displayName, WebInspector.stepIntoAlternateKeyboardShortcut.displayName), stepIntoImage.src, stepIntoImage.width, stepIntoImage.height);
        this._debuggerStepIntoButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.debuggerStepInto, this);
        this._debuggerStepIntoButtonItem.enabled = false;
        this._navigationBar.addNavigationItem(this._debuggerStepIntoButtonItem);

        this._debuggerStepOutButtonItem = new WebInspector.ButtonNavigationItem("debugger-step-out", WebInspector.UIString("Step out (%s or %s)").format(WebInspector.stepOutKeyboardShortcut.displayName, WebInspector.stepOutAlternateKeyboardShortcut.displayName), stepOutImage.src, stepOutImage.width, stepOutImage.height);
        this._debuggerStepOutButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, WebInspector.debuggerStepOut, this);
        this._debuggerStepOutButtonItem.enabled = false;
        this._navigationBar.addNavigationItem(this._debuggerStepOutButtonItem);

        // Add this offset-sections class name so the sticky headers don't overlap the navigation bar.
        this.element.classList.add(WebInspector.DebuggerSidebarPanel.OffsetSectionsStyleClassName);

        this._globalBreakpointsFolderTreeElement = new WebInspector.FolderTreeElement(WebInspector.UIString("Global Breakpoints"), null, WebInspector.DebuggerSidebarPanel.GlobalIconStyleClassName);
        this._allExceptionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.allExceptionsBreakpoint, WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName, WebInspector.UIString("All Exceptions"));
        this._allUncaughtExceptionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.allUncaughtExceptionsBreakpoint, WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName, WebInspector.UIString("All Uncaught Exceptions"));
        this.suppressFilteringOnTreeElements([this._globalBreakpointsFolderTreeElement, this._allExceptionsBreakpointTreeElement, this._allUncaughtExceptionsBreakpointTreeElement]);

        this.filterBar.placeholder = WebInspector.UIString("Filter Breakpoint List");

        function showResourcesWithIssuesOnlyFilterFunction(treeElement)
        {
            // Keep issues.
            if (treeElement instanceof WebInspector.IssueTreeElement)
                return true;

            // Keep resources with issues.
            if (treeElement.hasChildren) {
                for (let child of treeElement.children) {
                    if (child instanceof WebInspector.IssueTreeElement)
                        return true;
                }
            }
            return false;
        };

        this.filterBar.addFilterBarButton("debugger-show-resources-with-issues-only", showResourcesWithIssuesOnlyFilterFunction, true, WebInspector.UIString("Show only resources with issues."), WebInspector.UIString("Show resources with and without issues."), "Images/Errors.svg", 15, 15);

        this._breakpointsContentTreeOutline = this.contentTreeOutline;

        let breakpointsRow = new WebInspector.DetailsSectionRow;
        breakpointsRow.element.appendChild(this._breakpointsContentTreeOutline.element);

        let breakpointsGroup = new WebInspector.DetailsSectionGroup([breakpointsRow]);
        let breakpointsSection = new WebInspector.DetailsSection("scripts", WebInspector.UIString("Scripts"), [breakpointsGroup]);
        this.contentView.element.appendChild(breakpointsSection.element);

        this._breakpointsContentTreeOutline.element.classList.add("breakpoints");
        this._breakpointsContentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        this._breakpointsContentTreeOutline.ondelete = this._breakpointTreeOutlineDeleteTreeElement.bind(this);
        this._breakpointsContentTreeOutline.oncontextmenu = this._breakpointTreeOutlineContextMenuTreeElement.bind(this);

        this._breakpointsContentTreeOutline.appendChild(this._globalBreakpointsFolderTreeElement);
        this._globalBreakpointsFolderTreeElement.appendChild(this._allExceptionsBreakpointTreeElement);
        this._globalBreakpointsFolderTreeElement.appendChild(this._allUncaughtExceptionsBreakpointTreeElement);
        this._globalBreakpointsFolderTreeElement.expand();

        this._callStackContentTreeOutline = this.createContentTreeOutline(true, true);
        this._callStackContentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        this._callStackRow = new WebInspector.DetailsSectionRow(WebInspector.UIString("No Call Frames"));
        this._callStackRow.showEmptyMessage();

        let callStackGroup = new WebInspector.DetailsSectionGroup([this._callStackRow]);
        this._callStackSection = new WebInspector.DetailsSection("call-stack", WebInspector.UIString("Call Stack"), [callStackGroup]);

        this._pauseReasonTreeOutline = null;

        this._pauseReasonLinkContainerElement = document.createElement("span");
        this._pauseReasonTextRow = new WebInspector.DetailsSectionTextRow;
        this._pauseReasonGroup = new WebInspector.DetailsSectionGroup([this._pauseReasonTextRow]);
        this._pauseReasonSection = new WebInspector.DetailsSection("paused-reason", null, [this._pauseReasonGroup], this._pauseReasonLinkContainerElement);
        this._pauseReasonSection.title = WebInspector.UIString("Pause Reason");

        WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WebInspector.IssueMessage.addEventListener(WebInspector.IssueMessage.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WebInspector.issueManager.addEventListener(WebInspector.IssueManager.Event.IssueWasAdded, this._handleIssueAdded, this);
        WebInspector.issueManager.addEventListener(WebInspector.IssueManager.Event.Cleared, this._handleIssuesCleared, this);

        if (WebInspector.frameResourceManager.mainFrame)
            this._addResourcesRecursivelyForFrame(WebInspector.frameResourceManager.mainFrame);

        for (var script of WebInspector.debuggerManager.knownNonResourceScripts)
            this._addScript(script);
    }

    // Public

    closed()
    {
        super.closed();

        WebInspector.Frame.removeEventListener(null, null, this);
        WebInspector.debuggerManager.removeEventListener(null, null, this);
        WebInspector.Breakpoint.removeEventListener(null, null, this);
        WebInspector.IssueMessage.removeEventListener(null, null, this);
    }

    showDefaultContentView()
    {
        var currentTreeElement = this._contentTreeOutline.children[0];
        while (currentTreeElement && !currentTreeElement.root) {
            if (currentTreeElement instanceof WebInspector.ResourceTreeElement || currentTreeElement instanceof WebInspector.ScriptTreeElement) {
                this.showDefaultContentViewForTreeElement(currentTreeElement);
                return;
            }

            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, true);
        }
    }

    treeElementForRepresentedObject(representedObject)
    {
        // The main resource is used as the representedObject instead of Frame in our tree.
        if (representedObject instanceof WebInspector.Frame)
            representedObject = representedObject.mainResource;

        return this.contentTreeOutline.getCachedTreeElement(representedObject);
    }

    // Protected

    saveStateToCookie(cookie)
    {
        console.assert(cookie);

        var selectedTreeElement = this._breakpointsContentTreeOutline.selectedTreeElement;
        if (!selectedTreeElement)
            return;

        var representedObject = selectedTreeElement.representedObject;

        if (representedObject === WebInspector.debuggerManager.allExceptionsBreakpoint) {
            cookie[WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey] = true;
            return;
        }

        if (representedObject === WebInspector.debuggerManager.allUncaughtExceptionsBreakpoint) {
            cookie[WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey] = true;
            return;
        }

        super.saveStateToCookie(cookie);
    }

    restoreStateFromCookie(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // Eagerly resolve the special breakpoints; otherwise, use the default behavior.
        if (cookie[WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey])
            this._allExceptionsBreakpointTreeElement.revealAndSelect();
        else if (cookie[WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey])
            this._allUncaughtExceptionsBreakpointTreeElement.revealAndSelect();
        else
            super.restoreStateFromCookie(cookie, relaxedMatchDelay);
    }

    // Private

    _debuggerWaitingToPause(event)
    {
        this._debuggerPauseResumeButtonItem.enabled = false;
    }

    _debuggerDidPause(event)
    {
        this.contentView.element.insertBefore(this._callStackSection.element, this.contentView.element.firstChild);
        if (this._updatePauseReason())
            this.contentView.element.insertBefore(this._pauseReasonSection.element, this.contentView.element.firstChild);

        this._debuggerPauseResumeButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.toggled = true;
        this._debuggerStepOverButtonItem.enabled = true;
        this._debuggerStepIntoButtonItem.enabled = true;

        this.element.classList.add(WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName);
    }

    _debuggerDidResume(event)
    {
        this._callStackSection.element.remove();
        this._pauseReasonSection.element.remove();

        this._debuggerPauseResumeButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.toggled = false;
        this._debuggerStepOverButtonItem.enabled = false;
        this._debuggerStepIntoButtonItem.enabled = false;
        this._debuggerStepOutButtonItem.enabled = false;

        this.element.classList.remove(WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName);
    }

    _breakpointsEnabledDidChange(event)
    {
        this._debuggerBreakpointsButtonItem.activated = WebInspector.debuggerManager.breakpointsEnabled;
    }

    _addBreakpoint(breakpoint)
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
        parentTreeElement.insertChild(breakpointTreeElement, insertionIndexForObjectInListSortedByFunction(breakpointTreeElement, parentTreeElement.children, this._compareDebuggerTreeElements));
        if (parentTreeElement.children.length === 1)
            parentTreeElement.expand();
        return breakpointTreeElement;
    }

    _addBreakpointsForSourceCode(sourceCode)
    {
        var breakpoints = WebInspector.debuggerManager.breakpointsForSourceCode(sourceCode);
        for (var i = 0; i < breakpoints.length; ++i)
            this._addBreakpoint(breakpoints[i], sourceCode);
    }

    _addIssuesForSourceCode(sourceCode)
    {
        var issues = WebInspector.issueManager.issuesForSourceCode(sourceCode);
        for (var issue of issues)
            this._addIssue(issue);
    }

    _addTreeElementForSourceCodeToContentTreeOutline(sourceCode)
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

        if (!treeElement) {
            console.error("Unknown sourceCode instance", sourceCode);
            return;
        }

        if (!treeElement.parent) {
            treeElement.hasChildren = false;
            treeElement.expand();

            this._breakpointsContentTreeOutline.insertChild(treeElement, insertionIndexForObjectInListSortedByFunction(treeElement, this._breakpointsContentTreeOutline.children, this._compareTopLevelTreeElements.bind(this)));
        }

        return treeElement;
    }

    _addResourcesRecursivelyForFrame(frame)
    {
        this._addResource(frame.mainResource);

        for (var resource of frame.resources)
            this._addResource(resource);

        for (var childFrame of frame.childFrames)
            this._addResourcesRecursivelyForFrame(childFrame);
    }

    _resourceAdded(event)
    {
        this._addResource(event.data.resource);
    }

    _addResource(resource)
    {
        if (![WebInspector.Resource.Type.Document, WebInspector.Resource.Type.Script].includes(resource.type))
            return;

        let treeElement = this._addTreeElementForSourceCodeToContentTreeOutline(resource);
        this._addBreakpointsForSourceCode(resource);
        this._addIssuesForSourceCode(resource);

        if (!this.contentBrowser.currentContentView)
            this.showDefaultContentViewForTreeElement(treeElement);
    }

    _mainResourceDidChange(event)
    {
        if (event.target.isMainFrame()) {
            // Aggressively prune resources now so the old resources are removed before
            // the new main resource is added below. This avoids a visual flash when the
            // prune normally happens on a later event loop cycle.
            this.pruneStaleResourceTreeElements();
            this.contentBrowser.contentViewContainer.closeAllContentViews();
        }

        var resource = event.target.mainResource;
        this._addTreeElementForSourceCodeToContentTreeOutline(resource);
        this._addBreakpointsForSourceCode(resource);
        this._addIssuesForSourceCode(resource);
    }

    _scriptAdded(event)
    {
        this._addScript(event.data.script);
    }

    _addScript(script)
    {
        // COMPATIBILITY(iOS 9): Backends could send the frontend built-in code, filter out JSC internals.
        if (!script.url)
            return;

        // Don't add breakpoints if the script is represented by a Resource. They were
        // already added by _resourceAdded.
        if (script.resource)
            return;

        let treeElement = this._addTreeElementForSourceCodeToContentTreeOutline(script);
        this._addBreakpointsForSourceCode(script);
        this._addIssuesForSourceCode(script);

        if (!this.contentBrowser.currentContentView)
            this.showDefaultContentViewForTreeElement(treeElement);
    }

    _scriptRemoved(event)
    {
        let script = event.data.script;
        let scriptTreeElement = this._breakpointsContentTreeOutline.getCachedTreeElement(script);
        if (!scriptTreeElement)
            return;

        scriptTreeElement.parent.removeChild(scriptTreeElement);
    }

    _scriptsCleared(event)
    {
        for (var i = this._breakpointsContentTreeOutline.children.length - 1; i >= 0; --i) {
            var treeElement = this._breakpointsContentTreeOutline.children[i];
            if (!(treeElement instanceof WebInspector.ScriptTreeElement))
                continue;

            this._breakpointsContentTreeOutline.removeChildAtIndex(i, true, true);
        }
    }

    _breakpointAdded(event)
    {
        var breakpoint = event.data.breakpoint;
        this._addBreakpoint(breakpoint);
    }

    _breakpointRemoved(event)
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

        this._removeDebuggerTreeElement(breakpointTreeElement);
    }

    _handleDebuggerObjectDisplayLocationDidChange(event)
    {
        var debuggerObject = event.target;

        if (event.data.oldDisplaySourceCode === debuggerObject.sourceCodeLocation.displaySourceCode)
            return;

        var debuggerTreeElement = this._breakpointsContentTreeOutline.getCachedTreeElement(debuggerObject);
        if (!debuggerTreeElement)
            return;

        // A known debugger object (breakpoint, issueMessage, etc.) moved between resources, remove the old tree element
        // and create a new tree element with the updated file.

        var wasSelected = debuggerTreeElement.selected;

        this._removeDebuggerTreeElement(debuggerTreeElement);
        var newDebuggerTreeElement = this._addDebuggerObject(debuggerObject);

        if (newDebuggerTreeElement && wasSelected)
            newDebuggerTreeElement.revealAndSelect(true, false, true, true);
    }

    _removeDebuggerTreeElement(debuggerTreeElement)
    {
        var parentTreeElement = debuggerTreeElement.parent;
        parentTreeElement.removeChild(debuggerTreeElement);

        console.assert(parentTreeElement.parent === this._breakpointsContentTreeOutline);
    }

    _debuggerCallFramesDidChange()
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
    }

    _debuggerActiveCallFrameDidChange()
    {
        var callFrames = WebInspector.debuggerManager.callFrames;
        if (!callFrames)
            return;

        var indexOfActiveCallFrame = callFrames.indexOf(WebInspector.debuggerManager.activeCallFrame);
        // It is useful to turn off the step out button when there is no call frame to go through
        // since there might be call frames in the backend that were removed when processing the call
        // frame payload.
        this._debuggerStepOutButtonItem.enabled = indexOfActiveCallFrame < callFrames.length - 1;
    }

    _breakpointsBeneathTreeElement(treeElement)
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
    }

    _removeAllBreakpoints(breakpoints)
    {
        for (var i = 0; i < breakpoints.length; ++i) {
            var breakpoint = breakpoints[i];
            if (WebInspector.debuggerManager.isBreakpointRemovable(breakpoint))
                WebInspector.debuggerManager.removeBreakpoint(breakpoint);
        }
    }

    _toggleAllBreakpoints(breakpoints, disabled)
    {
        for (var i = 0; i < breakpoints.length; ++i)
            breakpoints[i].disabled = disabled;
    }

    _breakpointTreeOutlineDeleteTreeElement(treeElement)
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
    }

    _breakpointTreeOutlineContextMenuTreeElement(event, treeElement)
    {
        console.assert(treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement || treeElement.constructor === WebInspector.FolderTreeElement);
        if (!(treeElement instanceof WebInspector.ResourceTreeElement) && !(treeElement instanceof WebInspector.ScriptTreeElement))
            return;

        let breakpoints = this._breakpointsBeneathTreeElement(treeElement);
        let shouldDisable = breakpoints.some((breakpoint) => !breakpoint.disabled);

        let removeAllResourceBreakpoints = () => {
            this._removeAllBreakpoints(breakpoints);
        };

        let toggleAllResourceBreakpoints = () => {
            this._toggleAllBreakpoints(breakpoints, shouldDisable);
        };

        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);
        if (shouldDisable)
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoints"), toggleAllResourceBreakpoints);
        else
            contextMenu.appendItem(WebInspector.UIString("Enable Breakpoints"), toggleAllResourceBreakpoints);
        contextMenu.appendItem(WebInspector.UIString("Delete Breakpoints"), removeAllResourceBreakpoints);
    }

    _treeSelectionDidChange(event)
    {
        let treeElement = event.data.selectedElement;
        if (!treeElement)
            return;

        // Deselect any other tree elements to prevent two selections in the sidebar.
        for (let treeOutline of this.visibleContentTreeOutlines) {
            if (treeOutline === treeElement.treeOutline)
                continue;

            let selectedTreeElement = treeOutline.selectedTreeElement;
            if (selectedTreeElement)
                selectedTreeElement.deselect();
        }

        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            WebInspector.showSourceCode(treeElement.representedObject);
            return;
        }

        if (treeElement instanceof WebInspector.CallFrameTreeElement) {
            let callFrame = treeElement.callFrame;
            WebInspector.debuggerManager.activeCallFrame = callFrame;
            WebInspector.showSourceCodeLocation(callFrame.sourceCodeLocation);
            return;
        }

        if (treeElement instanceof WebInspector.IssueTreeElement) {
            WebInspector.showSourceCodeLocation(treeElement.issueMessage.sourceCodeLocation);
            return;
        }

        if (!(treeElement instanceof WebInspector.BreakpointTreeElement) || treeElement.parent.constructor === WebInspector.FolderTreeElement)
            return;

        let breakpoint = treeElement.breakpoint;
        if (treeElement.treeOutline === this._pauseReasonTreeOutline) {
            WebInspector.showSourceCodeLocation(breakpoint.sourceCodeLocation);
            return;
        }

        if (!treeElement.parent.representedObject)
            return;

        console.assert(treeElement.parent.representedObject instanceof WebInspector.SourceCode);
        if (!(treeElement.parent.representedObject instanceof WebInspector.SourceCode))
            return;

        WebInspector.showSourceCodeLocation(breakpoint.sourceCodeLocation);
    }

    _compareTopLevelTreeElements(a, b)
    {
        if (a === this._globalBreakpointsFolderTreeElement)
            return -1;
        if (b === this._globalBreakpointsFolderTreeElement)
            return 1;

        return a.mainTitle.localeCompare(b.mainTitle);
    }

    _compareDebuggerTreeElements(a, b)
    {
        if (!a.debuggerObject || !b.debuggerObject)
            return 0;
        
        var aLocation = a.debuggerObject.sourceCodeLocation;
        var bLocation = b.debuggerObject.sourceCodeLocation;

        var comparisonResult = aLocation.displayLineNumber - bLocation.displayLineNumber;
        if (comparisonResult !== 0)
            return comparisonResult;

        return aLocation.displayColumnNumber - bLocation.displayColumnNumber;
    }

    _updatePauseReason()
    {
        this._pauseReasonTreeOutline = null;

        this._updatePauseReasonGotoArrow();
        return this._updatePauseReasonSection();
    }

    _updatePauseReasonSection()
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
                let breakpoint = WebInspector.debuggerManager.breakpointForIdentifier(pauseData.breakpointId);
                this._pauseReasonTreeOutline = this.createContentTreeOutline(true, true);
                this._pauseReasonTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

                let breakpointTreeElement = new WebInspector.BreakpointTreeElement(breakpoint, WebInspector.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName, WebInspector.UIString("Triggered Breakpoint"));
                let breakpointDetailsSection = new WebInspector.DetailsSectionRow;
                this._pauseReasonTreeOutline.appendChild(breakpointTreeElement);
                breakpointDetailsSection.element.appendChild(this._pauseReasonTreeOutline.element);

                this._pauseReasonGroup.rows = [breakpointDetailsSection];
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
    }

    _updatePauseReasonGotoArrow()
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

    _addDebuggerObject(debuggerObject)
    {
        if (debuggerObject instanceof WebInspector.Breakpoint)
            return this._addBreakpoint(debuggerObject);

        if (debuggerObject instanceof WebInspector.IssueMessage)
            return this._addIssue(debuggerObject);
    }

    _addIssue(issueMessage)
    {
        var parentTreeElement = this._addTreeElementForSourceCodeToContentTreeOutline(issueMessage.sourceCodeLocation.sourceCode);
        if (!parentTreeElement)
            return null;

        var issueTreeElement = new WebInspector.IssueTreeElement(issueMessage);

        parentTreeElement.insertChild(issueTreeElement, insertionIndexForObjectInListSortedByFunction(issueTreeElement, parentTreeElement.children, this._compareDebuggerTreeElements));
        if (parentTreeElement.children.length === 1)
            parentTreeElement.expand();

        return issueTreeElement;
    }

    _handleIssueAdded(event)
    {
        var issue = event.data.issue;

        // We only want to show issues originating from JavaScript source code.
        if (!issue.sourceCodeLocation || !issue.sourceCodeLocation.sourceCode || (issue.source !== "javascript" && issue.source !== "console-api"))
            return;

        this._addIssue(issue);
    }

    _handleIssuesCleared(event)
    {
        var currentTreeElement = this._contentTreeOutline.children[0];
        var issueTreeElements = [];

        while (currentTreeElement && !currentTreeElement.root) {
            if (currentTreeElement instanceof WebInspector.IssueTreeElement)
                issueTreeElements.push(currentTreeElement);
            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, true);
        }

        for (var issueTreeElement of issueTreeElements)
            issueTreeElement.parent.removeChild(issueTreeElement);
    }
};

WebInspector.DebuggerSidebarPanel.OffsetSectionsStyleClassName = "offset-sections";
WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName = "paused";
WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName = "breakpoint-exception-icon";
WebInspector.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName = "breakpoint-paused-icon";
WebInspector.DebuggerSidebarPanel.GlobalIconStyleClassName = "global-breakpoints-icon";

WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey = "debugger-sidebar-panel-all-exceptions-breakpoint";
WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey = "debugger-sidebar-panel-all-uncaught-exceptions-breakpoint";
