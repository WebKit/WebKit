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

WebInspector.TimelineContentView = function(recording)
{
    WebInspector.ContentView.call(this, recording);

    this.element.classList.add(WebInspector.TimelineContentView.StyleClassName);

    this._timelineOverview = new WebInspector.TimelineOverview;
    this.element.appendChild(this._timelineOverview.element);

    this._currentTimeMarker = new WebInspector.TimelineMarker(0, WebInspector.TimelineMarker.Type.CurrentTime);
    this._timelineOverview.addMarker(this._currentTimeMarker);

    this._viewContainer = document.createElement("div");
    this._viewContainer.classList.add(WebInspector.TimelineContentView.ViewContainerStyleClassName);
    this.element.appendChild(this._viewContainer);

    this._overviewTimelineView = new WebInspector.OverviewTimelineView;
    this._discreteTimelineViewMap = {
        [WebInspector.TimelineRecord.Type.Network]: new WebInspector.TimelineView,
        [WebInspector.TimelineRecord.Type.Layout]: new WebInspector.TimelineView,
        [WebInspector.TimelineRecord.Type.Script]: new WebInspector.TimelineView
    };

    function createPathComponent(displayName, className, representedObject)
    {
        var pathComponent = new WebInspector.HierarchicalPathComponent(displayName, className, representedObject);
        pathComponent.addEventListener(WebInspector.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
        return pathComponent;
    }

    var networkPathComponent = createPathComponent.call(this, WebInspector.UIString("Network Requests"), WebInspector.TimelineSidebarPanel.NetworkIconStyleClass, WebInspector.TimelineRecord.Type.Network);
    var layoutPathComponent = createPathComponent.call(this, WebInspector.UIString("Layout & Rendering"), WebInspector.TimelineSidebarPanel.ColorsIconStyleClass, WebInspector.TimelineRecord.Type.Layout);
    var scriptPathComponent = createPathComponent.call(this, WebInspector.UIString("JavaScript & Events"), WebInspector.TimelineSidebarPanel.ScriptIconStyleClass, WebInspector.TimelineRecord.Type.Script);

    networkPathComponent.nextSibling = layoutPathComponent;
    layoutPathComponent.previousSibling = networkPathComponent;
    layoutPathComponent.nextSibling = scriptPathComponent;
    scriptPathComponent.previousSibling = layoutPathComponent;

    this._pathComponentMap = {
        [WebInspector.TimelineRecord.Type.Network]: networkPathComponent,
        [WebInspector.TimelineRecord.Type.Layout]: layoutPathComponent,
        [WebInspector.TimelineRecord.Type.Script]: scriptPathComponent
    };

    this._currentTimelineView = null;
    this._currentTimelineViewIdentifier = null;

    this._updating = false;
    this._lastUpdateTimestamp = NaN;

    WebInspector.timelineManager.recording.addEventListener(WebInspector.TimelineRecording.Event.Reset, this._recordingReset, this);
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingStarted, this._recordingStarted, this);
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.Event.RecordingStopped, this._recordingStopped, this);

    this.showOverviewTimelineView();
};

WebInspector.TimelineContentView.StyleClassName = "timeline";
WebInspector.TimelineContentView.ViewContainerStyleClassName = "view-container";

WebInspector.TimelineContentView.prototype = {
    constructor: WebInspector.TimelineContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    showOverviewTimelineView: function()
    {
        this._showTimelineView(this._overviewTimelineView);
    },

    showTimelineView: function(identifier)
    {
        console.assert(identifier in this._discreteTimelineViewMap);
        if (!(identifier in this._discreteTimelineViewMap))
            return;

        this._showTimelineView(this._discreteTimelineViewMap[identifier], identifier);
    },

    get allowedNavigationSidebarPanels()
    {
        return ["timeline"];
    },

    get supportsSplitContentBrowser()
    {
        // The layout of the overview and split content browser don't work well.
        return false;
    },

    get selectionPathComponents()
    {
        if (!this._currentTimelineViewIdentifier)
            return [];
        return [this._pathComponentMap[this._currentTimelineViewIdentifier]];
    },

    shown: function()
    {
        if (!this._currentTimelineView)
            return;

        this._currentTimelineView.shown();
    },

    hidden: function()
    {
        if (!this._currentTimelineView)
            return;

        this._currentTimelineView.hidden();
    },

    updateLayout: function()
    {
        this._timelineOverview.updateLayout();

        if (!this._currentTimelineView)
            return;

        this._currentTimelineView.updateLayout();
    },

    // Private

    _pathComponentSelected: function(event)
    {
        WebInspector.timelineSidebarPanel.showTimelineView(event.data.pathComponent.representedObject);
    },

    _showTimelineView: function(timelineView, identifier)
    {
        console.assert(timelineView instanceof WebInspector.TimelineView);

        if (this._currentTimelineView === timelineView)
            return;

        if (this._currentTimelineView) {
            this._currentTimelineView.hidden();
            this._currentTimelineView.element.remove();
        }

        this._currentTimelineView = timelineView;
        this._currentTimelineViewIdentifier = identifier || null;

        WebInspector.timelineSidebarPanel.contentTreeOutline = timelineView && timelineView.navigationSidebarTreeOutline;
        WebInspector.timelineSidebarPanel.contentTreeOutlineLabel = timelineView && timelineView.navigationSidebarTreeOutlineLabel;

        if (this._currentTimelineView) {
            this._viewContainer.appendChild(this._currentTimelineView.element);
            this._currentTimelineView.shown();
        }

        this.dispatchEventToListeners(WebInspector.ContentView.Event.SelectionPathComponentsDidChange);
    },

    _update: function(timestamp)
    {
        var startTime = WebInspector.timelineManager.recording.startTime;
        var currentTime = this._currentTimeMarker.time || startTime;
        var endTime = WebInspector.timelineManager.recording.endTime;
        var timespanSinceLastUpdate = (timestamp - this._lastUpdateTimestamp) / 1000 || 0;

        currentTime += timespanSinceLastUpdate;

        this._currentTimeMarker.time = currentTime;

        if (this._startTimeNeedsReset && !isNaN(startTime)) {
            var selectionOffset = this._timelineOverview.selectionStartTime - this._timelineOverview.startTime;
            this._timelineOverview.startTime = startTime;
            this._timelineOverview.selectionStartTime = startTime + selectionOffset;
            delete this._startTimeNeedsReset;
        }

        this._timelineOverview.endTime = Math.max(endTime, currentTime);

        // Force a layout now since we are already in an animation frame and don't need to delay it until the next.
        this._timelineOverview.updateLayout();

        this._timelineOverview.revealMarker(this._currentTimeMarker);

        // Only stop updating if the current time is greater than the end time.
        if (!this._updating && currentTime >= endTime) {
            this._lastUpdateTimestamp = NaN;
            return;
        }

        this._lastUpdateTimestamp = timestamp;

        requestAnimationFrame(this._updateCallback);
    },

    _recordingStarted: function(event)
    {
        console.assert(!this._updating);
        if (this._updating)
            return;

        this._updating = true;

        if (!this._updateCallback)
            this._updateCallback = this._update.bind(this);

        requestAnimationFrame(this._updateCallback);
    },

    _recordingStopped: function(event)
    {
        console.assert(this._updating);
        this._updating = false;
    },

    _recordingReset: function(event)
    {
        this._startTimeNeedsReset = true;
        this._currentTimeMarker.time = 0;

        this._overviewTimelineView.reset();

        for (var identifier in this._discreteTimelineViewMap)
            this._discreteTimelineViewMap[identifier].reset();
    }
};
