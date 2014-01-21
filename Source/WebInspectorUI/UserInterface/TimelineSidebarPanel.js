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

WebInspector.TimelineSidebarPanel = function()
{
    WebInspector.NavigationSidebarPanel.call(this, "timeline", WebInspector.UIString("Timelines"), "Images/NavigationItemStopwatch.svg", "2");

    this._timelineEventsTitleBarElement = document.createElement("div");
    this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
    this._timelineEventsTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass);
    this.element.insertBefore(this._timelineEventsTitleBarElement, this.element.firstChild);

    this.contentTreeOutlineLabel = "";

    this._timelinesContentContainer = document.createElement("div");
    this._timelinesContentContainer.classList.add(WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass);
    this.element.insertBefore(this._timelinesContentContainer, this.element.firstChild);

    this._timelinesTreeOutline = this.createContentTreeOutline(true, true);
    this._timelinesTreeOutline.element.classList.add(WebInspector.NavigationSidebarPanel.HideDisclosureButtonsStyleClassName);
    this._timelinesTreeOutline.onselect = this._timelinesTreeElementSelected.bind(this);
    this._timelinesContentContainer.appendChild(this._timelinesTreeOutline.element);

    var timelinesTitleBarElement = document.createElement("div");
    timelinesTitleBarElement.textContent = WebInspector.UIString("Timelines");
    timelinesTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TitleBarStyleClass);
    timelinesTitleBarElement.classList.add(WebInspector.TimelineSidebarPanel.TimelinesTitleBarStyleClass);
    this.element.insertBefore(timelinesTitleBarElement, this.element.firstChild);

    this._navigationBar = new WebInspector.NavigationBar;
    this.element.insertBefore(this._navigationBar.element, this.element.firstChild);

    function createTimelineTreeElement(label, iconClass, identifier)
    {
        var treeElement = new WebInspector.GeneralTreeElement([iconClass, WebInspector.TimelineSidebarPanel.LargeIconStyleClass], label, null, identifier);
        var closeButton = document.createElement("img");
        closeButton.classList.add(WebInspector.TimelineSidebarPanel.CloseButtonStyleClass);
        closeButton.addEventListener("click", this.showTimelineOverview.bind(this));
        treeElement.status = closeButton;
        return treeElement;
    }

    var networkTimelineTreeElement = createTimelineTreeElement.call(this, WebInspector.UIString("Network Requests"), WebInspector.TimelineSidebarPanel.NetworkIconStyleClass, WebInspector.TimelineRecord.Type.Network);
    var layoutTimelineTreeElement = createTimelineTreeElement.call(this, WebInspector.UIString("Layout & Rendering"), WebInspector.TimelineSidebarPanel.ColorsIconStyleClass, WebInspector.TimelineRecord.Type.Layout);
    var scriptTimelineTreeElement = createTimelineTreeElement.call(this, WebInspector.UIString("JavaScript & Events"), WebInspector.TimelineSidebarPanel.ScriptIconStyleClass, WebInspector.TimelineRecord.Type.Script);

    this._timelinesTreeOutline.appendChild(networkTimelineTreeElement);
    this._timelinesTreeOutline.appendChild(layoutTimelineTreeElement);
    this._timelinesTreeOutline.appendChild(scriptTimelineTreeElement);

    this._timelineTreeElementMap = {
        [WebInspector.TimelineRecord.Type.Network]: networkTimelineTreeElement,
        [WebInspector.TimelineRecord.Type.Layout]: layoutTimelineTreeElement,
        [WebInspector.TimelineRecord.Type.Script]: scriptTimelineTreeElement
    };

    this._timelineOverviewTreeElement = new WebInspector.GeneralTreeElement(WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass, WebInspector.UIString("Timelines"), null, WebInspector.timelineManager.recording);
    this._timelineOverviewTreeElement.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this.showTimelineOverview, this);

    this._stripeBackgroundElement = document.createElement("div");
    this._stripeBackgroundElement.className = WebInspector.TimelineSidebarPanel.StripeBackgroundStyleClass;
    this.contentElement.insertBefore(this._stripeBackgroundElement, this.contentElement.firstChild);

    WebInspector.contentBrowser.addEventListener(WebInspector.ContentBrowser.Event.CurrentContentViewDidChange, this._contentBrowserCurrentContentViewDidChange, this);

    function delayedWork()
    {
        // Prime the creation of the singleton TimelineContentCiew since it needs to listen for events.
        // It needs to be delayed since TimelineContentView depends on WebInspector.timelineSidebarPanel existing.
        this._timelineContentView = WebInspector.contentBrowser.contentViewForRepresentedObject(WebInspector.timelineManager.recording);
    }

    setTimeout(delayedWork.bind(this), 0);
};

