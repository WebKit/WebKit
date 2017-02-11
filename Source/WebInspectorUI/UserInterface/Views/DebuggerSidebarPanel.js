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
        WebInspector.Target.addEventListener(WebInspector.Target.Event.ResourceAdded, this._resourceAdded, this);

        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointsEnabledDidChange, this._breakpointsEnabledDidChange, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointAdded, this._breakpointAdded, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.BreakpointRemoved, this._breakpointRemoved, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptAdded, this._scriptAdded, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptRemoved, this._scriptRemoved, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ScriptsCleared, this._scriptsCleared, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.CallFramesDidChange, this._debuggerCallFramesDidChange, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.ActiveCallFrameDidChange, this._debuggerActiveCallFrameDidChange, this);
        WebInspector.debuggerManager.addEventListener(WebInspector.DebuggerManager.Event.WaitingToPause, this._debuggerWaitingToPause, this);

        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingWillStart, this._timelineCapturingWillStart, this);
        WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.CapturingStopped, this._timelineCapturingStopped, this);

        WebInspector.targetManager.addEventListener(WebInspector.TargetManager.Event.TargetAdded, this._targetAdded, this);
        WebInspector.targetManager.addEventListener(WebInspector.TargetManager.Event.TargetRemoved, this._targetRemoved, this);

        this._timelineRecordingWarningElement = document.createElement("div");
        this._timelineRecordingWarningElement.classList.add("warning-banner");
        this._timelineRecordingWarningElement.append(WebInspector.UIString("Debugger disabled during Timeline recording"), " ");
        let stopRecordingLink = this._timelineRecordingWarningElement.appendChild(document.createElement("a"));
        stopRecordingLink.textContent = WebInspector.UIString("Stop recording");
        stopRecordingLink.addEventListener("click", () => { WebInspector.timelineManager.stopCapturing(); });

        this._breakpointsDisabledWarningElement = document.createElement("div");
        this._breakpointsDisabledWarningElement.classList.add("warning-banner");
        this._breakpointsDisabledWarningElement.append(WebInspector.UIString("Breakpoints disabled"), document.createElement("br"));
        let enableBreakpointsLink = this._breakpointsDisabledWarningElement.appendChild(document.createElement("a"));
        enableBreakpointsLink.textContent = WebInspector.UIString("Enable breakpoints");
        enableBreakpointsLink.addEventListener("click", () => { WebInspector.debuggerToggleBreakpoints(); });

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

        this._allExceptionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.allExceptionsBreakpoint, WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName, WebInspector.UIString("All Exceptions"));
        this._allUncaughtExceptionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.allUncaughtExceptionsBreakpoint, WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName, WebInspector.UIString("Uncaught Exceptions"));
        this._assertionsBreakpointTreeElement = new WebInspector.BreakpointTreeElement(WebInspector.debuggerManager.assertionsBreakpoint, WebInspector.DebuggerSidebarPanel.AssertionIconStyleClassName, WebInspector.UIString("Assertion Failures"));

        this.suppressFilteringOnTreeElements([this._allExceptionsBreakpointTreeElement, this._allUncaughtExceptionsBreakpointTreeElement, this._assertionsBreakpointTreeElement]);

        this.filterBar.placeholder = WebInspector.UIString("Filter List");

        function showResourcesWithIssuesOnlyFilterFunction(treeElement)
        {
            // Issues are only shown in the scripts tree outline.
            if (treeElement.treeOutline !== this._scriptsContentTreeOutline)
                return true;

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
        }

        this.filterBar.addFilterBarButton("debugger-show-resources-with-issues-only", showResourcesWithIssuesOnlyFilterFunction.bind(this), false, WebInspector.UIString("Only show resources with issues"), WebInspector.UIString("Show all resources"), "Images/Errors.svg", 15, 15);

        this._breakpointsContentTreeOutline = this.contentTreeOutline;

        let breakpointsRow = new WebInspector.DetailsSectionRow;
        breakpointsRow.element.appendChild(this._breakpointsContentTreeOutline.element);

        let breakpointsGroup = new WebInspector.DetailsSectionGroup([breakpointsRow]);
        let breakpointsSection = new WebInspector.DetailsSection("breakpoints", WebInspector.UIString("Breakpoints"), [breakpointsGroup]);
        this.contentView.element.appendChild(breakpointsSection.element);

        this._breakpointSectionElement = breakpointsSection.element;

        this._breakpointsContentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        this._breakpointsContentTreeOutline.ondelete = this._breakpointTreeOutlineDeleteTreeElement.bind(this);
        this._breakpointsContentTreeOutline.populateContextMenu = function(contextMenu, event, treeElement) {
            this._breakpointTreeOutlineContextMenuTreeElement(contextMenu, event, treeElement);

            WebInspector.TreeOutline.prototype.populateContextMenu(contextMenu, event, treeElement);
        }.bind(this);

        this._breakpointsContentTreeOutline.appendChild(this._allExceptionsBreakpointTreeElement);
        this._breakpointsContentTreeOutline.appendChild(this._allUncaughtExceptionsBreakpointTreeElement);

        // COMPATIBILITY (iOS 10): DebuggerAgent.setPauseOnAssertions did not exist yet.
        if (DebuggerAgent.setPauseOnAssertions)
            this._breakpointsContentTreeOutline.appendChild(this._assertionsBreakpointTreeElement);

        this._scriptsContentTreeOutline = this.createContentTreeOutline();
        this._scriptsContentTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        let scriptsRow = new WebInspector.DetailsSectionRow;
        scriptsRow.element.appendChild(this._scriptsContentTreeOutline.element);

        let scriptsGroup = new WebInspector.DetailsSectionGroup([scriptsRow]);
        this._scriptsSection = new WebInspector.DetailsSection("scripts", WebInspector.UIString("Sources"), [scriptsGroup]);
        this.contentView.element.appendChild(this._scriptsSection.element);

        const suppressFiltering = true;
        this._callStackTreeOutline = this.createContentTreeOutline(suppressFiltering);
        this._callStackTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        this._mainTargetTreeElement = new WebInspector.ThreadTreeElement(WebInspector.mainTarget);
        this._callStackTreeOutline.appendChild(this._mainTargetTreeElement);

        this._updateCallStackTreeOutline();

        this._callStackRow = new WebInspector.DetailsSectionRow;
        this._callStackRow.element.appendChild(this._callStackTreeOutline.element);

        this._callStackGroup = new WebInspector.DetailsSectionGroup([this._callStackRow]);
        this._callStackSection = new WebInspector.DetailsSection("call-stack", WebInspector.UIString("Call Stack"), [this._callStackGroup]);

        this._showingSingleThreadCallStack = true;

        this._activeCallFrameTreeElement = null;

        this._pauseReasonTreeOutline = null;

        this._pauseReasonLinkContainerElement = document.createElement("span");
        this._pauseReasonTextRow = new WebInspector.DetailsSectionTextRow;
        this._pauseReasonGroup = new WebInspector.DetailsSectionGroup([this._pauseReasonTextRow]);
        this._pauseReasonSection = new WebInspector.DetailsSection("paused-reason", WebInspector.UIString("Pause Reason"), [this._pauseReasonGroup], this._pauseReasonLinkContainerElement);

        WebInspector.Breakpoint.addEventListener(WebInspector.Breakpoint.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WebInspector.IssueMessage.addEventListener(WebInspector.IssueMessage.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WebInspector.issueManager.addEventListener(WebInspector.IssueManager.Event.IssueWasAdded, this._handleIssueAdded, this);
        WebInspector.issueManager.addEventListener(WebInspector.IssueManager.Event.Cleared, this._handleIssuesCleared, this);

        if (WebInspector.frameResourceManager.mainFrame)
            this._addResourcesRecursivelyForFrame(WebInspector.frameResourceManager.mainFrame);

        for (var script of WebInspector.debuggerManager.knownNonResourceScripts)
            this._addScript(script);

        if (WebInspector.debuggerManager.paused)
            this._debuggerDidPause(null);

        if (WebInspector.timelineManager.isCapturing() && WebInspector.debuggerManager.breakpointsDisabledTemporarily)
            this._timelineCapturingWillStart(null);

        this._updateBreakpointsDisabledBanner();
    }

    // Public

    get minimumWidth()
    {
        return this._navigationBar.minimumWidth;
    }

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
                if (this.showDefaultContentViewForTreeElement(currentTreeElement))
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

        let treeElement = this._breakpointsContentTreeOutline.findTreeElement(representedObject);
        if (!treeElement)
            treeElement = this._scriptsContentTreeOutline.findTreeElement(representedObject);

        if (treeElement)
            return treeElement;

        // Only special case Script objects.
        if (!(representedObject instanceof WebInspector.Script)) {
            console.error("Didn't find a TreeElement for representedObject", representedObject);
            return null;
        }

        // If the Script has a URL we should have found it earlier.
        if (representedObject.url) {
            console.error("Didn't find a ScriptTreeElement for a Script with a URL.");
            return null;
        }

        // Since the Script does not have a URL we consider it an 'anonymous' script. These scripts happen from calls to
        // window.eval() or browser features like Auto Fill and Reader. They are not normally added to the sidebar, but since
        // we have a ScriptContentView asking for the tree element we will make a ScriptTreeElement on demand and add it.

        return this._addScript(representedObject);
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

        if (representedObject === WebInspector.debuggerManager.assertionsBreakpoint) {
            cookie[WebInspector.DebuggerSidebarPanel.SelectedAssertionsCookieKey] = true;
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
        else if (cookie[WebInspector.DebuggerSidebarPanel.SelectedAssertionsCookieKey])
            this._assertionsBreakpointTreeElement.revealAndSelect();
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
        this._debuggerStepOutButtonItem.enabled = true;

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

        this._updateBreakpointsDisabledBanner();
    }

    _addBreakpoint(breakpoint)
    {
        let sourceCode = breakpoint.sourceCodeLocation.displaySourceCode;
        if (!sourceCode)
            return null;

        if (this._breakpointsContentTreeOutline.findTreeElement(breakpoint))
            return;

        let parentTreeElement = this._addTreeElementForSourceCodeToTreeOutline(sourceCode, this._breakpointsContentTreeOutline);

        // Mark disabled breakpoints as resolved if there is source code loaded with that URL.
        // This gives the illusion the breakpoint was resolved, but since we don't send disabled
        // breakpoints to the backend we don't know for sure. If the user enables the breakpoint
        // it will be resolved properly.
        if (breakpoint.disabled)
            breakpoint.resolved = true;

        let breakpointTreeElement = new WebInspector.BreakpointTreeElement(breakpoint);
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

    _addTreeElementForSourceCodeToTreeOutline(sourceCode, treeOutline)
    {
        let treeElement = treeOutline.getCachedTreeElement(sourceCode);
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
            return null;
        }

        if (!treeElement.parent) {
            treeElement.hasChildren = false;
            treeElement.expand();

            treeOutline.insertChild(treeElement, insertionIndexForObjectInListSortedByFunction(treeElement, treeOutline.children, this._compareTopLevelTreeElements.bind(this)));
        }

        return treeElement;
    }

    _addResourcesRecursivelyForFrame(frame)
    {
        this._addResource(frame.mainResource);

        for (let resource of frame.resourceCollection.items)
            this._addResource(resource);

        for (let childFrame of frame.childFrameCollection.items)
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

        let treeElement = this._addTreeElementForSourceCodeToTreeOutline(resource, this._scriptsContentTreeOutline);
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

        if (!event.data.oldMainResource) {
            var resource = event.target.mainResource;
            this._addTreeElementForSourceCodeToTreeOutline(resource, this._scriptsContentTreeOutline);
            this._addBreakpointsForSourceCode(resource);
            this._addIssuesForSourceCode(resource);
        }
    }

    _timelineCapturingWillStart(event)
    {
        this._debuggerBreakpointsButtonItem.enabled = false;
        this._debuggerPauseResumeButtonItem.enabled = false;

        this.contentView.element.insertBefore(this._timelineRecordingWarningElement, this.contentView.element.firstChild);
        this._updateBreakpointsDisabledBanner();
    }

    _timelineCapturingStopped(event)
    {
        this._debuggerBreakpointsButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.enabled = true;

        this._timelineRecordingWarningElement.remove();
        this._updateBreakpointsDisabledBanner();
    }

    _updateBreakpointsDisabledBanner()
    {
        let breakpointsEnabled = WebInspector.debuggerManager.breakpointsEnabled;
        let timelineWarningShowing = !!this._timelineRecordingWarningElement.parentElement;

        if (!breakpointsEnabled && !timelineWarningShowing)
            this.contentView.element.insertBefore(this._breakpointsDisabledWarningElement, this.contentView.element.firstChild);
        else
            this._breakpointsDisabledWarningElement.remove();
    }

    _scriptAdded(event)
    {
        this._addScript(event.data.script);
    }

    _addScript(script)
    {
        // COMPATIBILITY(iOS 9): Backends could send the frontend built-in code, filter out JSC internals.
        if (!script.url && !script.sourceURL)
            return null;

        // In general, do not show dynamically added script elements.
        if (script.dynamicallyAddedScriptElement && !script.sourceURL)
            return null;

        // Don't add breakpoints if the script is represented by a Resource. They were
        // already added by _resourceAdded.
        if (script.resource)
            return null;

        let treeElement = this._addTreeElementForSourceCodeToTreeOutline(script, this._scriptsContentTreeOutline);
        this._addBreakpointsForSourceCode(script);
        this._addIssuesForSourceCode(script);

        if (!this.contentBrowser.currentContentView)
            this.showDefaultContentViewForTreeElement(treeElement);

        return treeElement;
    }

    _scriptRemoved(event)
    {
        function removeScript(script, treeOutline)
        {
            let scriptTreeElement = treeOutline.getCachedTreeElement(script);
            if (scriptTreeElement)
                scriptTreeElement.parent.removeChild(scriptTreeElement);
        }

        let script = event.data.script;
        removeScript(script, this._breakpointsContentTreeOutline);
        removeScript(script, this._scriptsContentTreeOutline);
    }

    _scriptsCleared(event)
    {
        for (var i = this._breakpointsContentTreeOutline.children.length - 1; i >= 0; --i) {
            var treeElement = this._breakpointsContentTreeOutline.children[i];
            if (!(treeElement instanceof WebInspector.ScriptTreeElement))
                continue;

            this._breakpointsContentTreeOutline.removeChildAtIndex(i, true, true);
        }

        this._scriptsContentTreeOutline.removeChildren();

        this._addResourcesRecursivelyForFrame(WebInspector.frameResourceManager.mainFrame);
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

    _findThreadTreeElementForTarget(target)
    {
        for (let child of this._callStackTreeOutline.children) {
            if (child.target === target)
                return child;
        }

        return null;
    }

    _targetAdded(event)
    {
        let target = event.data.target;
        let treeElement = new WebInspector.ThreadTreeElement(target);
        this._callStackTreeOutline.appendChild(treeElement);

        this._updateCallStackTreeOutline();
    }

    _targetRemoved(event)
    {
        let target = event.data.target;
        let treeElement = this._findThreadTreeElementForTarget(target);
        this._callStackTreeOutline.removeChild(treeElement);

        this._updateCallStackTreeOutline();
    }

    _updateCallStackTreeOutline()
    {
        let singleThreadShowing = WebInspector.targets.size === 1;
        this._callStackTreeOutline.element.classList.toggle("single-thread", singleThreadShowing);
        this._mainTargetTreeElement.selectable = !singleThreadShowing;
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
        // If this is a BreakpointTreeElement being deleted because of a cause
        // outside of the TreeOutline then deselect if it is selected to avoid
        // TreeOutline selection changes causing unexpected ContentView changes.
        if (!debuggerTreeElement.__deletedViaDeleteKeyboardShortcut)
            debuggerTreeElement.deselect();

        let parentTreeElement = debuggerTreeElement.parent;
        parentTreeElement.removeChild(debuggerTreeElement);

        console.assert(parentTreeElement.parent === this._breakpointsContentTreeOutline);
        if (parentTreeElement.children.length)
            return;

        parentTreeElement.treeOutline.removeChild(parentTreeElement);
    }

    _debuggerCallFramesDidChange(event)
    {
        let target = event.data.target;
        let treeElement = this._findThreadTreeElementForTarget(target);
        treeElement.refresh();
    }

    _debuggerActiveCallFrameDidChange()
    {
        if (this._activeCallFrameTreeElement) {
            this._activeCallFrameTreeElement.isActiveCallFrame = false;
            this._activeCallFrameTreeElement = null;
        }

        if (!WebInspector.debuggerManager.activeCallFrame)
            return;

        this._activeCallFrameTreeElement = this._callStackTreeOutline.findTreeElement(WebInspector.debuggerManager.activeCallFrame);
        if (this._activeCallFrameTreeElement)
            this._activeCallFrameTreeElement.isActiveCallFrame = true;
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

        var wasTopResourceTreeElement = treeElement.previousSibling === this._assertionsBreakpointTreeElement || treeElement.previousSibling === this._allUncaughtExceptionsBreakpointTreeElement;
        var nextSibling = treeElement.nextSibling;

        var breakpoints = this._breakpointsBeneathTreeElement(treeElement);
        this._removeAllBreakpoints(breakpoints);

        if (wasTopResourceTreeElement && nextSibling)
            nextSibling.select(true, true);

        return true;
    }

    _breakpointTreeOutlineContextMenuTreeElement(contextMenu, event, treeElement)
    {
        // This check is necessary since the context menu is created by the TreeOutline, meaning
        // that any child could be the target of the context menu event.
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

        if (treeElement instanceof WebInspector.ResourceTreeElement || treeElement instanceof WebInspector.ScriptTreeElement) {
            WebInspector.showSourceCode(treeElement.representedObject);
            return;
        }

        if (treeElement instanceof WebInspector.CallFrameTreeElement) {
            let callFrame = treeElement.callFrame;
            if (callFrame.id)
                WebInspector.debuggerManager.activeCallFrame = callFrame;

            if (callFrame.sourceCodeLocation)
                WebInspector.showSourceCodeLocation(callFrame.sourceCodeLocation);
            return;
        }

        if (treeElement instanceof WebInspector.IssueTreeElement) {
            WebInspector.showSourceCodeLocation(treeElement.issueMessage.sourceCodeLocation);
            return;
        }

        if (!(treeElement instanceof WebInspector.BreakpointTreeElement))
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
        function isSpecialBreakpoint(treeElement)
        {
            return treeElement.representedObject === WebInspector.debuggerManager.allExceptionsBreakpoint
                || treeElement.representedObject === WebInspector.debuggerManager.allUncaughtExceptionsBreakpoint
                || treeElement.representedObject === WebInspector.debuggerManager.assertionsBreakpoint;
        }

        if (isSpecialBreakpoint(a))
            return -1;
        if (isSpecialBreakpoint(b))
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
        let target = WebInspector.debuggerManager.activeCallFrame.target;
        let targetData = WebInspector.debuggerManager.dataForTarget(target);
        let {pauseReason, pauseData} = targetData;

        switch (pauseReason) {
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
                const suppressFiltering = true;
                this._pauseReasonTreeOutline = this.createContentTreeOutline(suppressFiltering);
                this._pauseReasonTreeOutline.addEventListener(WebInspector.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

                let breakpoint = WebInspector.debuggerManager.breakpointForIdentifier(pauseData.breakpointId);
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
                var data = WebInspector.RemoteObject.fromPayload(pauseData, target);
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

        return null;
    }

    _addIssue(issueMessage)
    {
        let issueTreeElement = this._scriptsContentTreeOutline.findTreeElement(issueMessage);
        if (issueTreeElement)
            return issueTreeElement;

        let parentTreeElement = this._addTreeElementForSourceCodeToTreeOutline(issueMessage.sourceCodeLocation.sourceCode, this._scriptsContentTreeOutline);
        if (!parentTreeElement)
            return null;

        issueTreeElement = new WebInspector.IssueTreeElement(issueMessage);

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
        let currentTreeElement = this._scriptsContentTreeOutline.children[0];
        let issueTreeElements = [];

        while (currentTreeElement && !currentTreeElement.root) {
            if (currentTreeElement instanceof WebInspector.IssueTreeElement)
                issueTreeElements.push(currentTreeElement);
            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, true);
        }

        issueTreeElements.forEach((treeElement) => treeElement.parent.removeChild(treeElement));
    }
};

WebInspector.DebuggerSidebarPanel.DebuggerPausedStyleClassName = "paused";
WebInspector.DebuggerSidebarPanel.ExceptionIconStyleClassName = "breakpoint-exception-icon";
WebInspector.DebuggerSidebarPanel.AssertionIconStyleClassName = "breakpoint-assertion-icon";
WebInspector.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName = "breakpoint-paused-icon";

WebInspector.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey = "debugger-sidebar-panel-all-exceptions-breakpoint";
WebInspector.DebuggerSidebarPanel.SelectedAllUncaughtExceptionsCookieKey = "debugger-sidebar-panel-all-uncaught-exceptions-breakpoint";
WebInspector.DebuggerSidebarPanel.SelectedAssertionsCookieKey = "debugger-sidebar-panel-assertions-breakpoint";
