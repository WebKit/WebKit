/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.SourcesNavigationSidebarPanel = class SourcesNavigationSidebarPanel extends WI.NavigationSidebarPanel
{
    constructor()
    {
        const shouldAutoPruneStaleTopLevelResourceTreeElements = true;
        super("sources", WI.UIString("Sources"), shouldAutoPruneStaleTopLevelResourceTreeElements);

        this._debuggerNavigationBar = new WI.NavigationBar;
        this.addSubview(this._debuggerNavigationBar);

        let createActivateNavigationItem = ({identifier, defaultToolTip, activatedToolTip, image}) => {
            let navigationItem = new WI.ActivateButtonNavigationItem(identifier, defaultToolTip, activatedToolTip, image, 15, 15);
            this._debuggerNavigationBar.addNavigationItem(navigationItem);
            return navigationItem;
        };

        let createToggleNavigationItem = ({identifier, defaultToolTip, alternateToolTip, defaultImage, alternateImage}) => {
            let navigationItem = new WI.ToggleButtonNavigationItem(identifier, defaultToolTip, alternateToolTip, defaultImage, alternateImage, 15, 15);
            this._debuggerNavigationBar.addNavigationItem(navigationItem);
            return navigationItem;
        };

        let createButtonNavigationitem = ({identifier, toolTipOrLabel, image}) => {
            let navigationItem = new WI.ButtonNavigationItem(identifier, toolTipOrLabel, image, 15, 15);
            this._debuggerNavigationBar.addNavigationItem(navigationItem);
            return navigationItem;
        };

        this._debuggerBreakpointsButtonItem = createActivateNavigationItem({
            identifier: "debugger-breakpoints",
            defaultToolTip: WI.UIString("Enable all breakpoints (%s)").format(WI.toggleBreakpointsKeyboardShortcut.displayName),
            activatedToolTip: WI.UIString("Disable all breakpoints (%s)").format(WI.toggleBreakpointsKeyboardShortcut.displayName),
            image: "Images/Breakpoints.svg",
        });
        this._debuggerBreakpointsButtonItem.activated = WI.debuggerManager.breakpointsEnabled;
        this._debuggerBreakpointsButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerToggleBreakpoints, this);

        this._debuggerPauseResumeButtonItem = createToggleNavigationItem({
            identifier: "debugger-pause-resume",
            defaultToolTip: WI.UIString("Pause script execution (%s or %s)").format(WI.pauseOrResumeKeyboardShortcut.displayName, WI.pauseOrResumeAlternateKeyboardShortcut.displayName),
            alternateToolTip: WI.UIString("Continue script execution (%s or %s)").format(WI.pauseOrResumeKeyboardShortcut.displayName, WI.pauseOrResumeAlternateKeyboardShortcut.displayName),
            defaultImage: "Images/Pause.svg",
            alternateImage: "Images/Resume.svg",
        });
        this._debuggerPauseResumeButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerPauseResumeToggle, this);

        this._debuggerStepOverButtonItem = createButtonNavigationitem({
            identifier: "debugger-step-over",
            toolTipOrLabel: WI.UIString("Step over (%s or %s)").format(WI.stepOverKeyboardShortcut.displayName, WI.stepOverAlternateKeyboardShortcut.displayName),
            image: "Images/StepOver.svg",
        });
        this._debuggerStepOverButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerStepOver, this);
        this._debuggerStepOverButtonItem.enabled = false;

        this._debuggerStepIntoButtonItem = createButtonNavigationitem({
            identifier: "debugger-step-into",
            toolTipOrLabel: WI.UIString("Step into (%s or %s)").format(WI.stepIntoKeyboardShortcut.displayName, WI.stepIntoAlternateKeyboardShortcut.displayName),
            image: "Images/StepInto.svg",
        });
        this._debuggerStepIntoButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerStepInto, this);
        this._debuggerStepIntoButtonItem.enabled = false;

        this._debuggerStepOutButtonItem = createButtonNavigationitem({
            identifier: "debugger-step-out",
            toolTipOrLabel: WI.UIString("Step out (%s or %s)").format(WI.stepOutKeyboardShortcut.displayName, WI.stepOutAlternateKeyboardShortcut.displayName),
            image: "Images/StepOut.svg",
        });
        this._debuggerStepOutButtonItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.debuggerStepOut, this);
        this._debuggerStepOutButtonItem.enabled = false;

        this._timelineRecordingWarningElement = null;
        this._auditTestWarningElement = null;
        this._breakpointsDisabledWarningElement = null;

        this._pauseReasonTreeOutline = null;
        this._pauseReasonLinkContainerElement = document.createElement("span");
        this._pauseReasonTextRow = new WI.DetailsSectionTextRow;
        this._pauseReasonGroup = new WI.DetailsSectionGroup([this._pauseReasonTextRow]);
        this._pauseReasonSection = new WI.DetailsSection("paused-reason", WI.UIString("Pause Reason"), [this._pauseReasonGroup], this._pauseReasonLinkContainerElement);

        this._callStackTreeOutline = this.createContentTreeOutline({suppressFiltering: true});
        this._callStackTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._handleTreeSelectionDidChange, this);

        let callStackRow = new WI.DetailsSectionRow;
        callStackRow.element.appendChild(this._callStackTreeOutline.element);

        let callStackGroup = new WI.DetailsSectionGroup([callStackRow]);
        this._callStackSection = new WI.DetailsSection("call-stack", WI.UIString("Call Stack"), [callStackGroup]);

        this._mainTargetTreeElement = null;
        this._activeCallFrameTreeElement = null;

        // Prevent the breakpoints list from being used as the source of selection restoration (e.g. on reload or navigation).
        this._breakpointsTreeOutline = this.createContentTreeOutline({ignoreCookieRestoration: true});
        this._breakpointsTreeOutline.addEventListener(WI.TreeOutline.Event.ElementAdded, this._handleBreakpointElementAddedOrRemoved, this);
        this._breakpointsTreeOutline.addEventListener(WI.TreeOutline.Event.ElementRemoved, this._handleBreakpointElementAddedOrRemoved, this);
        this._breakpointsTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._handleTreeSelectionDidChange, this);
        this._breakpointsTreeOutline.ondelete = (treeElement) => {
            console.assert(treeElement.selected);
            console.assert(treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement);
            if (!(treeElement instanceof WI.ResourceTreeElement) && !(treeElement instanceof WI.ScriptTreeElement))
                return false;

            let wasTopResourceTreeElement = treeElement.previousSibling === this._assertionsBreakpointTreeElement || treeElement.previousSibling === this._allUncaughtExceptionsBreakpointTreeElement;
            let nextSibling = treeElement.nextSibling;

            let breakpoints = this._breakpointsBeneathTreeElement(treeElement);
            this._removeAllBreakpoints(breakpoints);

            if (wasTopResourceTreeElement && nextSibling) {
                const omitFocus = true;
                const selectedByUser = true;
                nextSibling.select(omitFocus, selectedByUser);
            }

            return true;
        };
        this._breakpointsTreeOutline.populateContextMenu = (contextMenu, event, treeElement) => {
            // This check is necessary since the context menu is created by the TreeOutline, meaning
            // that any child could be the target of the context menu event.
            if (treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement) {
                let breakpoints = this._breakpointsBeneathTreeElement(treeElement);

                let shouldDisable = breakpoints.some((breakpoint) => !breakpoint.disabled);
                contextMenu.appendItem(shouldDisable ? WI.UIString("Disable Breakpoints") : WI.UIString("Enable Breakpoints"), () => {
                    this._toggleAllBreakpoints(breakpoints, shouldDisable);
                });

                contextMenu.appendItem(WI.UIString("Delete Breakpoints"), () => {
                    this._removeAllBreakpoints(breakpoints);
                });
            }

            WI.TreeOutline.prototype.populateContextMenu(contextMenu, event, treeElement);
        };

        let breakpointsRow = new WI.DetailsSectionRow;
        breakpointsRow.element.appendChild(this._breakpointsTreeOutline.element);

        let breakpointNavigationBarWrapper = document.createElement("div");

        let breakpointNavigationBar = new WI.NavigationBar;
        breakpointNavigationBarWrapper.appendChild(breakpointNavigationBar.element);

        let createBreakpointButton = new WI.ButtonNavigationItem("create-breakpoint", WI.UIString("Create Breakpoint"), "Images/Plus13.svg", 13, 13);
        createBreakpointButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleCreateBreakpointClicked, this);
        breakpointNavigationBar.addNavigationItem(createBreakpointButton);

        let breakpointsGroup = new WI.DetailsSectionGroup([breakpointsRow]);
        let breakpointsSection = new WI.DetailsSection("breakpoints", WI.UIString("Breakpoints"), [breakpointsGroup], breakpointNavigationBarWrapper);
        this.contentView.element.insertBefore(breakpointsSection.element, this.contentView.element.firstChild);

        this._resourcesTreeOutline = this.contentTreeOutline;
        this._resourcesTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._handleTreeSelectionDidChange, this);
        this._resourcesTreeOutline.includeSourceMapResourceChildren = true;

        this._resourcesNavigationBar = new WI.NavigationBar;
        this.contentView.addSubview(this._resourcesNavigationBar);
        this.contentView.element.insertBefore(this._resourcesNavigationBar.element, this._resourcesTreeOutline.element);

        this._workerTargetTreeElementMap = new Map;
        this._extensionScriptsFolderTreeElement = null;
        this._extraScriptsFolderTreeElement = null;
        this._anonymousScriptsFolderTreeElement = null;

        const resourceTypeScopeItemPrefix = "sources-resource-type-";
        let resourceTypeScopeBarItems = [];
        resourceTypeScopeBarItems.push(new WI.ScopeBarItem(resourceTypeScopeItemPrefix + "all", WI.UIString("All Resources"), {exclusive: true}));
        for (let value of Object.values(WI.Resource.Type)) {
            let scopeBarItem = new WI.ScopeBarItem(resourceTypeScopeItemPrefix + value, WI.Resource.displayNameForType(value, true));
            scopeBarItem[WI.SourcesNavigationSidebarPanel.ResourceTypeSymbol] = value;
            resourceTypeScopeBarItems.push(scopeBarItem);
        }

        const shouldGroupNonExclusiveItems = true;
        this._resourceTypeScopeBar = new WI.ScopeBar("sources-resource-type-scope-bar", resourceTypeScopeBarItems, resourceTypeScopeBarItems[0], shouldGroupNonExclusiveItems);
        this._resourceTypeScopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._handleResourceTypeScopeBarSelectionChanged, this);
        this._resourcesNavigationBar.addNavigationItem(this._resourceTypeScopeBar);

        let onlyShowResourcesWithIssuesFilterFunction = (treeElement) => {
            if (treeElement.treeOutline !== this._resourcesTreeOutline)
                return true;

            if (treeElement instanceof WI.IssueTreeElement)
                return true;

            if (treeElement.hasChildren) {
                for (let child of treeElement.children) {
                    if (child instanceof WI.IssueTreeElement)
                        return true;
                }
            }
            return false;
        };
        const activatedByDefault = false;
        this.filterBar.addFilterBarButton("sources-only-show-resources-with-issues", onlyShowResourcesWithIssuesFilterFunction, activatedByDefault, WI.UIString("Only show resources with issues"), WI.UIString("Show all resources"), "Images/Errors.svg", 15, 15);

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._handleFrameMainResourceDidChange, this);
        WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, this._handleResourceAdded, this);
        WI.Target.addEventListener(WI.Target.Event.ResourceAdded, this._handleResourceAdded, this);

        WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, this._handleMainFrameDidChange, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointAdded, this._handleDebuggerBreakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointAdded, this._handleDebuggerBreakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.EventBreakpointAdded, this._handleDebuggerBreakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.URLBreakpointAdded, this._handleDebuggerBreakpointAdded, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointRemoved, this._handleDebuggerBreakpointRemoved, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.DOMBreakpointRemoved, this._handleDebuggerBreakpointRemoved, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, this._handleDebuggerBreakpointRemoved, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.URLBreakpointRemoved, this._handleDebuggerBreakpointRemoved, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.BreakpointsEnabledDidChange, this._handleDebuggerBreakpointsEnabledDidChange, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ScriptAdded, this._handleDebuggerScriptAdded, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ScriptRemoved, this._handleDebuggerScriptRemoved, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ScriptsCleared, this._handleDebuggerScriptsCleared, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, this._handleDebuggerPaused, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Resumed, this._handleDebuggerResumed, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.CallFramesDidChange, this._handleDebuggerCallFramesDidChange, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._handleDebuggerActiveCallFrameDidChange, this);
        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.WaitingToPause, this._handleDebuggerWaitingToPause, this);

        WI.Breakpoint.addEventListener(WI.Breakpoint.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);
        WI.IssueMessage.addEventListener(WI.IssueMessage.Event.DisplayLocationDidChange, this._handleDebuggerObjectDisplayLocationDidChange, this);

        WI.DOMBreakpoint.addEventListener(WI.DOMBreakpoint.Event.ResolvedStateDidChange, this._handleDOMBreakpointResolvedStateDidChange, this);

        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.IssueAdded, this._handleConsoleIssueAdded, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.Cleared, this._handleConsoleCleared, this);

        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingWillStart, this._handleTimelineCapturingWillStart, this);
        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStopped, this._handleTimelineCapturingStopped, this);

        WI.auditManager.addEventListener(WI.AuditManager.Event.TestScheduled, this._handleAuditManagerTestScheduled, this);
        WI.auditManager.addEventListener(WI.AuditManager.Event.TestCompleted, this._handleAuditManagerTestCompleted, this);

        WI.cssManager.addEventListener(WI.CSSManager.Event.StyleSheetAdded, this._handleCSSStyleSheetAdded, this);

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetAdded, this._handleTargetAdded, this);
        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._handleTargetRemoved, this);

        WI.notifications.addEventListener(WI.Notification.ExtraDomainsActivated, this._handleExtraDomainsActivated, this);

        if (WI.SourcesNavigationSidebarPanel.shouldPlaceResourcesAtTopLevel()) {
            this._resourcesTreeOutline.disclosureButtons = false;
            WI.SourceCode.addEventListener(WI.SourceCode.Event.SourceMapAdded, () => {
                this._resourcesTreeOutline.disclosureButtons = true;
            }, this);
        }

        WI.debuggerManager.addBreakpoint(WI.debuggerManager.allExceptionsBreakpoint);
        WI.debuggerManager.addBreakpoint(WI.debuggerManager.uncaughtExceptionsBreakpoint);

        // COMPATIBILITY (iOS 10): DebuggerAgent.setPauseOnAssertions did not exist yet.
        if (InspectorBackend.domains.Debugger.setPauseOnAssertions && WI.settings.showAssertionFailuresBreakpoint.value)
            WI.debuggerManager.addBreakpoint(WI.debuggerManager.assertionFailuresBreakpoint);

        let mainFrame = WI.networkManager.mainFrame;
        if (mainFrame) {
            this._updateMainFrameTreeElement(mainFrame);
            this._addResourcesRecursivelyForFrame(mainFrame);
        }

        for (let target of WI.targets)
            this._addTarget(target);

        this._updateCallStackTreeOutline();

        for (let script of WI.debuggerManager.knownNonResourceScripts) {
            this._addScript(script);

            if (script.sourceMaps.length && WI.SourcesNavigationSidebarPanel.shouldPlaceResourcesAtTopLevel())
                this._resourcesTreeOutline.disclosureButtons = true;
        }

        if (WI.domDebuggerManager.supported) {
            if (WI.settings.showAllRequestsBreakpoint.value)
                WI.domDebuggerManager.addURLBreakpoint(WI.domDebuggerManager.allRequestsBreakpoint);

            for (let eventBreakpoint of WI.domDebuggerManager.eventBreakpoints)
                this._addBreakpoint(eventBreakpoint);

            for (let domBreakpoint of WI.domDebuggerManager.domBreakpoints)
                this._addBreakpoint(domBreakpoint);

            for (let eventListenerBreakpoint of WI.domManager.eventListenerBreakpoints)
                this._addBreakpoint(eventListenerBreakpoint);

            for (let urlBreakpoints of WI.domDebuggerManager.urlBreakpoints)
                this._addBreakpoint(urlBreakpoints);
        }

        if (WI.debuggerManager.paused)
            this._handleDebuggerPaused();

        if (WI.debuggerManager.breakpointsDisabledTemporarily) {
            if (WI.timelineManager.isCapturing())
                this._handleTimelineCapturingWillStart();

            if (WI.auditManager.runningState === WI.AuditManager.RunningState.Active || WI.auditManager.runningState === WI.AuditManager.RunningState.Stopping)
                this._handleAuditManagerTestScheduled();
        }

        this._updateBreakpointsDisabledBanner();
    }

    // Static

    static shouldPlaceResourcesAtTopLevel()
    {
        return (WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript && !WI.sharedApp.hasExtraDomains)
            || WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker;
    }

    // Public

    get minimumWidth()
    {
        return Math.max(this._debuggerNavigationBar.minimumWidth, this._resourcesNavigationBar.minimumWidth);
    }

    closed()
    {
        WI.Frame.removeEventListener(null, null, this);
        WI.Target.removeEventListener(null, null, this);
        WI.networkManager.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);
        WI.domDebuggerManager.removeEventListener(null, null, this);
        WI.Breakpoint.removeEventListener(null, null, this);
        WI.IssueMessage.removeEventListener(null, null, this);
        WI.DOMBreakpoint.removeEventListener(null, null, this);
        WI.consoleManager.removeEventListener(null, null, this);
        WI.timelineManager.removeEventListener(null, null, this);
        WI.auditManager.removeEventListener(null, null, this);
        WI.cssManager.removeEventListener(null, null, this);
        WI.targetManager.removeEventListener(null, null, this);
        WI.notifications.removeEventListener(null, null, this);
        WI.SourceCode.removeEventListener(null, null, this);

        super.closed();
    }

    showDefaultContentView()
    {
        if (WI.networkManager.mainFrame) {
            this.contentBrowser.showContentViewForRepresentedObject(WI.networkManager.mainFrame);
            return;
        }

        let firstTreeElement = this._resourcesTreeOutline.children[0];
        if (firstTreeElement)
            this.showDefaultContentViewForTreeElement(firstTreeElement);
    }

    treeElementForRepresentedObject(representedObject)
    {
        // A custom implementation is needed for this since the frames are populated lazily.

        if (!this._mainFrameTreeElement && (representedObject instanceof WI.Resource || representedObject instanceof WI.Frame || representedObject instanceof WI.Collection)) {
            // All resources are under the main frame, so we need to return early if we don't have the main frame yet.
            return null;
        }

        // The Frame is used as the representedObject instead of the main resource in our tree.
        if (representedObject instanceof WI.Resource && representedObject.parentFrame && representedObject.parentFrame.mainResource === representedObject)
            representedObject = representedObject.parentFrame;

        function isAncestor(ancestor, resourceOrFrame) {
            // SourceMapResources are descendants of another SourceCode object.
            if (resourceOrFrame instanceof WI.SourceMapResource) {
                if (resourceOrFrame.sourceMap.originalSourceCode === ancestor)
                    return true;

                // Not a direct ancestor, so check the ancestors of the parent SourceCode object.
                resourceOrFrame = resourceOrFrame.sourceMap.originalSourceCode;
            }

            let currentFrame = resourceOrFrame.parentFrame;
            while (currentFrame) {
                if (currentFrame === ancestor)
                    return true;
                currentFrame = currentFrame.parentFrame;
            }
            return false;
        }

        function getParent(resourceOrFrame) {
            // SourceMapResources are descendants of another SourceCode object.
            if (resourceOrFrame instanceof WI.SourceMapResource)
                return resourceOrFrame.sourceMap.originalSourceCode;
            return resourceOrFrame.parentFrame;
        }

        let treeElement = this._resourcesTreeOutline.findTreeElement(representedObject, isAncestor, getParent);
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

        if (!this._anonymousScriptsFolderTreeElement)
            this._anonymousScriptsFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Anonymous Scripts"), new WI.ScriptCollection);

        if (!this._anonymousScriptsFolderTreeElement.parent) {
            let index = insertionIndexForObjectInListSortedByFunction(this._anonymousScriptsFolderTreeElement, this._resourcesTreeOutline.children, this._compareTreeElements);
            this._resourcesTreeOutline.insertChild(this._anonymousScriptsFolderTreeElement, index);
        }

        this._anonymousScriptsFolderTreeElement.representedObject.add(representedObject);

        let scriptTreeElement = new WI.ScriptTreeElement(representedObject);
        this._anonymousScriptsFolderTreeElement.appendChild(scriptTreeElement);
        return scriptTreeElement;
    }

    // Protected

    resetFilter()
    {
        this._resourceTypeScopeBar.resetToDefault();

        super.resetFilter();
    }

    hasCustomFilters()
    {
        console.assert(this._resourceTypeScopeBar.selectedItems.length === 1);
        let selectedScopeBarItem = this._resourceTypeScopeBar.selectedItems[0];
        return selectedScopeBarItem && !selectedScopeBarItem.exclusive;
    }

    matchTreeElementAgainstCustomFilters(treeElement, flags)
    {
        // Only apply the resource type filter to the resources list.
        if (treeElement.treeOutline !== this._resourcesTreeOutline)
            return true;

        console.assert(this._resourceTypeScopeBar.selectedItems.length === 1);
        let selectedScopeBarItem = this._resourceTypeScopeBar.selectedItems[0];

        // Show everything if there is no selection or "All Resources" is selected (the exclusive item).
        if (!selectedScopeBarItem || selectedScopeBarItem.exclusive)
            return true;

        // Folders are hidden on the first pass, but visible childen under the folder will force the folder visible again.
        if (treeElement instanceof WI.FolderTreeElement)
            return false;

        function match()
        {
            if (treeElement instanceof WI.FrameTreeElement)
                return selectedScopeBarItem[WI.SourcesNavigationSidebarPanel.ResourceTypeSymbol] === WI.Resource.Type.Document;

            if (treeElement instanceof WI.ScriptTreeElement)
                return selectedScopeBarItem[WI.SourcesNavigationSidebarPanel.ResourceTypeSymbol] === WI.Resource.Type.Script;

            if (treeElement instanceof WI.CSSStyleSheetTreeElement)
                return selectedScopeBarItem[WI.SourcesNavigationSidebarPanel.ResourceTypeSymbol] === WI.Resource.Type.Stylesheet;

            console.assert(treeElement instanceof WI.ResourceTreeElement, "Unknown treeElement", treeElement);
            if (!(treeElement instanceof WI.ResourceTreeElement))
                return false;

            return treeElement.resource.type === selectedScopeBarItem[WI.SourcesNavigationSidebarPanel.ResourceTypeSymbol];
        }

        let matched = match();
        if (matched)
            flags.expandTreeElement = true;
        return matched;
    }

    // Popover delegate

    willDismissPopover(popover)
    {
        let breakpoint = popover.breakpoint;
        if (!breakpoint)
            return;

        if (breakpoint instanceof WI.EventBreakpoint)
            WI.domDebuggerManager.addEventBreakpoint(breakpoint);
        else if (breakpoint instanceof WI.URLBreakpoint)
            WI.domDebuggerManager.addURLBreakpoint(breakpoint);
    }

    // Private

    _compareTreeElements(a, b)
    {
        // Always sort the main frame element first.
        if (a instanceof WI.FrameTreeElement)
            return -1;
        if (b instanceof WI.FrameTreeElement)
            return 1;

        console.assert(a.mainTitle);
        console.assert(b.mainTitle);

        return (a.mainTitle || "").extendedLocaleCompare(b.mainTitle || "");
    }

    _updateMainFrameTreeElement(mainFrame)
    {
        if (this.didInitialLayout)
            this.contentBrowser.contentViewContainer.closeAllContentViews();

        if (this._mainFrameTreeElement) {
            this._resourcesTreeOutline.removeChild(this._mainFrameTreeElement);
            this._mainFrameTreeElement = null;
        }

        if (!mainFrame)
            return;

        this._mainFrameTreeElement = new WI.FrameTreeElement(mainFrame);
        this._resourcesTreeOutline.insertChild(this._mainFrameTreeElement, 0);

        // Cookie restoration will attempt to re-select the resource we were showing.
        // Give it time to do that before selecting the main frame resource.
        setTimeout(() => {
            if (this._resourcesTreeOutline.selectedTreeElement)
                return;

            let currentContentView = this.contentBrowser.currentContentView;
            let treeElement = currentContentView ? this.treeElementForRepresentedObject(currentContentView.representedObject) : null;
            if (!treeElement)
                treeElement = this._mainFrameTreeElement;
            this.showDefaultContentViewForTreeElement(treeElement);
        });
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

    _findCallStackTargetTreeElement(target)
    {
        for (let child of this._callStackTreeOutline.children) {
            if (child.target === target)
                return child;
        }
        return null;
    }

    _updateCallStackTreeOutline()
    {
        let singleThreadShowing = WI.targets.length <= 1;
        this._callStackTreeOutline.element.classList.toggle("single-thread", singleThreadShowing);
        if (this._mainTargetTreeElement)
            this._mainTargetTreeElement.selectable = !singleThreadShowing;
    }

    _addResource(resource)
    {
        if (resource.type !== WI.Resource.Type.Document && resource.type !== WI.Resource.Type.Script)
            return;

        this._addBreakpointsForSourceCode(resource);
        this._addIssuesForSourceCode(resource);
    }

    _addResourcesRecursivelyForFrame(frame)
    {
        this._addResource(frame.mainResource);

        for (let resource of frame.resourceCollection)
            this._addResource(resource);

        for (let childFrame of frame.childFrameCollection)
            this._addResourcesRecursivelyForFrame(childFrame);
    }

    _addScript(script)
    {
        // We don't add scripts without URLs here. Those scripts can quickly clutter the interface and
        // are usually more transient. They will get added if/when they need to be shown in a content view.
        if (!script.url && !script.sourceURL)
            return;

        // Target main resource.
        if (WI.sharedApp.debuggableType !== WI.DebuggableType.JavaScript) {
            if (script.target !== WI.pageTarget) {
                if (script.isMainResource()) {
                    this._addWorkerTargetWithMainResource(script.target);
                    this._addBreakpointsForSourceCode(script);
                    this._addIssuesForSourceCode(script);
                }
                this._resourcesTreeOutline.disclosureButtons = true;
                return;
            }
        }

        // If the script URL matches a resource we can assume it is part of that resource and does not need added.
        if (script.resource || script.dynamicallyAddedScriptElement)
            return;

        let insertIntoTopLevel = false;
        let parentFolderTreeElement = null;

        if (script.injected) {
            if (!this._extensionScriptsFolderTreeElement) {
                let collection = new WI.ScriptCollection;
                this._extensionScriptsFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Extension Scripts"), collection);
            }

            parentFolderTreeElement = this._extensionScriptsFolderTreeElement;
        } else {
            if (WI.SourcesNavigationSidebarPanel.shouldPlaceResourcesAtTopLevel())
                insertIntoTopLevel = true;
            else {
                if (!this._extraScriptsFolderTreeElement) {
                    let collection = new WI.ScriptCollection;
                    this._extraScriptsFolderTreeElement = new WI.FolderTreeElement(WI.UIString("Extra Scripts"), collection);
                }

                parentFolderTreeElement = this._extraScriptsFolderTreeElement;
            }
        }

        if (parentFolderTreeElement)
            parentFolderTreeElement.representedObject.add(script);

        let scriptTreeElement = new WI.ScriptTreeElement(script);

        if (insertIntoTopLevel) {
            let index = insertionIndexForObjectInListSortedByFunction(scriptTreeElement, this._resourcesTreeOutline.children, this._compareTreeElements);
            this._resourcesTreeOutline.insertChild(scriptTreeElement, index);
        } else {
            if (!parentFolderTreeElement.parent) {
                let index = insertionIndexForObjectInListSortedByFunction(parentFolderTreeElement, this._resourcesTreeOutline.children, this._compareTreeElements);
                this._resourcesTreeOutline.insertChild(parentFolderTreeElement, index);
            }

            parentFolderTreeElement.appendChild(scriptTreeElement);
        }

        this._addBreakpointsForSourceCode(script);
        this._addIssuesForSourceCode(script);
    }

    _addWorkerTargetWithMainResource(target)
    {
        console.assert(target.type === WI.Target.Type.Worker || target.type === WI.Target.Type.ServiceWorker);

        let targetTreeElement = new WI.WorkerTreeElement(target);
        this._workerTargetTreeElementMap.set(target, targetTreeElement);

        let index = insertionIndexForObjectInListSortedByFunction(targetTreeElement, this._resourcesTreeOutline.children, this._compareTreeElements);
        this._resourcesTreeOutline.insertChild(targetTreeElement, index);
    }

    _addDebuggerTreeElementForSourceCode(sourceCode)
    {
        let treeElement = this._breakpointsTreeOutline.findTreeElement(sourceCode);
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

            this._insertDebuggerTreeElement(treeElement, this._breakpointsTreeOutline);
        }

        return treeElement;
    }

    _insertDebuggerTreeElement(treeElement, parentTreeElement)
    {
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
                (treeElement) => treeElement instanceof WI.URLBreakpointTreeElement,
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

        return aLocation.displayLineNumber - bLocation.displayLineNumber || aLocation.displayColumnNumber - bLocation.displayColumnNumber;
    }

    _addBreakpoint(breakpoint)
    {
        let constructor = WI.BreakpointTreeElement;
        let options = {};
        let parentTreeElement = this._breakpointsTreeOutline;

        let getDOMNodeTreeElement = (domNode) => {
            console.assert(domNode, "Missing DOMNode for identifier", breakpoint.domNodeIdentifier);
            if (!domNode)
                return null;

            let domNodeTreeElement = this._breakpointsTreeOutline.findTreeElement(domNode);
            if (!domNodeTreeElement) {
                domNodeTreeElement = new WI.DOMNodeTreeElement(domNode);
                this._insertDebuggerTreeElement(domNodeTreeElement, parentTreeElement);
            }
            return domNodeTreeElement;
        };

        if (breakpoint === WI.debuggerManager.allExceptionsBreakpoint) {
            options.className = "breakpoint-exception-icon";
            options.title = WI.UIString("All Exceptions");
        } else if (breakpoint === WI.debuggerManager.uncaughtExceptionsBreakpoint) {
            options.className = "breakpoint-exception-icon";
            options.title = WI.UIString("Uncaught Exceptions");
        } else if (breakpoint === WI.debuggerManager.assertionFailuresBreakpoint) {
            options.className = "breakpoint-assertion-icon";
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
        } else if (breakpoint instanceof WI.URLBreakpoint) {
            constructor = WI.URLBreakpointTreeElement;

            if (breakpoint === WI.domDebuggerManager.allRequestsBreakpoint) {
                options.className = "breakpoint-assertion-icon";
                options.title = WI.UIString("All Requests");
            }
        } else {
            let sourceCode = breakpoint.sourceCodeLocation && breakpoint.sourceCodeLocation.displaySourceCode;
            if (!sourceCode)
                return null;

            if (this._breakpointsTreeOutline.findTreeElement(breakpoint))
                return null;

            parentTreeElement = this._addDebuggerTreeElementForSourceCode(sourceCode);

            // Mark disabled breakpoints as resolved if there is source code loaded with that URL.
            // This gives the illusion the breakpoint was resolved, but since we don't send disabled
            // breakpoints to the backend we don't know for sure. If the user enables the breakpoint
            // it will be resolved properly.
            if (breakpoint.disabled)
                breakpoint.resolved = true;
        }

        let breakpointTreeElement = new constructor(breakpoint, options);
        this._insertDebuggerTreeElement(breakpointTreeElement, parentTreeElement);
        if (parentTreeElement.children.length === 1)
            parentTreeElement.expand();
        return breakpointTreeElement;
    }

    _removeBreakpoint(breakpoint)
    {
        if (this._pauseReasonTreeOutline) {
            let pauseReasonBreakpointTreeElement = this._pauseReasonTreeOutline.findTreeElement(breakpoint);
            if (pauseReasonBreakpointTreeElement)
                pauseReasonBreakpointTreeElement.status = null;
        }

        let breakpointTreeElement = this._breakpointsTreeOutline.findTreeElement(breakpoint);
        if (!breakpointTreeElement)
            return;

        this._removeDebuggerTreeElement(breakpointTreeElement);
    }

    _removeAllBreakpoints(breakpoints)
    {
        for (let breakpoint of breakpoints) {
            if (WI.debuggerManager.isBreakpointRemovable(breakpoint))
                WI.debuggerManager.removeBreakpoint(breakpoint);
        }
    }

    _toggleAllBreakpoints(breakpoints, disabled)
    {
        for (let breakpoint of breakpoints)
            breakpoint.disabled = disabled;
    }

    _breakpointsBeneathTreeElement(treeElement)
    {
        console.assert(treeElement instanceof WI.ResourceTreeElement || treeElement instanceof WI.ScriptTreeElement);
        if (!(treeElement instanceof WI.ResourceTreeElement) && !(treeElement instanceof WI.ScriptTreeElement))
            return [];

        let breakpoints = [];
        for (let child of treeElement.children) {
            console.assert(child instanceof WI.BreakpointTreeElement);
            let breakpoint = child.breakpoint;
            console.assert(breakpoint);
            if (breakpoint)
                breakpoints.push(breakpoint);
        }
        return breakpoints;
    }

    _addIssue(issueMessage)
    {
        let issueTreeElement = this._resourcesTreeOutline.findTreeElement(issueMessage);
        if (!issueTreeElement) {
            let parentTreeElement = this._resourcesTreeOutline.findTreeElement(issueMessage.sourceCodeLocation.sourceCode);
            if (!parentTreeElement)
                return null;

            issueTreeElement = new WI.IssueTreeElement(issueMessage);

            parentTreeElement.insertChild(issueTreeElement, insertionIndexForObjectInListSortedByFunction(issueTreeElement, parentTreeElement.children, this._compareBreakpointTreeElements));
            if (parentTreeElement.children.length === 1)
                parentTreeElement.expand();
        }
        return issueTreeElement;
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

        if (parentTreeElement.children.length || parentTreeElement === this._breakpointsTreeOutline)
            return;

        parentTreeElement.treeOutline.removeChild(parentTreeElement);
    }

    _addBreakpointsForSourceCode(sourceCode)
    {
        for (let breakpoint of WI.debuggerManager.breakpointsForSourceCode(sourceCode))
            this._addBreakpoint(breakpoint, sourceCode);
    }

    _addIssuesForSourceCode(sourceCode)
    {
        for (let issue of WI.consoleManager.issuesForSourceCode(sourceCode))
            this._addIssue(issue);
    }

    _updateTemporarilyDisabledBreakpointsButtons()
    {
        let breakpointsDisabledTemporarily = WI.debuggerManager.breakpointsDisabledTemporarily;
        this._debuggerBreakpointsButtonItem.enabled = !breakpointsDisabledTemporarily;
        this._debuggerPauseResumeButtonItem.enabled = !breakpointsDisabledTemporarily;
    }

    _updateBreakpointsDisabledBanner()
    {
        if (!WI.debuggerManager.breakpointsEnabled && !this._timelineRecordingWarningElement && !this._auditTestWarningElement) {
            if (!this._breakpointsDisabledWarningElement) {
                let enableBreakpointsButton = document.createElement("button");
                enableBreakpointsButton.textContent = WI.UIString("Enable Breakpoints");
                enableBreakpointsButton.addEventListener("click", () => {
                    WI.debuggerToggleBreakpoints();
                });

                this._breakpointsDisabledWarningElement = document.createElement("div");
                this._breakpointsDisabledWarningElement.classList.add("warning-banner");
                this._breakpointsDisabledWarningElement.append(WI.UIString("Breakpoints disabled"), document.createElement("br"), enableBreakpointsButton);
            }

            this.contentView.element.insertBefore(this._breakpointsDisabledWarningElement, this.contentView.element.firstChild);
        } else if (this._breakpointsDisabledWarningElement) {
            this._breakpointsDisabledWarningElement.remove();
            this._breakpointsDisabledWarningElement = null;
        }
    }

    _updatePauseReason()
    {
        this._pauseReasonTreeOutline = null;

        this._updatePauseReasonGotoArrow();
        return this._updatePauseReasonSection();
    }

    _updatePauseReasonGotoArrow()
    {
        this._pauseReasonLinkContainerElement.removeChildren();

        let activeCallFrame = WI.debuggerManager.activeCallFrame;
        if (!activeCallFrame)
            return;

        let sourceCodeLocation = activeCallFrame.sourceCodeLocation;
        if (!sourceCodeLocation)
            return;

        const options = {
            useGoToArrowButton: true,
        };
        let linkElement = WI.createSourceCodeLocationLink(sourceCodeLocation, options);
        this._pauseReasonLinkContainerElement.appendChild(linkElement);
    }

    _updatePauseReasonSection()
    {
        let target = WI.debuggerManager.activeCallFrame.target;
        let targetData = WI.debuggerManager.dataForTarget(target);
        let {pauseReason, pauseData} = targetData;

        switch (pauseReason) {
        case WI.DebuggerManager.PauseReason.AnimationFrame: {
            console.assert(pauseData, "Expected data with an animation frame, but found none.");
            if (!pauseData)
                break;

            let eventBreakpoint = WI.domDebuggerManager.eventBreakpointForTypeAndEventName(WI.EventBreakpoint.Type.AnimationFrame, pauseData.eventName);
            console.assert(eventBreakpoint, "Expected AnimationFrame breakpoint for event name.", pauseData.eventName);
            if (!eventBreakpoint)
                break;

            this._pauseReasonTreeOutline = this.createContentTreeOutline({suppressFiltering: true});

            let eventBreakpointTreeElement = new WI.EventBreakpointTreeElement(eventBreakpoint, {
                className: "breakpoint-paused-icon",
                title: WI.UIString("%s Fired").format(pauseData.eventName),
            });
            this._pauseReasonTreeOutline.appendChild(eventBreakpointTreeElement);

            let eventBreakpointRow = new WI.DetailsSectionRow;
            eventBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

            this._pauseReasonGroup.rows = [eventBreakpointRow];
            return true;
        }

        case WI.DebuggerManager.PauseReason.Assertion:
            // FIXME: We should include the assertion condition string.
            console.assert(pauseData, "Expected data with an assertion, but found none.");
            if (pauseData && pauseData.message)
                this._pauseReasonTextRow.text = WI.UIString("Assertion with message: %s").format(pauseData.message);
            else
                this._pauseReasonTextRow.text = WI.UIString("Assertion Failed");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.Breakpoint: {
            console.assert(pauseData, "Expected breakpoint identifier, but found none.");
            if (!pauseData || !pauseData.breakpointId)
                break;

            this._pauseReasonTreeOutline = this.createContentTreeOutline({suppressFiltering: true});
            this._pauseReasonTreeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._handleTreeSelectionDidChange, this);

            let breakpoint = WI.debuggerManager.breakpointForIdentifier(pauseData.breakpointId);
            let breakpointTreeElement = new WI.BreakpointTreeElement(breakpoint, {
                className: "breakpoint-paused-icon",
                title: WI.UIString("Triggered Breakpoint"),
            });
            let breakpointDetailsSection = new WI.DetailsSectionRow;
            this._pauseReasonTreeOutline.appendChild(breakpointTreeElement);
            breakpointDetailsSection.element.appendChild(this._pauseReasonTreeOutline.element);

            this._pauseReasonGroup.rows = [breakpointDetailsSection];
            return true;
        }

        case WI.DebuggerManager.PauseReason.CSPViolation:
            console.assert(pauseData, "Expected data with a CSP Violation, but found none.");
            if (!pauseData)
                break;

            // COMPATIBILITY (iOS 8): 'directive' was 'directiveText'.
            this._pauseReasonTextRow.text = WI.UIString("Content Security Policy violation of directive: %s").format(pauseData.directive || pauseData.directiveText);
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.DebuggerStatement:
            this._pauseReasonTextRow.text = WI.UIString("Debugger Statement");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.DOM: {
            console.assert(WI.domDebuggerManager.supported);
            console.assert(pauseData, "Expected DOM breakpoint data, but found none.");
            if (!pauseData || !pauseData.nodeId)
                break;

            let domNode = WI.domManager.nodeForId(pauseData.nodeId);
            let domBreakpoints = WI.domDebuggerManager.domBreakpointsForNode(domNode);
            let domBreakpoint;
            for (let breakpoint of domBreakpoints) {
                if (breakpoint.type === pauseData.type) {
                    domBreakpoint = breakpoint;
                    break;
                }
            }

            console.assert(domBreakpoint, "Missing DOM breakpoint of type for node", pauseData.type, domNode);
            if (!domBreakpoint)
                break;

            this._pauseReasonTreeOutline = this.createContentTreeOutline({suppressFiltering: true});

            let type = WI.DOMBreakpointTreeElement.displayNameForType(domBreakpoint.type);
            let domBreakpointTreeElement = new WI.DOMBreakpointTreeElement(domBreakpoint, {
                className: "breakpoint-paused-icon",
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

        case WI.DebuggerManager.PauseReason.EventListener: {
            console.assert(pauseData, "Expected data with an event listener, but found none.");
            if (!pauseData)
                break;

            let eventBreakpoint = null;
            if (pauseData.eventListenerId)
                eventBreakpoint = WI.domManager.breakpointForEventListenerId(pauseData.eventListenerId);
            if (!eventBreakpoint)
                eventBreakpoint = WI.domDebuggerManager.eventBreakpointForTypeAndEventName(WI.EventBreakpoint.Type.Listener, pauseData.eventName);

            console.assert(eventBreakpoint, "Expected Event Listener breakpoint for event name.", pauseData.eventName);
            if (!eventBreakpoint)
                break;

            this._pauseReasonTreeOutline = this.createContentTreeOutline({suppressFiltering: true});

            let eventBreakpointTreeElement = new WI.EventBreakpointTreeElement(eventBreakpoint, {
                className: "breakpoint-paused-icon",
                title: WI.UIString("\u201C%s\u201D Event Fired").format(pauseData.eventName),
            });
            this._pauseReasonTreeOutline.appendChild(eventBreakpointTreeElement);

            let eventBreakpointRow = new WI.DetailsSectionRow;
            eventBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

            let rows = [eventBreakpointRow];

            let eventListener = eventBreakpoint.eventListener;
            if (eventListener) {
                console.assert(eventListener.eventListenerId === pauseData.eventListenerId);

                let ownerElementRow = new WI.DetailsSectionSimpleRow(WI.UIString("Element"), WI.linkifyNodeReference(eventListener.node));
                rows.push(ownerElementRow);
            }

            this._pauseReasonGroup.rows = rows;
            return true;
        }

        case WI.DebuggerManager.PauseReason.Exception: {
            console.assert(pauseData, "Expected data with an exception, but found none.");
            if (!pauseData)
                break;

            // FIXME: We should improve the appearance of thrown objects. This works well for exception strings.
            let data = WI.RemoteObject.fromPayload(pauseData, target);
            this._pauseReasonTextRow.text = WI.UIString("Exception with thrown value: %s").format(data.description);
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;
        }

        case WI.DebuggerManager.PauseReason.PauseOnNextStatement:
            this._pauseReasonTextRow.text = WI.UIString("Immediate Pause Requested");
            this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            return true;

        case WI.DebuggerManager.PauseReason.Timer: {
            console.assert(pauseData, "Expected data with a timer, but found none.");
            if (!pauseData)
                break;

            let eventBreakpoint = WI.domDebuggerManager.eventBreakpointForTypeAndEventName(WI.EventBreakpoint.Type.Timer, pauseData.eventName);
            console.assert(eventBreakpoint, "Expected Timer breakpoint for event name.", pauseData.eventName);
            if (!eventBreakpoint)
                break;

            this._pauseReasonTreeOutline = this.createContentTreeOutline({suppressFiltering: true});

            let eventBreakpointTreeElement = new WI.EventBreakpointTreeElement(eventBreakpoint, {
                className: "breakpoint-paused-icon",
                title: WI.UIString("%s Fired").format(pauseData.eventName),
            });
            this._pauseReasonTreeOutline.appendChild(eventBreakpointTreeElement);

            let eventBreakpointRow = new WI.DetailsSectionRow;
            eventBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

            this._pauseReasonGroup.rows = [eventBreakpointRow];
            return true;
        }

        case WI.DebuggerManager.PauseReason.Fetch:
        case WI.DebuggerManager.PauseReason.XHR: {
            console.assert(WI.domDebuggerManager.supported);
            console.assert(pauseData, "Expected URL breakpoint data, but found none.");
            if (!pauseData)
                break;

            if (pauseData.breakpointURL) {
                let urlBreakpoint = WI.domDebuggerManager.urlBreakpointForURL(pauseData.breakpointURL);
                console.assert(urlBreakpoint, "Expected URL breakpoint for URL.", pauseData.breakpointURL);

                this._pauseReasonTreeOutline = this.createContentTreeOutline({suppressFiltering: true});

                let urlBreakpointTreeElement = new WI.URLBreakpointTreeElement(urlBreakpoint, {
                    className: "breakpoint-paused-icon",
                    title: WI.UIString("Triggered URL Breakpoint"),
                });
                let urlBreakpointRow = new WI.DetailsSectionRow;
                this._pauseReasonTreeOutline.appendChild(urlBreakpointTreeElement);
                urlBreakpointRow.element.appendChild(this._pauseReasonTreeOutline.element);

                this._pauseReasonTextRow.text = pauseData.url;
                this._pauseReasonGroup.rows = [urlBreakpointRow, this._pauseReasonTextRow];
            } else {
                console.assert(pauseData.breakpointURL === "", "Should be the All Requests breakpoint which has an empty URL");
                this._pauseReasonTextRow.text = WI.UIString("Requesting: %s").format(pauseData.url);
                this._pauseReasonGroup.rows = [this._pauseReasonTextRow];
            }
            this._pauseReasonTextRow.element.title = pauseData.url;
            return true;
        }

        case WI.DebuggerManager.PauseReason.Other:
            console.error("Paused for unknown reason. We should always have a reason.");
            break;
        }

        return false;
    }

    _handleResourceTypeScopeBarSelectionChanged(event)
    {
        this.updateFilter();
    }

    _handleTreeSelectionDidChange(event)
    {
        if (!this.selected)
            return;

        let treeElement = event.target.selectedTreeElement;
        if (!treeElement)
            return;

        if (treeElement instanceof WI.DOMNodeTreeElement
            || treeElement instanceof WI.DOMBreakpointTreeElement
            || treeElement instanceof WI.EventBreakpointTreeElement
            || treeElement instanceof WI.URLBreakpointTreeElement)
            return;

        if (treeElement instanceof WI.FolderTreeElement
            || treeElement instanceof WI.ResourceTreeElement
            || treeElement instanceof WI.ScriptTreeElement
            || treeElement instanceof WI.CSSStyleSheetTreeElement) {
            WI.showRepresentedObject(treeElement.representedObject);
            return;
        }

        if (treeElement instanceof WI.CallFrameTreeElement) {
            let callFrame = treeElement.callFrame;
            if (callFrame.id)
                WI.debuggerManager.activeCallFrame = callFrame;
            if (callFrame.sourceCodeLocation)
                WI.showSourceCodeLocation(callFrame.sourceCodeLocation);
            return;
        }

        if (treeElement instanceof WI.IssueTreeElement) {
            WI.showSourceCodeLocation(treeElement.issueMessage.sourceCodeLocation);
            return;
        }

        if (treeElement instanceof WI.BreakpointTreeElement) {
            let breakpoint = treeElement.breakpoint;
            if (WI.debuggerManager.isBreakpointSpecial(breakpoint))
                return;

            if (treeElement.treeOutline === this._pauseReasonTreeOutline) {
                WI.showSourceCodeLocation(breakpoint.sourceCodeLocation);
                return;
            }

            if (treeElement.parent.representedObject) {
                console.assert(treeElement.parent.representedObject instanceof WI.SourceCode);
                if (treeElement.parent.representedObject instanceof WI.SourceCode) {
                    WI.showSourceCodeLocation(breakpoint.sourceCodeLocation);
                    return;
                }
            }
        }

        console.error("Unknown tree element", treeElement);
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
                    WI.domDebuggerManager.removeURLBreakpoint(WI.domDebuggerManager.allRequestsBreakpoint);
                else {
                    WI.domDebuggerManager.allRequestsBreakpoint.disabled = false;
                    WI.domDebuggerManager.addURLBreakpoint(WI.domDebuggerManager.allRequestsBreakpoint);
                }
            }, allRequestsBreakpointShown);

            contextMenu.appendItem(WI.UIString("URL Breakpoint\u2026"), () => {
                let popover = new WI.URLBreakpointPopover(this);
                popover.show(event.target.element, [WI.RectEdge.MAX_Y, WI.RectEdge.MIN_Y, WI.RectEdge.MAX_X]);
            });
        }

        contextMenu.show();
    }

    _handleFrameMainResourceDidChange(event)
    {
        if (event.target.isMainFrame()) {
            this._updateMainFrameTreeElement(event.target);

            for (let domBreakpoint of WI.domDebuggerManager.domBreakpoints)
                this._removeBreakpoint(domBreakpoint);
        }

        if (!event.data.oldMainResource) {
            let resource = event.target.mainResource;
            this._addBreakpointsForSourceCode(resource);
            this._addIssuesForSourceCode(resource);
        }
    }

    _handleResourceAdded(event)
    {
        this._addResource(event.data.resource);
    }

    _handleMainFrameDidChange(event)
    {
        this._updateMainFrameTreeElement(WI.networkManager.mainFrame);
    }

    _handleDebuggerBreakpointAdded(event)
    {
        this._addBreakpoint(event.data.breakpoint);
    }

    _handleDebuggerBreakpointRemoved(event)
    {
        this._removeBreakpoint(event.data.breakpoint);
    }

    _handleDebuggerBreakpointsEnabledDidChange(event)
    {
        this._debuggerBreakpointsButtonItem.activated = WI.debuggerManager.breakpointsEnabled;

        this._updateBreakpointsDisabledBanner();
    }

    _handleDebuggerScriptAdded(event)
    {
        this._addScript(event.data.script);
    }

    _handleDebuggerScriptRemoved(event)
    {
        let script = event.data.script;
        let scriptTreeElement = this._resourcesTreeOutline.findTreeElement(script);
        if (!scriptTreeElement)
            return;

        let parentTreeElement = scriptTreeElement.parent;
        parentTreeElement.removeChild(scriptTreeElement);

        if (parentTreeElement instanceof WI.FolderTreeElement) {
            parentTreeElement.representedObject.remove(script);

            if (!parentTreeElement.children.length)
                parentTreeElement.parent.removeChild(parentTreeElement);
        }
    }

    _handleDebuggerScriptsCleared(event)
    {
        const suppressOnDeselect = true;
        const suppressSelectSibling = true;

        for (let i = this._breakpointsTreeOutline.children.length - 1; i >= 0; --i) {
            let treeElement = this._breakpointsTreeOutline.children[i];
            if (!(treeElement instanceof WI.ScriptTreeElement))
                continue;

            this._breakpointsTreeOutline.removeChild(treeElement, suppressOnDeselect, suppressSelectSibling);
        }

        if (this._extensionScriptsFolderTreeElement) {
            if (this._extensionScriptsFolderTreeElement.parent)
                this._extensionScriptsFolderTreeElement.parent.removeChild(this._extensionScriptsFolderTreeElement, suppressOnDeselect, suppressSelectSibling);

            this._extensionScriptsFolderTreeElement.representedObject.clear();
            this._extensionScriptsFolderTreeElement = null;
        }

        if (this._extraScriptsFolderTreeElement) {
            if (this._extraScriptsFolderTreeElement.parent)
                this._extraScriptsFolderTreeElement.parent.removeChild(this._extraScriptsFolderTreeElement, suppressOnDeselect, suppressSelectSibling);

            this._extraScriptsFolderTreeElement.representedObject.clear();
            this._extraScriptsFolderTreeElement = null;
        }

        if (this._anonymousScriptsFolderTreeElement) {
            if (this._anonymousScriptsFolderTreeElement.parent)
                this._anonymousScriptsFolderTreeElement.parent.removeChild(this._anonymousScriptsFolderTreeElement, suppressOnDeselect, suppressSelectSibling);

            this._anonymousScriptsFolderTreeElement.representedObject.clear();
            this._anonymousScriptsFolderTreeElement = null;
        }

        if (this._workerTargetTreeElementMap.size) {
            for (let treeElement of this._workerTargetTreeElementMap.values())
                treeElement.parent.removeChild(treeElement, suppressOnDeselect, suppressSelectSibling);
            this._workerTargetTreeElementMap.clear();
        }

        this._addResourcesRecursivelyForFrame(WI.networkManager.mainFrame);
    }

    _handleDebuggerPaused(event)
    {
        this.contentView.element.insertBefore(this._callStackSection.element, this.contentView.element.firstChild);

        if (this._updatePauseReason())
            this.contentView.element.insertBefore(this._pauseReasonSection.element, this.contentView.element.firstChild);

        this._debuggerPauseResumeButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.toggled = true;
        this._debuggerStepOverButtonItem.enabled = true;
        this._debuggerStepIntoButtonItem.enabled = true;
        this._debuggerStepOutButtonItem.enabled = true;

        this.element.classList.add("paused");
    }

    _handleDebuggerResumed(event)
    {
        this._callStackSection.element.remove();

        this._pauseReasonSection.element.remove();

        this._debuggerPauseResumeButtonItem.enabled = true;
        this._debuggerPauseResumeButtonItem.toggled = false;
        this._debuggerStepOverButtonItem.enabled = false;
        this._debuggerStepIntoButtonItem.enabled = false;
        this._debuggerStepOutButtonItem.enabled = false;

        this.element.classList.remove("paused");
    }

    _handleDebuggerCallFramesDidChange(event)
    {
        let {target} = event.data;
        let treeElement = this._findCallStackTargetTreeElement(target);
        console.assert(treeElement);
        if (treeElement)
            treeElement.refresh();
    }

    _handleDebuggerActiveCallFrameDidChange(event)
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

    _handleDebuggerWaitingToPause(event)
    {
        this._debuggerPauseResumeButtonItem.enabled = false;
    }

    _handleDebuggerObjectDisplayLocationDidChange(event)
    {
        let debuggerObject = event.target;

        if (event.data.oldDisplaySourceCode === debuggerObject.sourceCodeLocation.displaySourceCode)
            return;

        // A known debugger object (breakpoint, issueMessage, etc.) moved between resources. Remove
        // the old tree element and create a new tree element with the updated file.

        let wasSelected = false;
        let oldDebuggerTreeElement = null;
        let newDebuggerTreeElement = null;
        if (debuggerObject instanceof WI.Breakpoint) {
            oldDebuggerTreeElement = this._breakpointsTreeOutline.findTreeElement(debuggerObject);
            if (oldDebuggerTreeElement) {
                newDebuggerTreeElement = this._addBreakpoint(debuggerObject);
                wasSelected = oldDebuggerTreeElement.selected;
            }
        } else if (debuggerObject instanceof WI.IssueMessage) {
            oldDebuggerTreeElement = this._resourcesTreeOutline.findTreeElement(debuggerObject);
            if (oldDebuggerTreeElement) {
                newDebuggerTreeElement = this._addIssue(debuggerObject);
                wasSelected = oldDebuggerTreeElement.selected;
            }
        }

        if (oldDebuggerTreeElement)
            this._removeDebuggerTreeElement(oldDebuggerTreeElement);

        if (newDebuggerTreeElement && wasSelected)
            newDebuggerTreeElement.revealAndSelect(true, false, true);
    }

    _handleDOMBreakpointResolvedStateDidChange(event)
    {
        let breakpoint = event.target;
        if (breakpoint.domNodeIdentifier)
            this._addBreakpoint(breakpoint);
        else
            this._removeBreakpoint(breakpoint);
    }

    _handleConsoleIssueAdded(event)
    {
        let {issue} = event.data;

        // We only want to show issues originating from JavaScript source code.
        if (!issue.sourceCodeLocation || !issue.sourceCodeLocation.sourceCode || (issue.source !== "javascript" && issue.source !== "console-api"))
            return;

        this._addIssue(issue);
    }

    _handleConsoleCleared(event)
    {
        let issueTreeElements = [];
        let currentTreeElement = this._resourcesTreeOutline.children[0];
        while (currentTreeElement && !currentTreeElement.root) {
            if (currentTreeElement instanceof WI.IssueTreeElement)
                issueTreeElements.push(currentTreeElement);

            const skipUnrevealed = false;
            const stayWithin = null;
            const dontPopulate = true;
            currentTreeElement = currentTreeElement.traverseNextTreeElement(skipUnrevealed, stayWithin, dontPopulate);
        }

        issueTreeElements.forEach((treeElement) => treeElement.parent.removeChild(treeElement));
    }

    _handleTimelineCapturingWillStart(event)
    {
        this._updateTemporarilyDisabledBreakpointsButtons();

        if (!this._timelineRecordingWarningElement) {
            let stopRecordingButton = document.createElement("button");
            stopRecordingButton.textContent = WI.UIString("Stop recording");
            stopRecordingButton.addEventListener("click", () => {
                WI.timelineManager.stopCapturing();
            });

            this._timelineRecordingWarningElement = document.createElement("div");
            this._timelineRecordingWarningElement.classList.add("warning-banner");
            this._timelineRecordingWarningElement.append(WI.UIString("Debugger disabled during Timeline recording"), document.createElement("br"), stopRecordingButton);
        }

        this.contentView.element.insertBefore(this._timelineRecordingWarningElement, this.contentView.element.firstChild);

        this._updateBreakpointsDisabledBanner();
    }

    _handleTimelineCapturingStopped(event)
    {
        this._updateTemporarilyDisabledBreakpointsButtons();

        if (this._timelineRecordingWarningElement) {
            this._timelineRecordingWarningElement.remove();
            this._timelineRecordingWarningElement = null;
        }

        this._updateBreakpointsDisabledBanner();
    }

    _handleAuditManagerTestScheduled(event)
    {
        this._updateTemporarilyDisabledBreakpointsButtons();

        if (!this._auditTestWarningElement) {
            let stopAuditButton = document.createElement("button");
            stopAuditButton.textContent = WI.UIString("Stop Audit");
            stopAuditButton.addEventListener("click", (event) => {
                WI.auditManager.stop();
            });

            this._auditTestWarningElement = document.createElement("div");
            this._auditTestWarningElement.classList.add("warning-banner");
            this._auditTestWarningElement.append(WI.UIString("Debugger disabled during Audit"), document.createElement("br"), stopAuditButton);
        }

        this.contentView.element.insertBefore(this._auditTestWarningElement, this.contentView.element.firstChild);

        this._updateBreakpointsDisabledBanner();
    }

    _handleAuditManagerTestCompleted(event)
    {
        this._updateTemporarilyDisabledBreakpointsButtons();

        if (this._auditTestWarningElement) {
            this._auditTestWarningElement.remove();
            this._auditTestWarningElement = null;
        }

        this._updateBreakpointsDisabledBanner();
    }

    _handleCSSStyleSheetAdded(event)
    {
        let styleSheet = event.data.styleSheet;
        if (!styleSheet.isInspectorStyleSheet())
            return;

        let frameTreeElement = this.treeElementForRepresentedObject(styleSheet.parentFrame);
        if (!frameTreeElement)
            return;

        frameTreeElement.addRepresentedObjectToNewChildQueue(styleSheet);
    }

    _handleTargetAdded(event)
    {
        this._addTarget(event.data.target);
    }

    _handleTargetRemoved(event)
    {
        let {target} = event.data;

        let workerTreeElement = this._workerTargetTreeElementMap.take(target);
        if (workerTreeElement)
            workerTreeElement.parent.removeChild(workerTreeElement);

        let callStackTreeElement = this._findCallStackTargetTreeElement(target);
        console.assert(callStackTreeElement);
        if (callStackTreeElement)
            this._callStackTreeOutline.removeChild(callStackTreeElement);

        this._updateCallStackTreeOutline();
    }

    _handleExtraDomainsActivated()
    {
        if (WI.SourcesNavigationSidebarPanel.shouldPlaceResourcesAtTopLevel())
            this._resourcesTreeOutline.disclosureButtons = true;
    }
};

WI.SourcesNavigationSidebarPanel.ResourceTypeSymbol = Symbol("resource-type");