WebInspector.TimelineSidebarPanel.TitleBarStyleClass = "title-bar";
WebInspector.TimelineSidebarPanel.TimelinesTitleBarStyleClass = "timelines";
WebInspector.TimelineSidebarPanel.TimelineEventsTitleBarStyleClass = "timeline-events";
WebInspector.TimelineSidebarPanel.TimelinesContentContainerStyleClass = "timelines-content";
WebInspector.TimelineSidebarPanel.StripeBackgroundStyleClass = "stripe-background";
WebInspector.TimelineSidebarPanel.CloseButtonStyleClass = "close-button";
WebInspector.TimelineSidebarPanel.LargeIconStyleClass = "large";
WebInspector.TimelineSidebarPanel.StopwatchIconStyleClass = "stopwatch-icon";
WebInspector.TimelineSidebarPanel.NetworkIconStyleClass = "network-icon";
WebInspector.TimelineSidebarPanel.ColorsIconStyleClass = "colors-icon";
WebInspector.TimelineSidebarPanel.ScriptIconStyleClass = "script-icon";
WebInspector.TimelineSidebarPanel.TimelineContentViewShowingStyleClass = "timeline-content-view-showing";
WebInspector.TimelineSidebarPanel.ShowingTimelineContentViewCookieKey = "timeline-sidebar-panel-showing-timeline-content-view";
WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey = "timeline-sidebar-panel-selected-timeline-view-identifier";
WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue = "overview";

