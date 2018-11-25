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

WI.DebuggerSidebarPanel = class DebuggerSidebarPanel extends WI.NavigationSidebarPanel
{
    constructor()
    {
        super("debugger", WI.UIString("Debugger"), true);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, this._resourceAdded, this);
        WI.Target.addEventListener(WI.Target.Event.ResourceAdded, this._resourceAdded, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointAdded, this._breakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, this._breakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.EventBreakpointAdded, this._breakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.XHRBreakpointAdded, this._breakpointAdded, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointRemoved, this._breakpointRemoved, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, this._breakpointRemoved, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, this._breakpointRemoved, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.XHRBreakpointRemoved, this._breakpointRemoved, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointsEnabledDidChange, this._breakpointsEnabledDidChange, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ScriptAdded, this._scriptAdded, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ScriptRemoved, this._scriptRemoved, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ScriptsCleared, this._scriptsCleared, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, this._debuggerDidPause, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Resumed, this._debuggerDidResume, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.CallFramesDidChange, this._debuggerCallFramesDidChange, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._debuggerActiveCallFrameDidChange, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.WaitingToPause, this._debuggerWaitingToPause, this);

        WI.DOMBreakpoint.addEventListener(WI.DOMBreakpoint.Event.ResolvedStateDidChange, this._domBreakpointResolvedStateDidChange, this);

        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingWillStart, this._timelineCapturingWillStart, this);
        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStopped, this._timelineCapturingStopped, this);

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetAdded, this._targetAdded, this);
        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._targetRemoved, this);

        this._timelineRecordingWarningElement = document.createElement("div");
        this._timelineRecordingWarningElement.classList.add("warning-banner");
        this._timelineRecordingWarningElement.append(WI.UIString("Debugger disabled during Timeline recording"), " ");
        let stopRecordingLink = this._timelineRecordingWarningElement.appendChild(document.createElement("a"));
        stopRecordingLink.textContent = WI.UIString("Stop recording");
        stopRecordingLink.addEventListener("click", () => { WI.timelineManager.stopCapturing(); });

        this._breakpointsDisabledWarningElement = document.createElement("div");
        this._breakpointsDisabledWarningElement.classList.add("warning-banner");
        this._breakpointsDisabledWarningElement.append(WI.UIString("Breakpoints disabled"), document.createElement("br"));
        let enableBreakpointsLink = this._breakpointsDisabledWarningElement.appendChild(document.createElement("a"));
        enableBreakpointsLink.textContent = WI.UIString("Enable breakpoints");
        enableBreakpointsLink.addEventListener("click", () => { WI.debuggerToggleBreakpoints(); });

        this._navigationBar = new WI.NavigationBar;
        this.addSubview(this._navigationBar);

        var breakpointsImage = {src: "Images/Breakpoints.svg", width: 15, height: 15};
        var pauseImage = {src: "Images/Pause.svg", width: 15, height: 15};
        var resumeImage = {src: "Images/Resume.svg", width: 15, height: 15};
        var stepOverImage = {src: "Images/StepOver.svg", width: 15, height: 15};
        var stepIntoImage = {src: "Images/StepInto.svg", width: 15, height: 15};
        var stepOutImage = {src: "Images/StepOut.svg", width: 15, height: 15};

        var toolTip = WI.UIString("Enable all breakpoints (%s)").format(WI.toggleBreakpointsKeyboardShortcut.displayName);
        var altToolTip = WI.UIString("Disable all breakpoints (%s)").format(WI.toggleBreakpointsKeyboardShortcut.displayName);

        this._debuggerBreakpointsButtonItem = new WI.ActivateButtonNavigationItem("debugger-breakpoints", toolTip, altToolTip, breakpointsImage.src, breakpointsImage.width, breakpointsImage.height);
        this._debuggerBreakpointsButtonItem.activated = WI.debuggerManager.breakpointsEnabled;
        this._debuggerBreakpointsButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerToggleBreakpoints, this);
        this._navigationBar.addNavigationItem(this._debuggerBreakpointsButtonItem);

        toolTip = WI.UIString("Pause script execution (%s or %s)").format(WI.pauseOrResumeKeyboardShortcut.displayName, WI.pauseOrResumeAlternateKeyboardShortcut.displayName);
        altToolTip = WI.UIString("Continue script execution (%s or %s)").format(WI.pauseOrResumeKeyboardShortcut.displayName, WI.pauseOrResumeAlternateKeyboardShortcut.displayName);

        this._debuggerPauseResumeButtonItem = new WI.ToggleButtonNavigationItem("debugger-pause-resume", toolTip, altToolTip, pauseImage.src, resumeImage.src, pauseImage.width, pauseImage.height);
        this._debuggerPauseResumeButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerPauseResumeToggle, this);
        this._navigationBar.addNavigationItem(this._debuggerPauseResumeButtonItem);

        this._debuggerStepOverButtonItem = new WI.ButtonNavigationItem("debugger-step-over", WI.UIString("Step over (%s or %s)").format(WI.stepOverKeyboardShortcut.displayName, WI.stepOverAlternateKeyboardShortcut.displayName), stepOverImage.src, stepOverImage.width, stepOverImage.height);
        this._debuggerStepOverButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerStepOver, this);
        this._debuggerStepOverButtonItem.enabled = false;
        this._navigationBar.addNavigationItem(this._debuggerStepOverButtonItem);

        this._debuggerStepIntoButtonItem = new WI.ButtonNavigationItem("debugger-step-into", WI.UIString("Step into (%s or %s)").format(WI.stepIntoKeyboardShortcut.displayName, WI.stepIntoAlternateKeyboardShortcut.displayName), stepIntoImage.src, stepIntoImage.width, stepIntoImage.height);
        this._debuggerStepIntoButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerStepInto, this);
        this._debuggerStepIntoButtonItem.enabled = false;
        this._navigationBar.addNavigationItem(this._debuggerStepIntoButtonItem);

        this._debuggerStepOutButtonItem = new WI.ButtonNavigationItem("debugger-step-out", WI.UIString("Step out (%s or %s)").format(WI.stepOutKeyboardShortcut.displayName, WI.stepOutAlternateKeyboardShortcut.displayName), stepOutImage.src, stepOutImage.width, stepOutImage.height);
        this._debuggerStepOutButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerStepOut, this);
        this._debuggerStepOutButtonItem.enabled = false;
        this._navigationBar.addNavigationItem(this._debuggerStepOutButtonItem);

        // Add this offset-sections class name so the sticky headers don't overlap the navigation bar.
        this.element.classList.add(WI.DebuggerSidebarPanel.OffsetSectionsStyleClassName);

        function showResourcesWithIssuesOnlyFilterFunction(treeElement)
        {
            // Issues are only shown in the scripts tree outline.
            if (treeElement.treeOutline !== this._scriptsContentTreeOutline)
                return true;

            // Keep issues.
            if (treeElement instanceof WI.IssueTreeElement)
                return true;

            // Keep resources with issues.
            if (treeElement.hasChildren) {
                for (let child of treeElement.children) {
                    if (child instanceof WI.IssueTreeElement)
                        return true;
                }
            }
            return false;
        }

        this.filterBar.addFilterBarButton("debugger-show-resources-with-issues-only", showResourcesWithIssuesOnlyFilterFunction.bind(this), false, WI.UIString("Only show resources with issues"), WI.UIString("Show all resources"), "Images/Errors.svg", 15, 15);

        this._breakpointsContentTreeOutline = this.contentTreeOutline;

        let breakpointsRow = new WI.DetailsSectionRow;
        breakpointsRow.element.appendChild(this._breakpointsContentTreeOutline.element);

        let breakpointNavigationBarWrapper = document.createElement("div");

        let breakpointNavigationBar = new WI.NavigationBar;
        breakpointNavigationBarWrapper.appendChild(breakpointNavigationBar.element);

        let createBreakpointButton = new WI.ButtonNavigationItem("create-breakpoint", WI.UIString("Create Breakpoint"), "Images/Plus13.svg", 13, 13);
        createBreakpointButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleCreateBreakpointClicked, this);
        breakpointNavigationBar.addNavigationItem(createBreakpointButton);

        let breakpointsGroup = new WI.DetailsSectionGroup([breakpointsRow]);
        let breakpointsSection = new WI.DetailsSection("breakpoints", WI.UIString("Breakpoints"), [breakpointsGroup], breakpointNavigationBarWrapper);
        this.contentView.element.appendChild(breakpointsSection.element);

        this._breakpointsContentTreeOutline.addEventListener(WI.TreeOutline.Event.ElementAdded, this._handleBreakpointElementAddedOrRemoved, this);
        this._breakpointsContentTreeOutline.addEventListener(WI.TreeOutline.Event.ElementRemoved, this._handleBreakpointElementAddedOrRemoved, this);
        this._breakpointsContentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        this._breakpointsContentTreeOutline.ondelete = this._breakpointTreeOutlineDeleteTreeElement.bind(this);
        this._breakpointsContentTreeOutline.populateContextMenu = function(contextMenu, event, treeElement) {
            this._breakpointTreeOutlineContextMenuTreeElement(contextMenu, event, treeElement);

            WI.TreeOutline.prototype.populateContextMenu(contextMenu, event, treeElement);
        }.bind(this);

        this._scriptsContentTreeOutline = this.createContentTreeOutline();
        this._scriptsContentTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        this._scriptsContentTreeOutline.includeSourceMapResourceChildren = true;

        let scriptsRow = new WI.DetailsSectionRow;
        scriptsRow.element.appendChild(this._scriptsContentTreeOutline.element);

        let scriptsGroup = new WI.DetailsSectionGroup([scriptsRow]);
        this._scriptsSection = new WI.DetailsSection("scripts", WI.UIString("Sources"), [scriptsGroup]);
        this.contentView.element.appendChild(this._scriptsSection.element);

        const suppressFiltering = true;
        this._callStackTreeOutline = this.createContentTreeOutline(suppressFiltering);
        this._callStackTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

        this._callStackRow = new WI.DetailsSectionRow;
        this._callStackRow.element.appendChild(this._callStackTreeOutline.element);

        this._callStackGroup = new WI.DetailsSectionGroup([this._callStackRow]);
        this._callStackSection = new WI.DetailsSection("call-stack", WI.UIString("Call Stack"), [this._callStackGroup]);

        this._mainTargetTreeElement = null;
        this._activeCallFrameTreeElement = null;

        this._pauseReasonTreeOutline = null;

        this._pauseReasonLinkContainerElement = document.createElement("span");
        this._pauseReasonTextRow = new WI.DetailsSectionTextRow;
        this._pauseReasonGroup = new WI.DetailsSectionGroup([this._pauseReasonTextRow]);
        this._pauseReasonSection = new WI.DetailsSection("paused-reason", WI.UIString("Pause Reason"), [this._pauseReasonGroup], this._pauseReasonLinkContainerElement);

        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WI.IssueMessage.addEventListener(WI.IssueMessage.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.IssueAdded, this._handleIssueAdded, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.Cleared, this._handleIssuesCleared, this);

        WI.debuggerManager.addBreakpoint(WI.debuggerManager.allExceptionsBreakpoint);
        WI.debuggerManager.addBreakpoint(WI.debuggerManager.uncaughtExceptionsBreakpoint);

        // COMPATIBILITY (iOS 10): DebuggerAgent.setPauseOnAssertions did not exist yet.
        if (InspectorBackend.domains.Debugger.setPauseOnAssertions && WI.settings.showAssertionFailuresBreakpoint.value)
            WI.debuggerManager.addBreakpoint(WI.debuggerManager.assertionFailuresBreakpoint);

        for (let target of WI.targets)
            this._addTarget(target);
        this._updateCallStackTreeOutline();

        if (WI.networkManager.mainFrame)
            this._addResourcesRecursivelyForFrame(WI.networkManager.mainFrame);

        for (let script of WI.debuggerManager.knownNonResourceScripts)
            this._addScript(script);

        if (WI.domDebuggerManager.supported) {
            if (WI.settings.showAllRequestsBreakpoint.value)
                WI.domDebuggerManager.addXHRBreakpoint(WI.domDebuggerManager.allRequestsBreakpoint);

            for (let eventBreakpoint of WI.domDebuggerManager.eventBreakpoints)
                this._addBreakpoint(eventBreakpoint);

            for (let domBreakpoint of WI.domDebuggerManager.domBreakpoints)
                this._addBreakpoint(domBreakpoint);

            for (let eventListenerBreakpoint of WI.domManager.eventListenerBreakpoints)
                this._addBreakpoint(eventListenerBreakpoint);

            for (let xhrBreakpoints of WI.domDebuggerManager.xhrBreakpoints)
                this._addBreakpoint(xhrBreakpoints);
        }

        if (WI.debuggerManager.paused)
            this._debuggerDidPause(null);

        if (WI.timelineManager.isCapturing() && WI.debuggerManager.breakpointsDisabledTemporarily)
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

        WI.Frame.removeEventListener(null, null, this);
        WI.Target.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);
        WI.domDebuggerManager.removeEventListener(null, null, this);
        WI.DOMBreakpoint.removeEventListener(null, null, this);
        WI.timelineManager.removeEventListener(null, null, this);
        WI.targetManager.removeEventListener(null, null, this);
        WI.Breakpoint.removeEventListener(null, null, this);
        WI.IssueMessage.removeEventListener(null, null, this);
    }

    showDefaultContentView()
    {
        if (WI.networkManager.mainFrame) {
            let mainTreeElement = this._scriptsContentTreeOutline.findTreeElement(WI.networkManager.mainFrame.mainResource);
            if (mainTreeElement && this.showDefaultContentViewForTreeElement(mainTreeElement))
                return;
        }

        let currentTreeElement = this._scriptsContentTreeOutline.children[0];
        while (currentTreeElement && !currentTreeElement.root) {
            if (currentTreeElement instanceof WI.ResourceTreeElement || currentTreeElement instanceof WI.ScriptTreeElement) {
                if (this.showDefaultContentViewForTreeElement(currentTreeElement))
                    return;
            }

            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, true);
        }
    }

    treeElementForRepresentedObject(representedObject)
    {
        // The main resource is used as the representedObject instead of Frame in our tree.
        if (representedObject instanceof WI.Frame)
            representedObject = representedObject.mainResource;

        let treeElement = this._breakpointsContentTreeOutline.findTreeElement(representedObject);
        if (!treeElement)
            treeElement = this._scriptsContentTreeOutline.findTreeElement(representedObject);

        if (treeElement)
            return treeElement;

        // Only special case Script objects.
        if (!(representedObject instanceof WI.Script)) {
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
        if (!selectedTreeElement) {
            super.saveStateToCookie(cookie);
            return;
        }

        var representedObject = selectedTreeElement.representedObject;

        if (representedObject === WI.debuggerManager.allExceptionsBreakpoint) {
            cookie[WI.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey] = true;
            return;
        }

        if (representedObject === WI.debuggerManager.uncaughtExceptionsBreakpoint) {
            cookie[WI.DebuggerSidebarPanel.SelectedUncaughtExceptionsCookieKey] = true;
            return;
        }

        if (representedObject === WI.debuggerManager.assertionFailuresBreakpoint) {
            cookie[WI.DebuggerSidebarPanel.SelectedAssertionFailuresCookieKey] = true;
            return;
        }

        if (representedObject === WI.domDebuggerManager.allRequestsBreakpoint) {
            cookie[WI.DebuggerSidebarPanel.SelectedAllRequestsCookieKey] = true;
            return;
        }

        super.saveStateToCookie(cookie);
    }

    restoreStateFromCookie(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        function revealAndSelect(treeOutline, breakpoint) {
            let treeElement = treeOutline.findTreeElement(breakpoint);
            if (!treeElement)
                return;

            treeElement.revealAndSelect();
        }

        // Eagerly resolve the special breakpoints; otherwise, use the default behavior.
        if (cookie[WI.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey])
            revealAndSelect(this._breakpointsContentTreeOutline, WI.debuggerManager.allExceptionsBreakpoint);
        else if (cookie[WI.DebuggerSidebarPanel.SelectedUncaughtExceptionsCookieKey])
            revealAndSelect(this._breakpointsContentTreeOutline, WI.debuggerManager.uncaughtExceptionsBreakpoint);
        else if (cookie[WI.DebuggerSidebarPanel.SelectedAssertionFailuresCookieKey])
            revealAndSelect(this._breakpointsContentTreeOutline, WI.debuggerManager.assertionFailuresBreakpoint);
        else if (cookie[WI.DebuggerSidebarPanel.SelectedAllRequestsCookieKey])
            revealAndSelect(this._breakpointsContentTreeOutline, WI.domDebuggerManager.allRequestsBreakpoint);
        else
            super.restoreStateFromCookie(cookie, relaxedMatchDelay);
    }

    // Popover delegate

    willDismissPopover(popover)
    {
        let breakpoint = popover.breakpoint;
        if (!breakpoint)
            return;

        if (breakpoint instanceof WI.EventBreakpoint)
            WI.domDebuggerManager.addEventBreakpoint(breakpoint);
        else if (breakpoint instanceof WI.XHRBreakpoint)
            WI.domDebuggerManager.addXHRBreakpoint(breakpoint);
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

        this.element.classList.add(WI.DebuggerSidebarPanel.DebuggerPausedStyleClassName);
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

        this.element.classList.remove(WI.DebuggerSidebarPanel.DebuggerPausedStyleClassName);
    }

    _breakpointsEnabledDidChange(event)
    {
        this._debuggerBreakpointsButtonItem.activated = WI.debuggerManager.breakpointsEnabled;

        this._updateBreakpointsDisabledBanner();
    }

    _addBreakpoint(breakpoint)
    {
        let constructor = WI.BreakpointTreeElement;
        let options = {};
        let parentTreeElement = this._breakpointsContentTreeOutline;

        let getDOMNodeTreeElement = (domNode) => {
            console.assert(domNode, "Missing DOMNode for identifier", breakpoint.domNodeIdentifier);
            if (!domNode)
                return null;

            let domNodeTreeElement = this._breakpointsContentTreeOutline.findTreeElement(domNode);
            if (!domNodeTreeElement) {
                domNodeTreeElement = new WI.DOMNodeTreeElement(domNode);
                this._addTreeElement(domNodeTreeElement, parentTreeElement);
            }
            return domNodeTreeElement;
        };

        if (breakpoint === WI.debuggerManager.allExceptionsBreakpoint) {
            options.className = WI.DebuggerSidebarPanel.ExceptionIconStyleClassName;
            options.title = WI.UIString("All Exceptions");
        } else if (breakpoint === WI.debuggerManager.uncaughtExceptionsBreakpoint) {
            options.className = WI.DebuggerSidebarPanel.ExceptionIconStyleClassName;
            options.title = WI.UIString("Uncaught Exceptions");
        } else if (breakpoint === WI.debuggerManager.assertionFailuresBreakpoint) {
            options.className = WI.DebuggerSidebarPanel.AssertionIconStyleClassName;
            options.title = WI.UIString("Assertion Failures");
        } else if (breakpoint instanceof WI.DOMBreakpoint) {
            if (!breakpoint.domNodeIdentifier)
                return null;

            constructor = WI.DOMBreakpointTreeElement;

            let domNode = WI.domManager.nodeForId(breakpoint.domNodeIdentifier);
            parentTreeElement = getDOMNodeTreeElement(domNode);
        } else if (breakpoint instanceof WI.EventBreakpoint) {
            constructor = WI.EventBreakpointTreeElement;

            if (breakpoint.eventListener)
                parentTreeElement = getDOMNodeTreeElement(breakpoint.eventListener.node);
        } else if (breakpoint instanceof WI.XHRBreakpoint) {
            constructor = WI.XHRBreakpointTreeElement;

            if (breakpoint === WI.domDebuggerManager.allRequestsBreakpoint) {
                options.className = WI.DebuggerSidebarPanel.AssertionIconStyleClassName;
                options.title = WI.UIString("All Requests");
            }
        } else {
            let sourceCode = breakpoint.sourceCodeLocation && breakpoint.sourceCodeLocation.displaySourceCode;
            if (!sourceCode)
                return null;

            if (this._breakpointsContentTreeOutline.findTreeElement(breakpoint))
                return null;

            parentTreeElement = this._addTreeElementForSourceCodeToTreeOutline(sourceCode, this._breakpointsContentTreeOutline);

            // Mark disabled breakpoints as resolved if there is source code loaded with that URL.
            // This gives the illusion the breakpoint was resolved, but since we don't send disabled
            // breakpoints to the backend we don't know for sure. If the user enables the breakpoint
            // it will be resolved properly.
            if (breakpoint.disabled)
                breakpoint.resolved = true;
        }

        let breakpointTreeElement = new constructor(breakpoint, options);
        this._addTreeElement(breakpointTreeElement, parentTreeElement);
        if (parentTreeElement.children.length === 1)
            parentTreeElement.expand();
        return breakpointTreeElement;
    }

    _removeBreakpoint(breakpoint)
    {
        if (this._pauseReasonTreeOutline) {
            let pauseReasonBreakpointTreeElement = this._pauseReasonTreeOutline.getCachedTreeElement(breakpoint);
            if (pauseReasonBreakpointTreeElement)
                pauseReasonBreakpointTreeElement.status = null;
        }

        let breakpointTreeElement = this._breakpointsContentTreeOutline.getCachedTreeElement(breakpoint);
        if (!breakpointTreeElement)
            return;

        this._removeDebuggerTreeElement(breakpointTreeElement);
    }

    _addBreakpointsForSourceCode(sourceCode)
    {
        var breakpoints = WI.debuggerManager.breakpointsForSourceCode(sourceCode);
        for (var i = 0; i < breakpoints.length; ++i)
            this._addBreakpoint(breakpoints[i], sourceCode);
    }

    _addIssuesForSourceCode(sourceCode)
    {
        var issues = WI.consoleManager.issuesForSourceCode(sourceCode);
        for (var issue of issues)
            this._addIssue(issue);
    }

    _addTreeElementForSourceCodeToTreeOutline(sourceCode, treeOutline)
    {
        let treeElement = treeOutline.getCachedTreeElement(sourceCode);
        if (!treeElement) {
            if (sourceCode instanceof WI.SourceMapResource)
                treeElement = new WI.SourceMapResourceTreeElement(sourceCode);
            else if (sourceCode instanceof WI.Resource)
                treeElement = new WI.ResourceTreeElement(sourceCode);
            else if (sourceCode instanceof WI.Script)
                treeElement = new WI.ScriptTreeElement(sourceCode);
        }

        if (!treeElement) {
            console.error("Unknown sourceCode instance", sourceCode);
            return null;
        }

        if (!treeElement.parent) {
            treeElement.hasChildren = false;
            treeElement.expand();

            this._addTreeElement(treeElement, treeOutline);
        }

        return treeElement;
    }

    _addResourcesRecursivelyForFrame(frame)
    {
        this._addResource(frame.mainResource);

        for (let resource of frame.resourceCollection)
            this._addResource(resource);

        for (let childFrame of frame.childFrameCollection)
            this._addResourcesRecursivelyForFrame(childFrame);
    }

    _resourceAdded(event)
    {
        this._addResource(event.data.resource);
    }

    _addResource(resource)
    {
        if (![WI.Resource.Type.Document, WI.Resource.Type.Script].includes(resource.type))
            return;

        let treeElement = this._addTreeElementForSourceCodeToTreeOutline(resource, this._scriptsContentTreeOutline);
        this._addBreakpointsForSourceCode(resource);
        this._addIssuesForSourceCode(resource);

        if (this.parentSidebar && !this.contentBrowser.currentContentView)
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

            for (let domBreakpoint of WI.domDebuggerManager.domBreakpoints)
                this._removeBreakpoint(domBreakpoint);
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
        let breakpointsEnabled = WI.debuggerManager.breakpointsEnabled;
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

        if (this.parentSidebar && !this.contentBrowser.currentContentView)
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
            if (!(treeElement instanceof WI.ScriptTreeElement))
                continue;

            this._breakpointsContentTreeOutline.removeChildAtIndex(i, true, true);
        }

        this._scriptsContentTreeOutline.removeChildren();

        this._addResourcesRecursivelyForFrame(WI.networkManager.mainFrame);
    }

    _breakpointAdded(event)
    {
        var breakpoint = event.data.breakpoint;
        this._addBreakpoint(breakpoint);
    }

    _breakpointRemoved(event)
    {
        var breakpoint = event.data.breakpoint;
        this._removeBreakpoint(breakpoint);
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
        this._addTarget(event.data.target);
    }

    _addTarget(target)
    {
        let treeElement = new WI.ThreadTreeElement(target);
        this._callStackTreeOutline.appendChild(treeElement);

        // FIXME: When WI.mainTarget changes?
        if (target === WI.mainTarget)
            this._mainTargetTreeElement = treeElement;

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
        let singleThreadShowing = WI.targets.length <= 1;
        this._callStackTreeOutline.element.classList.toggle("single-thread", singleThreadShowing);
        if (this._mainTargetTreeElement)
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

        if (parentTreeElement.children.length || parentTreeElement === this._breakpointsContentTreeOutline)
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

        if (!WI.debuggerManager.activeCallFrame)
            return;

        this._activeCallFrameTreeElement = this._callStackTreeOutline.findTreeElement(WI.debuggerManager.activeCallFrame);
        if (this._activeCallFrameTreeElement)
            this._activeCallFrameTreeElement.isActiveCallFrame = true;
    }

    _breakpointsBeneathTreeElement(treeElement)
    {
        console.assert(treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement);
        if (!(treeElement instanceof WI.ResourceTreeElement) && !(treeElement instanceof WI.ScriptTreeElement))
            return [];

        var breakpoints = [];
        var breakpointTreeElements = treeElement.children;
        for (var i = 0; i < breakpointTreeElements.length; ++i) {
            console.assert(breakpointTreeElements[i] instanceof WI.BreakpointTreeElement);
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
            if (WI.debuggerManager.isBreakpointRemovable(breakpoint))
                WI.debuggerManager.removeBreakpoint(breakpoint);
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
        console.assert(treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement);
        if (!(treeElement instanceof WI.ResourceTreeElement) && !(treeElement instanceof WI.ScriptTreeElement))
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
        if (!(treeElement instanceof WI.ResourceTreeElement) && !(treeElement instanceof WI.ScriptTreeElement))
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
            contextMenu.appendItem(WI.UIString("Disable Breakpoints"), toggleAllResourceBreakpoints);
        else
            contextMenu.appendItem(WI.UIString("Enable Breakpoints"), toggleAllResourceBreakpoints);
        contextMenu.appendItem(WI.UIString("Delete Breakpoints"), removeAllResourceBreakpoints);
    }

    _treeSelectionDidChange(event)
    {
        if (!this.selected)
            return;

        let treeElement = event.target.selectedTreeElement;
        if (!treeElement)
            return;

        if (treeElement instanceof WI.DOMNodeTreeElement
            || treeElement instanceof WI.DOMBreakpointTreeElement
            || treeElement instanceof WI.EventBreakpointTreeElement
            || treeElement instanceof WI.XHRBreakpointTreeElement)
            return;

        const options = {
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };

        if (treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement) {
            WI.showSourceCode(treeElement.representedObject, options);
            return;
        }

        if (treeElement instanceof WI.CallFrameTreeElement) {
            let callFrame = treeElement.callFrame;
            if (callFrame.id)
                WI.debuggerManager.activeCallFrame = callFrame;

            if (callFrame.sourceCodeLocation)
                WI.showSourceCodeLocation(callFrame.sourceCodeLocation, options);

            return;
        }

        if (treeElement instanceof WI.IssueTreeElement) {
            WI.showSourceCodeLocation(treeElement.issueMessage.sourceCodeLocation, options);
            return;
        }

        if (!(treeElement instanceof WI.BreakpointTreeElement))
            return;

        let breakpoint = treeElement.breakpoint;
        if (treeElement.treeOutline === this._pauseReasonTreeOutline) {
            WI.showSourceCodeLocation(breakpoint.sourceCodeLocation, options);
            return;
        }

        if (!treeElement.parent.representedObject)
            return;

        console.assert(treeElement.parent.representedObject instanceof WI.SourceCode);
        if (!(treeElement.parent.representedObject instanceof WI.SourceCode))
            return;

        WI.showSourceCodeLocation(breakpoint.sourceCodeLocation, options);
    }

    _addTreeElement(treeElement, parentTreeElement)
    {
        if (!parentTreeElement)
            parentTreeElement = this._breakpointsContentTreeOutline;

        let comparator = (a, b) => {
            const rankFunctions = [
                (treeElement) => treeElement.representedObject === WI.debuggerManager.allExceptionsBreakpoint,
                (treeElement) => treeElement.representedObject === WI.debuggerManager.uncaughtExceptionsBreakpoint,
                (treeElement) => treeElement.representedObject === WI.debuggerManager.assertionFailuresBreakpoint,
                (treeElement) => treeElement.representedObject === WI.domDebuggerManager.allRequestsBreakpoint,
                (treeElement) => treeElement instanceof WI.BreakpointTreeElement || treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement,
                (treeElement) => treeElement instanceof WI.EventBreakpointTreeElement,
                (treeElement) => treeElement instanceof WI.DOMNodeTreeElement,
                (treeElement) => treeElement instanceof WI.DOMBreakpointTreeElement,
                (treeElement) => treeElement instanceof WI.XHRBreakpointTreeElement,
            ];

            let aRank = rankFunctions.findIndex((rankFunction) => rankFunction(a));
            let bRank = rankFunctions.findIndex((rankFunction) => rankFunction(b));
            if (aRank >= 0 && bRank >= 0) {
                if (aRank < bRank)
                    return -1;
                if (bRank < aRank)
                    return 1;
            }

            if (a instanceof WI.BreakpointTreeElement && b instanceof WI.BreakpointTreeElement)
                return this._compareBreakpointTreeElements(a, b);

            return a.mainTitle.extendedLocaleCompare(b.mainTitle) || a.subtitle.extendedLocaleCompare(b.subtitle);
        };

        parentTreeElement.insertChild(treeElement, insertionIndexForObjectInListSortedByFunction(treeElement, parentTreeElement.children, comparator));
    }

    _compareBreakpointTreeElements(a, b)
    {
        if (!a.representedObject || !b.representedObject)
            return 0;

        let aLocation = a.representedObject.sourceCodeLocation;
        let bLocation = b.representedObject.sourceCodeLocation;
        if (!aLocation || !bLocation)
            return 0;

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
        let target = WI.debuggerManager.activeCallFrame.target;
        let targetData = WI.debuggerManager.dataForTarget(target);
        let {pauseReason, pauseData} = targetData;

        switch (pauseReason) {
        case WI.DebuggerManager.PauseReason.AnimationFrame:
            console.assert(pauseData, "Expected data with an animation frame, but found none.");
            if (!pauseData)
                return false;

            var eventBreakpoint = WI.domDebuggerManager.eventBreakpointForTypeAndEventName(WI.EventBreakpoint.Type.AnimationFrame, pauseData.eventName);
            console.assert(eventBreakpoint, "Expected AnimationFrame breakpoint for event name.", pauseData.eventName);
            if (!eventBreakpoint)
                return false;

            var suppressFiltering = true;
            this._pauseReasonTreeOutline = this.createContentTreeOutline(suppressFiltering);

            var eventBreakpointTreeElement = new WI.EventBreakpointTreeElement(eventBreakpoint, {
                className: WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName,
                title: WI.UIString("%s Fired").format(pauseData.eventName),
            });
            this._pauseReasonTreeOutline.appendChild(eventBreakpointTreeElement);

            var eventBreakpointRow = new WI.DetailsSectionRow;
            eventBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

            this._pauseReasonGroup.rows = [eventBreakpointRow];
            return true;

        case WI.DebuggerManager.PauseReason.Assertion:
            // FIXME: We should include the assertion condition string.
            console.assert(pauseData, "Expected data with an assertion, but found none.");
            if (pauseData && pauseData.message) {
                this._pauseReasonTextRow.text = WI.UIString("Assertion with message: %s").format(pauseData.message);
                return true;
            }

            this._pauseReasonTextRow.text = WI.UIString("Assertion Failed");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.Breakpoint:
            console.assert(pauseData, "Expected breakpoint identifier, but found none.");
            if (pauseData && pauseData.breakpointId) {
                const suppressFiltering = true;
                this._pauseReasonTreeOutline = this.createContentTreeOutline(suppressFiltering);
                this._pauseReasonTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);

                let breakpoint = WI.debuggerManager.breakpointForIdentifier(pauseData.breakpointId);
                let breakpointTreeElement = new WI.BreakpointTreeElement(breakpoint, {
                    className: WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName,
                    title: WI.UIString("Triggered Breakpoint"),
                });
                let breakpointDetailsSection = new WI.DetailsSectionRow;
                this._pauseReasonTreeOutline.appendChild(breakpointTreeElement);
                breakpointDetailsSection.element.appendChild(this._pauseReasonTreeOutline.element);

                this._pauseReasonGroup.rows = [breakpointDetailsSection];
                return true;
            }
            break;

        case WI.DebuggerManager.PauseReason.CSPViolation:
            console.assert(pauseData, "Expected data with a CSP Violation, but found none.");
            if (pauseData) {
                // COMPATIBILITY (iOS 8): 'directive' was 'directiveText'.
                this._pauseReasonTextRow.text = WI.UIString("Content Security Policy violation of directive: %s").format(pauseData.directive || pauseData.directiveText);
                this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
                return true;
            }
            break;

        case WI.DebuggerManager.PauseReason.DebuggerStatement:
            this._pauseReasonTextRow.text = WI.UIString("Debugger Statement");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.DOM:
            console.assert(WI.domDebuggerManager.supported);
            console.assert(pauseData, "Expected DOM breakpoint data, but found none.");
            if (pauseData && pauseData.nodeId) {
                let domNode = WI.domManager.nodeForId(pauseData.nodeId);
                let domBreakpoints = WI.domDebuggerManager.domBreakpointsForNode(domNode);
                let domBreakpoint;
                for (let breakpoint of domBreakpoints) {
                    if (breakpoint.type === pauseData.type) {
                        domBreakpoint = breakpoint;
                        break;
                    }
                }

                if (!domBreakpoint)
                    return;

                const suppressFiltering = true;
                this._pauseReasonTreeOutline = this.createContentTreeOutline(suppressFiltering);

                let type = WI.DOMBreakpointTreeElement.displayNameForType(domBreakpoint.type);
                let domBreakpointTreeElement = new WI.DOMBreakpointTreeElement(domBreakpoint, {
                    className: WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName,
                    title: type,
                });
                let domBreakpointRow = new WI.DetailsSectionRow;
                this._pauseReasonTreeOutline.appendChild(domBreakpointTreeElement);
                domBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

                let ownerElementRow = new WI.DetailsSectionSimpleRow(WI.UIString("Element"), WI.linkifyNodeReference(domNode));
                this._pauseReasonGroup.rows = [domBreakpointRow, ownerElementRow];

                if (domBreakpoint.type !== WI.DOMBreakpoint.Type.SubtreeModified)
                    return true;

                console.assert(pauseData.targetNode);

                let remoteObject = WI.RemoteObject.fromPayload(pauseData.targetNode, target);
                remoteObject.pushNodeToFrontend((nodeId) => {
                    if (!nodeId)
                        return;

                    let node = WI.domManager.nodeForId(nodeId);
                    console.assert(node, "Missing node for id.", nodeId);
                    if (!node)
                        return;

                    let fragment = document.createDocumentFragment();
                    let description = pauseData.insertion ? WI.UIString("Child added to ") : WI.UIString("Removed descendant ");
                    fragment.append(description, WI.linkifyNodeReference(node));

                    let targetDescriptionRow = new WI.DetailsSectionSimpleRow(WI.UIString("Details"), fragment);
                    targetDescriptionRow.element.classList.add("target-description");

                    this._pauseReasonGroup.rows = this._pauseReasonGroup.rows.concat(targetDescriptionRow);
                });

                return true;
            }
            break;

        case WI.DebuggerManager.PauseReason.EventListener:
            console.assert(pauseData, "Expected data with an event listener, but found none.");
            if (!pauseData)
                return false;

            var eventBreakpoint = null;
            if (pauseData.eventListenerId)
                eventBreakpoint = WI.domManager.breakpointForEventListenerId(pauseData.eventListenerId);
            if (!eventBreakpoint)
                eventBreakpoint = WI.domDebuggerManager.eventBreakpointForTypeAndEventName(WI.EventBreakpoint.Type.Listener, pauseData.eventName);

            console.assert(eventBreakpoint, "Expected Event Listener breakpoint for event name.", pauseData.eventName);
            if (!eventBreakpoint)
                return false;

            var suppressFiltering = true;
            this._pauseReasonTreeOutline = this.createContentTreeOutline(suppressFiltering);

            var eventBreakpointTreeElement = new WI.EventBreakpointTreeElement(eventBreakpoint, {
                className: WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName,
                title: WI.UIString("“%s“ Event Fired").format(pauseData.eventName),
            });
            this._pauseReasonTreeOutline.appendChild(eventBreakpointTreeElement);

            var eventBreakpointRow = new WI.DetailsSectionRow;
            eventBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

            var rows = [eventBreakpointRow];

            var eventListener = eventBreakpoint.eventListener;
            if (eventListener) {
                console.assert(eventListener.eventListenerId === pauseData.eventListenerId);

                let ownerElementRow = new WI.DetailsSectionSimpleRow(WI.UIString("Element"), WI.linkifyNodeReference(eventListener.node));
                rows.push(ownerElementRow);
            }

            this._pauseReasonGroup.rows = rows;
            return true;

        case WI.DebuggerManager.PauseReason.Exception:
            console.assert(pauseData, "Expected data with an exception, but found none.");
            if (pauseData) {
                // FIXME: We should improve the appearance of thrown objects. This works well for exception strings.
                var data = WI.RemoteObject.fromPayload(pauseData, target);
                this._pauseReasonTextRow.text = WI.UIString("Exception with thrown value: %s").format(data.description);
                this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
                return true;
            }
            break;

        case WI.DebuggerManager.PauseReason.PauseOnNextStatement:
            this._pauseReasonTextRow.text = WI.UIString("Immediate Pause Requested");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.Timer:
            console.assert(pauseData, "Expected data with a timer, but found none.");
            if (!pauseData)
                return false;

            var eventBreakpoint = WI.domDebuggerManager.eventBreakpointForTypeAndEventName(WI.EventBreakpoint.Type.Timer, pauseData.eventName);
            console.assert(eventBreakpoint, "Expected Timer breakpoint for event name.", pauseData.eventName);
            if (!eventBreakpoint)
                return false;

            var suppressFiltering = true;
            this._pauseReasonTreeOutline = this.createContentTreeOutline(suppressFiltering);

            var eventBreakpointTreeElement = new WI.EventBreakpointTreeElement(eventBreakpoint, {
                className: WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName,
                title: WI.UIString("%s Fired").format(pauseData.eventName),
            });
            this._pauseReasonTreeOutline.appendChild(eventBreakpointTreeElement);

            var eventBreakpointRow = new WI.DetailsSectionRow;
            eventBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

            this._pauseReasonGroup.rows = [eventBreakpointRow];
            return true;

        case WI.DebuggerManager.PauseReason.XHR:
            console.assert(WI.domDebuggerManager.supported);
            console.assert(pauseData, "Expected XHR breakpoint data, but found none.");
            if (pauseData) {
                if (pauseData.breakpointURL) {
                    let xhrBreakpoint = WI.domDebuggerManager.xhrBreakpointForURL(pauseData.breakpointURL);
                    console.assert(xhrBreakpoint, "Expected XHR breakpoint for URL.", pauseData.breakpointURL);

                    this._pauseReasonTreeOutline = this.createContentTreeOutline(true);

                    let xhrBreakpointTreeElement = new WI.XHRBreakpointTreeElement(xhrBreakpoint, {
                        className: WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName,
                        title: WI.UIString("Triggered XHR Breakpoint"),
                    });
                    let xhrBreakpointRow = new WI.DetailsSectionRow;
                    this._pauseReasonTreeOutline.appendChild(xhrBreakpointTreeElement);
                    xhrBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

                    this._pauseReasonTextRow.text = pauseData.url;
                    this._pauseReasonGroup.rows = [xhrBreakpointRow, this._pauseReasonTextRow];
                } else {
                    console.assert(pauseData.breakpointURL === "", "Should be the All Requests breakpoint which has an empty URL");
                    this._pauseReasonTextRow.text = WI.UIString("Requesting: %s").format(pauseData.url);
                    this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
                }
                this._pauseReasonTextRow.element.title = pauseData.url;
                return true;
            }
            break;

        case WI.DebuggerManager.PauseReason.Other:
            console.error("Paused for unknown reason. We should always have a reason.");
            break;
        }

        return false;
    }

    _updatePauseReasonGotoArrow()
    {
        this._pauseReasonLinkContainerElement.removeChildren();

        var activeCallFrame = WI.debuggerManager.activeCallFrame;
        if (!activeCallFrame)
            return;

        var sourceCodeLocation = activeCallFrame.sourceCodeLocation;
        if (!sourceCodeLocation)
            return;

        const options = {
            useGoToArrowButton: true,
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };
        let linkElement = WI.createSourceCodeLocationLink(sourceCodeLocation, options);
        this._pauseReasonLinkContainerElement.appendChild(linkElement);
    }

    _addDebuggerObject(debuggerObject)
    {
        if (debuggerObject instanceof WI.Breakpoint)
            return this._addBreakpoint(debuggerObject);

        if (debuggerObject instanceof WI.IssueMessage)
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

        issueTreeElement = new WI.IssueTreeElement(issueMessage);

        parentTreeElement.insertChild(issueTreeElement, insertionIndexForObjectInListSortedByFunction(issueTreeElement, parentTreeElement.children, this._compareBreakpointTreeElements));
        if (parentTreeElement.children.length === 1)
            parentTreeElement.expand();

        return issueTreeElement;
    }

    _domBreakpointResolvedStateDidChange(event)
    {
        let breakpoint = event.target;
        if (breakpoint.domNodeIdentifier)
            this._addBreakpoint(breakpoint);
        else
            this._removeBreakpoint(breakpoint);
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
            if (currentTreeElement instanceof WI.IssueTreeElement)
                issueTreeElements.push(currentTreeElement);
            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, true);
        }

        issueTreeElements.forEach((treeElement) => treeElement.parent.removeChild(treeElement));
    }

    _handleBreakpointElementAddedOrRemoved(event)
    {
        let treeElement = event.data.element;

        let setting = null;
        if (treeElement.breakpoint === WI.debuggerManager.assertionFailuresBreakpoint)
            setting = WI.settings.showAssertionFailuresBreakpoint;
        else if (treeElement.representedObject === WI.domDebuggerManager.allRequestsBreakpoint)
            setting = WI.settings.showAllRequestsBreakpoint;

        if (setting)
            setting.value = !!treeElement.parent;
    }

    _handleCreateBreakpointClicked(event)
    {
        let contextMenu = WI.ContextMenu.createFromEvent(event.data.nativeEvent);

        // COMPATIBILITY (iOS 10): DebuggerAgent.setPauseOnAssertions did not exist yet.
        if (InspectorBackend.domains.Debugger.setPauseOnAssertions) {
            let assertionFailuresBreakpointShown = WI.settings.showAssertionFailuresBreakpoint.value;

            contextMenu.appendCheckboxItem(WI.UIString("Assertion Failures"), () => {
                if (assertionFailuresBreakpointShown)
                    WI.debuggerManager.removeBreakpoint(WI.debuggerManager.assertionFailuresBreakpoint);
                else {
                    WI.debuggerManager.assertionFailuresBreakpoint.disabled = false;
                    WI.debuggerManager.addBreakpoint(WI.debuggerManager.assertionFailuresBreakpoint);
                }
            }, assertionFailuresBreakpointShown);
        }

        if (WI.domDebuggerManager.supported) {
            contextMenu.appendSeparator();

            contextMenu.appendItem(WI.UIString("Event Breakpoint\u2026"), () => {
                let popover = new WI.EventBreakpointPopover(this);
                popover.show(event.target.element, [WI.RectEdge.MAX_Y, WI.RectEdge.MIN_Y, WI.RectEdge.MAX_X]);
            });

            contextMenu.appendSeparator();

            let allRequestsBreakpointShown = WI.settings.showAllRequestsBreakpoint.value;

            contextMenu.appendCheckboxItem(WI.UIString("All Requests"), () => {
                if (allRequestsBreakpointShown)
                    WI.domDebuggerManager.removeXHRBreakpoint(WI.domDebuggerManager.allRequestsBreakpoint);
                else {
                    WI.domDebuggerManager.allRequestsBreakpoint.disabled = false;
                    WI.domDebuggerManager.addXHRBreakpoint(WI.domDebuggerManager.allRequestsBreakpoint);
                }
            }, allRequestsBreakpointShown);

            contextMenu.appendItem(WI.UIString("XHR Breakpoint\u2026"), () => {
                let popover = new WI.XHRBreakpointPopover(this);
                popover.show(event.target.element, [WI.RectEdge.MAX_Y, WI.RectEdge.MIN_Y, WI.RectEdge.MAX_X]);
            });
        }

        contextMenu.show();
    }
};

WI.DebuggerSidebarPanel.DebuggerPausedStyleClassName = "paused";
WI.DebuggerSidebarPanel.ExceptionIconStyleClassName = "breakpoint-exception-icon";
WI.DebuggerSidebarPanel.AssertionIconStyleClassName = "breakpoint-assertion-icon";
WI.DebuggerSidebarPanel.PausedBreakpointIconStyleClassName = "breakpoint-paused-icon";

WI.DebuggerSidebarPanel.SelectedAllExceptionsCookieKey = "debugger-sidebar-panel-all-exceptions-breakpoint";
WI.DebuggerSidebarPanel.SelectedUncaughtExceptionsCookieKey = "debugger-sidebar-panel-uncaught-exceptions-breakpoint";
WI.DebuggerSidebarPanel.SelectedAssertionFailuresCookieKey = "debugger-sidebar-panel-assertion-failures-breakpoint";
WI.DebuggerSidebarPanel.SelectedAllRequestsCookieKey = "debugger-sidebar-panel-all-requests-breakpoint";