WebInspector.TimelineSidebarPanel.prototype = {
    constructor: WebInspector.TimelineSidebarPanel,
    __proto__: WebInspector.NavigationSidebarPanel.prototype,

    // Public

    showDefaultContentView: function()
    {
        WebInspector.contentBrowser.showContentView(this._timelineContentView);
    },

    treeElementForRepresentedObject: function(representedObject)
    {
        if (representedObject instanceof WebInspector.TimelineRecording)
            return this._timelineOverviewTreeElement;

        // The main resource is used as the representedObject instead of Frame in our tree.
        if (representedObject instanceof WebInspector.Frame)
            representedObject = representedObject.mainResource;

        var foundTreeElement = this.contentTreeOutline.getCachedTreeElement(representedObject);
        if (foundTreeElement)
            return foundTreeElement;

        // Look for TreeElements loosely based on represented objects that can contain the represented
        // object we are really looking for. This allows a SourceCodeTimelineTreeElement or a
        // TimelineRecordTreeElement to stay selected when the Resource it represents is showing.

        function looselyCompareRepresentedObjects(candidateTreeElement)
        {
            if (!candidateTreeElement)
                return false;

            var candidateRepresentedObject = candidateTreeElement.representedObject;
            if (candidateRepresentedObject instanceof WebInspector.SourceCodeTimeline) {
                if (candidateRepresentedObject.sourceCode === representedObject)
                    return true;
                return false;
            }

            if (candidateRepresentedObject instanceof WebInspector.TimelineRecord) {
                if (!candidateRepresentedObject.sourceCodeLocation)
                    return false;
                if (candidateRepresentedObject.sourceCodeLocation.sourceCode === representedObject)
                    return true;
                return false;
            }

            console.error("Unknown TreeElement");
            return false;
        }

        // Check the selected tree element first so we don't need to do a longer search and it is
        // likely to be the best candidate for the current view.
        if (looselyCompareRepresentedObjects(this.contentTreeOutline.selectedTreeElement))
            return this.contentTreeOutline.selectedTreeElement;

        var currentTreeElement = this._contentTreeOutline.children[0];
        while (currentTreeElement && !currentTreeElement.root) {
            if (looselyCompareRepresentedObjects(currentTreeElement))
                return currentTreeElement;
            currentTreeElement = currentTreeElement.traverseNextTreeElement(false, null, false);
        }

        return null;
    },

    get contentTreeOutlineLabel()
    {
        return this._timelineEventsTitleBarElement.textContent;
    },

    set contentTreeOutlineLabel(label)
    {
        label = label || WebInspector.UIString("Timeline Events");

        this._timelineEventsTitleBarElement.textContent = label;
        this.filterBar.placeholder = WebInspector.UIString("Filter %s").format(label);
    },

    showTimelineOverview: function()
    {
        if (this._timelinesTreeOutline.selectedTreeElement)
            this._timelinesTreeOutline.selectedTreeElement.deselect();

        this._timelineContentView.showOverviewTimelineView();
        WebInspector.contentBrowser.showContentView(this._timelineContentView);
    },

    showTimelineView: function(identifier)
    {
        if (!this._timelineTreeElementMap[identifier])
            return;

        this._timelineTreeElementMap[identifier].select(true, false, true, true);

        this._timelineContentView.showTimelineView(identifier);
        WebInspector.contentBrowser.showContentView(this._timelineContentView);
    },

    // Protected

    updateCustomContentOverflow: function()
    {
        if (!this._stripeBackgroundElement)
            return;

        var contentHeight = this.contentTreeOutline.element.offsetHeight;
        var currentHeight = parseInt(this._stripeBackgroundElement.style.height);
        if (currentHeight !== contentHeight)
            this._stripeBackgroundElement.style.height = contentHeight + "px";
    },

    hasCustomFilters: function()
    {
        return true;
    },

    matchTreeElementAgainstCustomFilters: function(treeElement)
    {
        if (!this._timelineContentView)
            return true;

        return this._timelineContentView.matchTreeElementAgainstCustomFilters(treeElement);
    },

    canShowDifferentContentView: function()
    {
        return !this.restoringState || !this._restoredShowingTimelineContentView;
    },

    saveStateToCookie: function(cookie)
    {
        console.assert(cookie);

        cookie[WebInspector.timelineSidebarPanel.ShowingTimelineContentViewCookieKey] = WebInspector.contentBrowser.currentContentView instanceof WebInspector.TimelineContentView;

        var selectedTreeElement = this._timelinesTreeOutline.selectedTreeElement;
        if (selectedTreeElement)
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = selectedTreeElement.representedObject;
        else
            cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey] = WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue;

        WebInspector.NavigationSidebarPanel.prototype.saveStateToCookie.call(this, cookie);
    },

    restoreStateFromCookie: function(cookie, relaxedMatchDelay)
    {
        console.assert(cookie);

        // The _timelineContentView is not ready on initial load, so delay the restore.
        // This matches the delayed work in the WebInspector.TimelineSidebarPanel constructor.
        if (!this._timelineContentView) {
            setTimeout(this.restoreStateFromCookie.bind(this, cookie, relaxedMatchDelay), 0);
            return;
        }

        this._restoredShowingTimelineContentView = cookie[WebInspector.timelineSidebarPanel.ShowingTimelineContentViewCookieKey];

        var selectedTimelineViewIdentifier = cookie[WebInspector.TimelineSidebarPanel.SelectedTimelineViewIdentifierCookieKey];
        if (selectedTimelineViewIdentifier === WebInspector.TimelineSidebarPanel.OverviewTimelineIdentifierCookieValue)
            this.showTimelineOverview();
        else
            this.showTimelineView(selectedTimelineViewIdentifier);

        WebInspector.NavigationSidebarPanel.prototype.restoreStateFromCookie.call(this, cookie, relaxedMatchDelay);
    },

    // Private

    _timelinesTreeElementSelected: function(treeElement, selectedByUser)
    {
        console.assert(this._timelineTreeElementMap[treeElement.representedObject] === treeElement);
        this.showTimelineView(treeElement.representedObject);
    },

    _contentBrowserCurrentContentViewDidChange: function(event)
    {
        if (WebInspector.contentBrowser.currentContentView instanceof WebInspector.TimelineContentView)
            this.element.classList.add(WebInspector.TimelineSidebarPanel.TimelineContentViewShowingStyleClass);
        else
            this.element.classList.remove(WebInspector.TimelineSidebarPanel.TimelineContentViewShowingStyleClass);
    }
};
