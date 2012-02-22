/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.Panel}
 */
WebInspector.TimelinePanel = function()
{
    WebInspector.Panel.call(this, "timeline");
    this.registerRequiredCSS("timelinePanel.css");

    this._presentationModel = new WebInspector.TimelinePresentationModel();
    this._presentationModel.addCategory(new WebInspector.TimelineCategory("loading", WebInspector.UIString("Loading"), "rgb(47,102,236)"));
    this._presentationModel.addCategory(new WebInspector.TimelineCategory("scripting", WebInspector.UIString("Scripting"), "rgb(157,231,119)"));
    this._presentationModel.addCategory(new WebInspector.TimelineCategory("rendering", WebInspector.UIString("Rendering"), "rgb(164,60,255)"));
    this._presentationModel.addEventListener(WebInspector.TimelinePresentationModel.Events.WindowChanged, this._scheduleRefresh.bind(this, false));
    this._presentationModel.addEventListener(WebInspector.TimelinePresentationModel.Events.CategoryVisibilityChanged, this._scheduleRefresh.bind(this, true));

    this._overviewPane = new WebInspector.TimelineOverviewPane(this._presentationModel);

    this.element.appendChild(this._overviewPane.element);
    this.element.addEventListener("contextmenu", this._contextMenu.bind(this), true);
    this.element.tabIndex = 0;

    this._sidebarBackgroundElement = document.createElement("div");
    this._sidebarBackgroundElement.className = "sidebar split-view-sidebar-left timeline-sidebar-background";
    this.element.appendChild(this._sidebarBackgroundElement);

    this.createSplitViewWithSidebarTree();
    this._containerElement = this.splitView.element;
    this._containerElement.id = "timeline-container";
    this._containerElement.addEventListener("scroll", this._onScroll.bind(this), false);

    if (WebInspector.experimentsSettings.showMemoryCounters.isEnabled()) {
        this._timelineMemorySplitter = this.element.createChild("div");
        this._timelineMemorySplitter.id = "timeline-memory-splitter";
        this._timelineMemorySplitter.addEventListener("mousedown", this._startSplitterDragging.bind(this), false);
        this._timelineMemorySplitter.addStyleClass("hidden");
        this._memoryStatistics = new WebInspector.MemoryStatistics(this, this.splitView.preferredSidebarWidth());
        this._overviewPane.addEventListener(WebInspector.TimelineOverviewPane.Events.ModeChanged, this._timelinesOverviewModeChanged, this);
    }

    var itemsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RECORDS"), {}, true);
    this.sidebarTree.appendChild(itemsTreeElement);

    this._sidebarListElement = document.createElement("div");
    this.sidebarElement.appendChild(this._sidebarListElement);

    this._containerContentElement = this.splitView.mainElement;
    this._containerContentElement.id = "resources-container-content";

    this._timelineGrid = new WebInspector.TimelineGrid();
    this._itemsGraphsElement = this._timelineGrid.itemsGraphsElement;
    this._itemsGraphsElement.id = "timeline-graphs";
    this._itemsGraphsElement.addEventListener("mousewheel", this._overviewPane.scrollWindow.bind(this._overviewPane), true);
    this._containerContentElement.appendChild(this._timelineGrid.element);

    this._topGapElement = document.createElement("div");
    this._topGapElement.className = "timeline-gap";
    this._itemsGraphsElement.appendChild(this._topGapElement);

    this._graphRowsElement = document.createElement("div");
    this._itemsGraphsElement.appendChild(this._graphRowsElement);

    this._bottomGapElement = document.createElement("div");
    this._bottomGapElement.className = "timeline-gap";
    this._itemsGraphsElement.appendChild(this._bottomGapElement);

    this._expandElements = document.createElement("div");
    this._expandElements.id = "orphan-expand-elements";
    this._itemsGraphsElement.appendChild(this._expandElements);

    this._rootRecord = this._createRootRecord();
    this._sendRequestRecords = {};
    this._scheduledResourceRequests = {};
    this._timerRecords = {};
    this._requestAnimationFrameRecords = {};

    this._calculator = new WebInspector.TimelineCalculator();
    var shortRecordThresholdTitle = Number.secondsToString(WebInspector.TimelinePanel.shortRecordThreshold);
    this._showShortRecordsTitleText = WebInspector.UIString("Show the records that are shorter than %s", shortRecordThresholdTitle);
    this._hideShortRecordsTitleText = WebInspector.UIString("Hide the records that are shorter than %s", shortRecordThresholdTitle);
    this._createStatusbarButtons();

    this._boundariesAreValid = true;
    this._scrollTop = 0;

    this._popoverHelper = new WebInspector.PopoverHelper(this._containerElement, this._getPopoverAnchor.bind(this), this._showPopover.bind(this));
    this._containerElement.addEventListener("mousemove", this._mouseMove.bind(this), false);
    this._containerElement.addEventListener("mouseout", this._mouseOut.bind(this), false);

    // Disable short events filter by default.
    this.toggleFilterButton.toggled = true;
    this._showShortEvents = this.toggleFilterButton.toggled;
    this._timeStampRecords = [];
    this._expandOffset = 15;

    this._createFileSelector();
    this._model = new WebInspector.TimelineModel();
    this._model.addEventListener(WebInspector.TimelineModel.Events.RecordAdded, this._onTimelineEventRecorded, this);
    this._model.addEventListener(WebInspector.TimelineModel.Events.RecordsCleared, this._onRecordsCleared, this);

    this._registerShortcuts();
    this._linkifier = WebInspector.debuggerPresentationModel.createLinkifier();
}

// Define row height, should be in sync with styles for timeline graphs.
WebInspector.TimelinePanel.rowHeight = 18;
WebInspector.TimelinePanel.shortRecordThreshold = 0.015;

WebInspector.TimelinePanel.prototype = {
    /**
     * @param {Event} event
     */
    _startSplitterDragging: function(event)
    {
        this._dragOffset = this._timelineMemorySplitter.offsetTop + 2 - event.pageY;
        WebInspector.elementDragStart(this._timelineMemorySplitter, this._splitterDragging.bind(this), this._endSplitterDragging.bind(this), event, "ns-resize");
    },

    /**
     * @param {Event} event
     */
    _splitterDragging: function(event)
    {
        var top = event.pageY + this._dragOffset
        this._setSplitterPosition(top);
        event.preventDefault();
    },

    /**
     * @param {Event} event
     */
    _endSplitterDragging: function(event)
    {
        delete this._dragOffset;
        WebInspector.elementDragEnd(event);
        if (this._memoryStatistics)
            this._memoryStatistics.show();
    },

    _setSplitterPosition: function(top)
    {
        const overviewHeight = 90;
        const sectionMinHeight = 100;
        top = Number.constrain(top, overviewHeight + sectionMinHeight, this.element.offsetHeight - sectionMinHeight);

        this.splitView.element.style.height = (top - overviewHeight) + "px";
        this._timelineMemorySplitter.style.top = (top - 2) + "px";
        this._memoryStatistics.setTopPosition(top);
    },

    _linkifyLocation: function(url, lineNumber, columnNumber)
    {
        // FIXME(62725): stack trace line/column numbers are one-based.
        lineNumber = lineNumber ? lineNumber - 1 : lineNumber;
        columnNumber = columnNumber ? columnNumber - 1 : 0;
        return this._linkifier.linkifyLocation(url, lineNumber, columnNumber, "timeline-details");
    },

    _linkifyCallFrame: function(callFrame)
    {
        return this._linkifyLocation(callFrame.url, callFrame.lineNumber, callFrame.columnNumber);
    },

    get calculator()
    {
        return this._calculator;
    },

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Timeline");
    },

    get statusBarItems()
    {
        var statusBarItems = [ this.toggleFilterButton.element, this.toggleTimelineButton.element, this.clearButton.element, this.garbageCollectButton.element, this._glueParentButton.element ];

        if (WebInspector.experimentsSettings.timelineStartAtZero.isEnabled())
            statusBarItems.push(this.toggleStartAtZeroButton.element);

        statusBarItems.push(this.statusBarFilters);
        return statusBarItems;
    },

    get defaultFocusedElement()
    {
        return this.element;
    },

    get _recordStyles()
    {
        if (!this._recordStylesArray) {
            var recordTypes = WebInspector.TimelineAgent.RecordType;
            var categories = this._presentationModel.categories;

            var recordStyles = {};
            recordStyles[recordTypes.EventDispatch] = { title: WebInspector.UIString("Event"), category: categories["scripting"] };
            recordStyles[recordTypes.Layout] = { title: WebInspector.UIString("Layout"), category: categories["rendering"] };
            recordStyles[recordTypes.RecalculateStyles] = { title: WebInspector.UIString("Recalculate Style"), category: categories["rendering"] };
            recordStyles[recordTypes.Paint] = { title: WebInspector.UIString("Paint"), category: categories["rendering"] };
            recordStyles[recordTypes.ParseHTML] = { title: WebInspector.UIString("Parse"), category: categories["loading"] };
            recordStyles[recordTypes.TimerInstall] = { title: WebInspector.UIString("Install Timer"), category: categories["scripting"] };
            recordStyles[recordTypes.TimerRemove] = { title: WebInspector.UIString("Remove Timer"), category: categories["scripting"] };
            recordStyles[recordTypes.TimerFire] = { title: WebInspector.UIString("Timer Fired"), category: categories["scripting"] };
            recordStyles[recordTypes.XHRReadyStateChange] = { title: WebInspector.UIString("XHR Ready State Change"), category: categories["scripting"] };
            recordStyles[recordTypes.XHRLoad] = { title: WebInspector.UIString("XHR Load"), category: categories["scripting"] };
            recordStyles[recordTypes.EvaluateScript] = { title: WebInspector.UIString("Evaluate Script"), category: categories["scripting"] };
            recordStyles[recordTypes.TimeStamp] = { title: WebInspector.UIString("Stamp"), category: categories["scripting"] };
            recordStyles[recordTypes.ResourceSendRequest] = { title: WebInspector.UIString("Send Request"), category: categories["loading"] };
            recordStyles[recordTypes.ResourceReceiveResponse] = { title: WebInspector.UIString("Receive Response"), category: categories["loading"] };
            recordStyles[recordTypes.ResourceFinish] = { title: WebInspector.UIString("Finish Loading"), category: categories["loading"] };
            recordStyles[recordTypes.FunctionCall] = { title: WebInspector.UIString("Function Call"), category: categories["scripting"] };
            recordStyles[recordTypes.ResourceReceivedData] = { title: WebInspector.UIString("Receive Data"), category: categories["loading"] };
            recordStyles[recordTypes.GCEvent] = { title: WebInspector.UIString("GC Event"), category: categories["scripting"] };
            recordStyles[recordTypes.MarkDOMContent] = { title: WebInspector.UIString("DOMContent event"), category: categories["scripting"] };
            recordStyles[recordTypes.MarkLoad] = { title: WebInspector.UIString("Load event"), category: categories["scripting"] };
            recordStyles[recordTypes.ScheduleResourceRequest] = { title: WebInspector.UIString("Schedule Request"), category: categories["loading"] };
            recordStyles[recordTypes.RequestAnimationFrame] = { title: WebInspector.UIString("Request Animation Frame"), category: categories["scripting"] };
            recordStyles[recordTypes.CancelAnimationFrame] = { title: WebInspector.UIString("Cancel Animation Frame"), category: categories["scripting"] };
            recordStyles[recordTypes.FireAnimationFrame] = { title: WebInspector.UIString("Animation Frame Fired"), category: categories["scripting"] };
            this._recordStylesArray = recordStyles;
        }
        return this._recordStylesArray;
    },

    _createStatusbarButtons: function()
    {
        this.toggleTimelineButton = new WebInspector.StatusBarButton(WebInspector.UIString("Record"), "record-profile-status-bar-item");
        this.toggleTimelineButton.addEventListener("click", this._toggleTimelineButtonClicked, this);

        this.clearButton = new WebInspector.StatusBarButton(WebInspector.UIString("Clear"), "clear-status-bar-item");
        this.clearButton.addEventListener("click", this._clearPanel, this);

        this.toggleFilterButton = new WebInspector.StatusBarButton(this._hideShortRecordsTitleText, "timeline-filter-status-bar-item");
        this.toggleFilterButton.addEventListener("click", this._toggleFilterButtonClicked, this);

        this.garbageCollectButton = new WebInspector.StatusBarButton(WebInspector.UIString("Collect Garbage"), "garbage-collect-status-bar-item");
        this.garbageCollectButton.addEventListener("click", this._garbageCollectButtonClicked, this);

        this.toggleStartAtZeroButton = new WebInspector.StatusBarButton(WebInspector.UIString("Display all top level events starting at zero"), "timeline-start-at-zero-status-bar-item");
        this.toggleStartAtZeroButton.addEventListener("click", this._toggleStartAtZeroButtonClicked, this);

        this.recordsCounter = document.createElement("span");
        this.recordsCounter.className = "timeline-records-counter";

        this._glueParentButton = new WebInspector.StatusBarButton(WebInspector.UIString("Glue asynchronous events to causes"), "glue-async-status-bar-item");
        this._glueParentButton.toggled = true;
        this._glueParentButton.addEventListener("click", this._glueParentButtonClicked, this);

        this.statusBarFilters = document.createElement("div");
        this.statusBarFilters.className = "status-bar-items";
        var categories = this._presentationModel.categories;
        for (var categoryName in categories) {
            var category = categories[categoryName];
            this.statusBarFilters.appendChild(this._createTimelineCategoryStatusBarCheckbox(category, this._onCategoryCheckboxClicked.bind(this, category)));
        }
    },

    _createTimelineCategoryStatusBarCheckbox: function(category, onCheckboxClicked)
    {
        var labelContainer = document.createElement("div");
        labelContainer.addStyleClass("timeline-category-statusbar-item");
        labelContainer.addStyleClass("timeline-category-" + category.name);
        labelContainer.addStyleClass("status-bar-item");

        var label = document.createElement("label");
        var checkElement = document.createElement("input");
        checkElement.type = "checkbox";
        checkElement.className = "timeline-category-checkbox";
        checkElement.checked = true;
        checkElement.addEventListener("click", onCheckboxClicked, false);
        label.appendChild(checkElement);

        var typeElement = document.createElement("span");
        typeElement.className = "type";
        typeElement.textContent = category.title;
        label.appendChild(typeElement);

        labelContainer.appendChild(label);
        return labelContainer;
    },

    _onCategoryCheckboxClicked: function(category, event)
    {
        this._presentationModel.setCategoryVisibility(category, event.target.checked);
    },

    _registerShortcuts: function()
    {
        var shortcut = WebInspector.KeyboardShortcut;
        var modifiers = shortcut.Modifiers;
        var section = WebInspector.shortcutsScreen.section(WebInspector.UIString("Timeline Panel"));

        this._shortcuts[shortcut.makeKey("e", modifiers.CtrlOrMeta)] = this._toggleTimelineButtonClicked.bind(this);
        section.addKey(shortcut.shortcutToString("e", modifiers.CtrlOrMeta), WebInspector.UIString("Start/stop recording"));

        if (InspectorFrontendHost.canSaveAs()) {
            this._shortcuts[shortcut.makeKey("s", modifiers.CtrlOrMeta)] = this._saveToFile.bind(this);
            section.addKey(shortcut.shortcutToString("s", modifiers.CtrlOrMeta), WebInspector.UIString("Save Timeline data\u2026"));
        }

        this._shortcuts[shortcut.makeKey("o", modifiers.CtrlOrMeta)] = this._fileSelectorElement.click.bind(this._fileSelectorElement);
        section.addKey(shortcut.shortcutToString("o", modifiers.CtrlOrMeta), WebInspector.UIString("Load Timeline data\u2026"));
    },

    _createFileSelector: function()
    {
        if (this._fileSelectorElement)
            this.element.removeChild(this._fileSelectorElement);

        var fileSelectorElement = document.createElement("input");
        fileSelectorElement.type = "file";
        fileSelectorElement.style.zIndex = -1;
        fileSelectorElement.style.position = "absolute";
        fileSelectorElement.onchange = this._loadFromFile.bind(this);
        this.element.appendChild(fileSelectorElement);
        this._fileSelectorElement = fileSelectorElement;
    },

    _contextMenu: function(event)
    {
        var contextMenu = new WebInspector.ContextMenu();
        if (InspectorFrontendHost.canSaveAs())
            contextMenu.appendItem(WebInspector.UIString("&Save Timeline data\u2026"), this._saveToFile.bind(this));
        contextMenu.appendItem(WebInspector.UIString("L&oad Timeline data\u2026"), this._fileSelectorElement.click.bind(this._fileSelectorElement));
        contextMenu.show(event);
    },

    _saveToFile: function()
    {
        this._model._saveToFile();
    },

    _loadFromFile: function()
    {
        if (this.toggleTimelineButton.toggled) {
            this.toggleTimelineButton.toggled = false;
            this._model.stopRecord();
        }
        this._model._loadFromFile(this._fileSelectorElement.files[0]);
        this._createFileSelector();
    },

    _updateRecordsCounter: function()
    {
        this.recordsCounter.textContent = WebInspector.UIString("%d of %d captured records are visible", this._rootRecord._visibleRecordsCount, this._rootRecord._allRecordsCount);
    },

    _updateEventDividers: function()
    {
        this._timelineGrid.removeEventDividers();
        var clientWidth = this._graphRowsElement.offsetWidth - this._expandOffset;
        var dividers = [];
        for (var i = 0; i < this._timeStampRecords.length; ++i) {
            var record = this._timeStampRecords[i];
            var positions = this._calculator.computeBarGraphWindowPosition(record, clientWidth);
            var dividerPosition = Math.round(positions.left);
            if (dividerPosition < 0 || dividerPosition >= clientWidth || dividers[dividerPosition])
                continue;
            var divider = this._createEventDivider(record);
            divider.style.left = (dividerPosition + this._expandOffset) + "px";
            dividers[dividerPosition] = divider;
        }
        this._timelineGrid.addEventDividers(dividers);
        this._overviewPane.updateEventDividers(this._timeStampRecords, this._createEventDivider.bind(this));
    },

    _createEventDivider: function(record)
    {
        var eventDivider = document.createElement("div");
        eventDivider.className = "resources-event-divider";
        var recordTypes = WebInspector.TimelineAgent.RecordType;

        var eventDividerPadding = document.createElement("div");
        eventDividerPadding.className = "resources-event-divider-padding";
        eventDividerPadding.title = record.title;

        if (record.type === recordTypes.MarkDOMContent)
            eventDivider.className += " resources-blue-divider";
        else if (record.type === recordTypes.MarkLoad)
            eventDivider.className += " resources-red-divider";
        else if (record.type === recordTypes.TimeStamp) {
            eventDivider.className += " resources-orange-divider";
            eventDividerPadding.title = record.data["message"];
        }
        eventDividerPadding.appendChild(eventDivider);
        return eventDividerPadding;
    },

    _timelinesOverviewModeChanged: function(event)
    {
        if (!this._memoryStatistics)
            return;
        var shouldShowMemory = event.data === WebInspector.TimelineOverviewPane.Mode.Memory;
        if (shouldShowMemory === this._memoryStatistics.visible())
            return;
        if (!shouldShowMemory) {
            this._timelineMemorySplitter.addStyleClass("hidden");
            this._memoryStatistics.hide();
            this.splitView.element.style.height = "auto";
            this.splitView.element.style.bottom = "0";
            this.onResize();
        } else {
            this._timelineMemorySplitter.removeStyleClass("hidden");
            this._memoryStatistics.show();
            this.splitView.element.style.bottom = "auto";
            this._setSplitterPosition(600);
        }
    },

    _toggleTimelineButtonClicked: function()
    {
        if (this.toggleTimelineButton.toggled)
            this._model.stopRecord();
        else {
            this._model.startRecord();
            WebInspector.userMetrics.TimelineStarted.record();
        }
        this.toggleTimelineButton.toggled = !this.toggleTimelineButton.toggled;
    },

    _toggleFilterButtonClicked: function()
    {
        this.toggleFilterButton.toggled = !this.toggleFilterButton.toggled;
        this._showShortEvents = this.toggleFilterButton.toggled;
        this.toggleFilterButton.element.title = this._showShortEvents ? this._hideShortRecordsTitleText : this._showShortRecordsTitleText;
        this._scheduleRefresh(true);
    },

    _garbageCollectButtonClicked: function()
    {
        ProfilerAgent.collectGarbage();
    },

    _glueParentButtonClicked: function()
    {
        this._glueParentButton.toggled = !this._glueParentButton.toggled;
        this._repopulateRecords();
    },

    _toggleStartAtZeroButtonClicked: function()
    {
        var toggled = !this.toggleStartAtZeroButton.toggled
        this._glueParentButton.disabled = toggled;
        this.toggleStartAtZeroButton.toggled = toggled;
        this._calculator = toggled ? new WebInspector.TimelineStartAtZeroCalculator() : new WebInspector.TimelineCalculator();
        this._repopulateRecords();
        this._overviewPane.setStartAtZero(toggled);
    },

    _repopulateRecords: function()
    {
        this._resetPanel();
        var records = this._model.records;
        for (var i = 0; i < records.length; ++i)
            this._innerAddRecordToTimeline(records[i], this._rootRecord);
        this._scheduleRefresh(false);
    },

    get _startAtZero()
    {
        return this.toggleStartAtZeroButton.toggled;
    },

    _onTimelineEventRecorded: function(event)
    {
        this._innerAddRecordToTimeline(event.data, this._rootRecord);
        this._scheduleRefresh(false);

        if (this._memoryStatistics && event.data["counters"])
            this._memoryStatistics.addTimlineEvent(event);
    },

    _findParentRecord: function(record)
    {
        if (!this._glueParentButton.toggled)
            return null;

        var recordTypes = WebInspector.TimelineAgent.RecordType;
        var parentRecord;
        if (record.type === recordTypes.ResourceReceiveResponse ||
            record.type === recordTypes.ResourceFinish ||
            record.type === recordTypes.ResourceReceivedData)
            parentRecord = this._sendRequestRecords[record.data["requestId"]];
        else if (record.type === recordTypes.TimerFire)
            parentRecord = this._timerRecords[record.data["timerId"]];
        else if (record.type === recordTypes.ResourceSendRequest)
            parentRecord = this._scheduledResourceRequests[record.data["url"]];
        else if (record.type === recordTypes.FireAnimationFrame)
            parentRecord = this._requestAnimationFrameRecords[record.data["id"]];
        return parentRecord;
    },

    _innerAddRecordToTimeline: function(record, parentRecord)
    {
        var connectedToOldRecord = false;
        var recordTypes = WebInspector.TimelineAgent.RecordType;

        if (record.type === recordTypes.MarkDOMContent || record.type === recordTypes.MarkLoad)
            parentRecord = null; // No bar entry for load events.
        else if (!this._startAtZero &&
                    (parentRecord === this._rootRecord ||
                    record.type === recordTypes.ResourceReceiveResponse ||
                    record.type === recordTypes.ResourceFinish ||
                    record.type === recordTypes.ResourceReceivedData)) {
            var newParentRecord = this._findParentRecord(record);
            if (newParentRecord) {
                parentRecord = newParentRecord;
                connectedToOldRecord = true;
            }
        }

        var children = record.children;
        var scriptDetails;
        if (record.data && record.data["scriptName"]) {
            scriptDetails = {
                scriptName: record.data["scriptName"],
                scriptLine: record.data["scriptLine"]
            }
        };

        if ((record.type === recordTypes.TimerFire || record.type === recordTypes.FireAnimationFrame) && children && children.length) {
            var childRecord = children[0];
            if (childRecord.type === recordTypes.FunctionCall) {
                scriptDetails = {
                    scriptName: childRecord.data["scriptName"],
                    scriptLine: childRecord.data["scriptLine"]
                };
                children = childRecord.children.concat(children.slice(1));
            }
        }

        var formattedRecord = new WebInspector.TimelinePanel.FormattedRecord(record, parentRecord, this, scriptDetails);

        if (record.type === recordTypes.MarkDOMContent || record.type === recordTypes.MarkLoad) {
            this._timeStampRecords.push(formattedRecord);
            return;
        }

        ++this._rootRecord._allRecordsCount;
        formattedRecord.collapsed = (parentRecord === this._rootRecord);

        var childrenCount = children ? children.length : 0;
        for (var i = 0; i < childrenCount; ++i)
            this._innerAddRecordToTimeline(children[i], formattedRecord);

        formattedRecord._calculateAggregatedStats(this._presentationModel.categories);

        if (connectedToOldRecord) {
            record = formattedRecord;
            do {
                var parent = record.parent;
                if (parent._lastChildEndTime < record._lastChildEndTime)
                    parent._lastChildEndTime = record._lastChildEndTime;
                for (var category in formattedRecord._aggregatedStats)
                    parent._aggregatedStats[category] += formattedRecord._aggregatedStats[category];
                record = parent;
            } while (record.parent);
        } else {
            if (parentRecord !== this._rootRecord)
                parentRecord._selfTime -= formattedRecord.endTime - formattedRecord.startTime;
        }
        // Keep bar entry for mark timeline since nesting might be interesting to the user.
        if (record.type === recordTypes.TimeStamp)
            this._timeStampRecords.push(formattedRecord);
    },

    sidebarResized: function(event)
    {
        var width = event.data;
        this._sidebarBackgroundElement.style.width = width + "px";
        this._scheduleRefresh(false);
        this._overviewPane.sidebarResized(width);
        // Min width = <number of buttons on the left> * 31
        this.statusBarFilters.style.left = Math.max((this.statusBarItems.length + 2) * 31, width) + "px";

        if (this._memoryStatistics)
            this._memoryStatistics.setSidebarWidth(width);
    },

    onResize: function()
    {
        this._closeRecordDetails();
        this._scheduleRefresh(false);
    },

    _createRootRecord: function()
    {
        var rootRecord = {};
        rootRecord.children = [];
        rootRecord._visibleRecordsCount = 0;
        rootRecord._allRecordsCount = 0;
        rootRecord._aggregatedStats = {};
        return rootRecord;
    },

    _clearPanel: function()
    {
        this._model._reset();
    },

    _onRecordsCleared: function()
    {
        this._resetPanel();
        this._refresh();
    },

    _resetPanel: function()
    {
        this._presentationModel.reset();
        this._timeStampRecords = [];
        this._sendRequestRecords = {};
        this._scheduledResourceRequests = {};
        this._timerRecords = {};
        this._requestAnimationFrameRecords = {};
        this._rootRecord = this._createRootRecord();
        this._boundariesAreValid = false;
        this._overviewPane.reset();
        this._adjustScrollPosition(0);
        this._closeRecordDetails();
        this._linkifier.reset();
    },

    elementsToRestoreScrollPositionsFor: function()
    {
        return [this._containerElement];
    },

    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
        this._refresh();
        WebInspector.drawer.currentPanelCounters = this.recordsCounter;
    },

    willHide: function()
    {
        this._closeRecordDetails();
        WebInspector.drawer.currentPanelCounters = null;
        WebInspector.Panel.prototype.willHide.call(this);
    },

    _onScroll: function(event)
    {
        this._closeRecordDetails();
        var scrollTop = this._containerElement.scrollTop;
        var dividersTop = Math.max(0, scrollTop);
        this._timelineGrid.setScrollAndDividerTop(scrollTop, dividersTop);
        this._scheduleRefresh(true);
    },

    _scheduleRefresh: function(preserveBoundaries)
    {
        this._closeRecordDetails();
        this._boundariesAreValid &= preserveBoundaries;

        if (!this.isShowing())
            return;

        if (preserveBoundaries)
            this._refresh();
        else {
            if (!this._refreshTimeout)
                this._refreshTimeout = setTimeout(this._refresh.bind(this), 100);
        }
    },

    _refresh: function()
    {
        if (this._refreshTimeout) {
            clearTimeout(this._refreshTimeout);
            delete this._refreshTimeout;
        }

        this._overviewPane.update(this._rootRecord.children, this._showShortEvents);

        if (!this._boundariesAreValid)
            this._updateBoundaries();
        this._refreshRecords(!this._boundariesAreValid);
        this._updateRecordsCounter();
        if(!this._boundariesAreValid)
            this._updateEventDividers();
        if (this._memoryStatistics && this._memoryStatistics.visible())
            this._memoryStatistics.refresh();
        this._boundariesAreValid = true;
    },

    _updateBoundaries: function()
    {
        this._calculator.reset();
        this._calculator.windowLeft = this._presentationModel.windowLeft;
        this._calculator.windowRight = this._presentationModel.windowRight;

        for (var i = 0; i < this._rootRecord.children.length; ++i)
            this._calculator.updateBoundaries(this._rootRecord.children[i]);

        this._calculator.calculateWindow();
    },

    _filterRecords: function()
    {
        var recordsInWindow = [];
        var filter = this._startAtZero ? new WebInspector.TimelineStartAtZeroRecordFilter(this._presentationModel, this._rootRecord, this._showShortEvents)
            : new WebInspector.TimelineRecordFilter(this._calculator, this._showShortEvents);
        this._rootRecord._visibleRecordsCount = 0;

        var stack = [{children: this._rootRecord.children, index: 0, parentIsCollapsed: false}];
        while (stack.length) {
            var entry = stack[stack.length - 1];
            var records = entry.children;
            if (records && entry.index < records.length) {
                 var record = records[entry.index];
                 ++entry.index;

                 if (filter.accept(record)) {
                     ++this._rootRecord._visibleRecordsCount;
                     ++record.parent._invisibleChildrenCount;
                     if (!entry.parentIsCollapsed)
                         recordsInWindow.push(record);
                 }

                 record._invisibleChildrenCount = 0;

                 stack.push({children: record.children,
                             index: 0,
                             parentIsCollapsed: (entry.parentIsCollapsed || record.collapsed),
                             parentRecord: record,
                             windowLengthBeforeChildrenTraversal: recordsInWindow.length});
            } else {
                stack.pop();
                if (entry.parentRecord)
                    entry.parentRecord._visibleChildrenCount = recordsInWindow.length - entry.windowLengthBeforeChildrenTraversal;
            }
        }

        return recordsInWindow;
    },

    _refreshRecords: function(updateBoundaries)
    {
        var recordsInWindow = this._filterRecords();

        // Calculate the visible area.
        this._scrollTop = this._containerElement.scrollTop;
        var visibleTop = this._scrollTop;
        var visibleBottom = visibleTop + this._containerElement.clientHeight;

        const rowHeight = WebInspector.TimelinePanel.rowHeight;

        // Convert visible area to visible indexes. Always include top-level record for a visible nested record.
        var startIndex = Math.max(0, Math.min(Math.floor(visibleTop / rowHeight) - 1, recordsInWindow.length - 1));
        var endIndex = Math.min(recordsInWindow.length, Math.ceil(visibleBottom / rowHeight));

        // Resize gaps first.
        const top = (startIndex * rowHeight) + "px";
        this._topGapElement.style.height = top;
        this.sidebarElement.style.top = top;
        this.splitView.sidebarResizerElement.style.top = top;
        this._bottomGapElement.style.height = (recordsInWindow.length - endIndex) * rowHeight + "px";

        // Update visible rows.
        var listRowElement = this._sidebarListElement.firstChild;
        var width = this._graphRowsElement.offsetWidth;
        this._itemsGraphsElement.removeChild(this._graphRowsElement);
        var graphRowElement = this._graphRowsElement.firstChild;
        var scheduleRefreshCallback = this._scheduleRefresh.bind(this, true);
        this._itemsGraphsElement.removeChild(this._expandElements);
        this._expandElements.removeChildren();

        for (var i = 0; i < endIndex; ++i) {
            var record = recordsInWindow[i];
            var isEven = !(i % 2);

            if (i < startIndex) {
                var lastChildIndex = i + record._visibleChildrenCount;
                if (lastChildIndex >= startIndex && lastChildIndex < endIndex) {
                    var expandElement = new WebInspector.TimelineExpandableElement(this._expandElements);
                    expandElement._update(record, i, this._calculator.computeBarGraphWindowPosition(record, width - this._expandOffset));
                }
            } else {
                if (!listRowElement) {
                    listRowElement = new WebInspector.TimelineRecordListRow().element;
                    this._sidebarListElement.appendChild(listRowElement);
                }
                if (!graphRowElement) {
                    graphRowElement = new WebInspector.TimelineRecordGraphRow(this._itemsGraphsElement, scheduleRefreshCallback).element;
                    this._graphRowsElement.appendChild(graphRowElement);
                }

                listRowElement.row.update(record, isEven, this._calculator, visibleTop);
                graphRowElement.row.update(record, isEven, this._calculator, width, this._expandOffset, i);

                listRowElement = listRowElement.nextSibling;
                graphRowElement = graphRowElement.nextSibling;
            }
        }

        // Remove extra rows.
        while (listRowElement) {
            var nextElement = listRowElement.nextSibling;
            listRowElement.row.dispose();
            listRowElement = nextElement;
        }
        while (graphRowElement) {
            var nextElement = graphRowElement.nextSibling;
            graphRowElement.row.dispose();
            graphRowElement = nextElement;
        }

        this._itemsGraphsElement.insertBefore(this._graphRowsElement, this._bottomGapElement);
        this._itemsGraphsElement.appendChild(this._expandElements);
        this.splitView.sidebarResizerElement.style.height = this.sidebarElement.clientHeight + "px";
        // Reserve some room for expand / collapse controls to the left for records that start at 0ms.
        var timelinePaddingLeft = this._calculator.windowLeft === 0 ? this._expandOffset : 0;
        if (updateBoundaries)
            this._timelineGrid.updateDividers(true, this._calculator, timelinePaddingLeft);
        this._adjustScrollPosition((recordsInWindow.length + 1) * rowHeight);
    },

    _adjustScrollPosition: function(totalHeight)
    {
        // Prevent the container from being scrolled off the end.
        if ((this._containerElement.scrollTop + this._containerElement.offsetHeight) > totalHeight + 1)
            this._containerElement.scrollTop = (totalHeight - this._containerElement.offsetHeight);
    },

    _getPopoverAnchor: function(element)
    {
        return element.enclosingNodeOrSelfWithClass("timeline-graph-bar") || element.enclosingNodeOrSelfWithClass("timeline-tree-item");
    },

    _mouseOut: function(e)
    {
        this._hideRectHighlight();
    },

    _mouseMove: function(e)
    {
        var anchor = this._getPopoverAnchor(e.target);

        if (anchor && anchor.row._record.type === "Paint")
            this._highlightRect(anchor.row._record);
        else
            this._hideRectHighlight();
    },

    _highlightRect: function(record)
    {
        if (this._highlightedRect === record.data)
            return;
        this._highlightedRect = record.data;
        DOMAgent.highlightRect(this._highlightedRect.x, this._highlightedRect.y, this._highlightedRect.width, this._highlightedRect.height, WebInspector.Color.PageHighlight.Content.toProtocolRGBA(), WebInspector.Color.PageHighlight.ContentOutline.toProtocolRGBA());
    },

    _hideRectHighlight: function()
    {
        if (this._highlightedRect) {
            delete this._highlightedRect;
            DOMAgent.hideHighlight();
        }
    },

    _showPopover: function(anchor, popover)
    {
        var record = anchor.row._record;
        popover.show(record._generatePopupContent(this._calculator, this._presentationModel.categories), anchor);
    },

    _closeRecordDetails: function()
    {
        this._popoverHelper.hidePopover();
    }
}

WebInspector.TimelinePanel.prototype.__proto__ = WebInspector.Panel.prototype;

/**
 * @constructor
 */
WebInspector.TimelineCategory = function(name, title, color)
{
    this.name = name;
    this.title = title;
    this.color = color;
}

/**
 * @constructor
 */
WebInspector.TimelineCalculator = function()
{
    this.reset();
    this.windowLeft = 0.0;
    this.windowRight = 1.0;
}

WebInspector.TimelineCalculator.prototype = {
    computeBarGraphPercentages: function(record)
    {
        var start = (record.startTime - this.minimumBoundary) / this.boundarySpan * 100;
        var end = (record.startTime + record._selfTime - this.minimumBoundary) / this.boundarySpan * 100;
        var endWithChildren = (record._lastChildEndTime - this.minimumBoundary) / this.boundarySpan * 100;
        return {start: start, end: end, endWithChildren: endWithChildren};
    },

    computeBarGraphWindowPosition: function(record, clientWidth)
    {
        const minWidth = 5;
        const borderWidth = 4;
        var workingArea = clientWidth - minWidth - borderWidth;
        var percentages = this.computeBarGraphPercentages(record);

        var left = percentages.start / 100 * workingArea;
        var width = (percentages.end - percentages.start) / 100 * workingArea + minWidth;
        var widthWithChildren =  (percentages.endWithChildren - percentages.start) / 100 * workingArea;
        if (percentages.endWithChildren > percentages.end)
            widthWithChildren += borderWidth + minWidth;
        return {left: left, width: width, widthWithChildren: widthWithChildren};
    },

    calculateWindow: function()
    {
        this.minimumBoundary = this._absoluteMinimumBoundary + this.windowLeft * (this._absoluteMaximumBoundary - this._absoluteMinimumBoundary);
        this.maximumBoundary = this._absoluteMinimumBoundary + this.windowRight * (this._absoluteMaximumBoundary - this._absoluteMinimumBoundary);
        this.boundarySpan = this.maximumBoundary - this.minimumBoundary;
    },

    reset: function()
    {
        this._absoluteMinimumBoundary = -1;
        this._absoluteMaximumBoundary = -1;
    },

    updateBoundaries: function(record)
    {
        var lowerBound = record.startTime;
        if (this._absoluteMinimumBoundary === -1 || lowerBound < this._absoluteMinimumBoundary)
            this._absoluteMinimumBoundary = lowerBound;

        const minimumTimeFrame = 0.1;
        const minimumDeltaForZeroSizeEvents = 0.01;
        var upperBound = Math.max(record._lastChildEndTime + minimumDeltaForZeroSizeEvents, lowerBound + minimumTimeFrame);
        if (this._absoluteMaximumBoundary === -1 || upperBound > this._absoluteMaximumBoundary)
            this._absoluteMaximumBoundary = upperBound;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value + this.minimumBoundary - this._absoluteMinimumBoundary);
    }
}

/**
 * @constructor
 * @extends WebInspector.TimelineCalculator
 */
WebInspector.TimelineStartAtZeroCalculator = function()
{
    this.reset();
    this.windowLeft = 0.0;
    this.windowRight = 1.0;
}

WebInspector.TimelineStartAtZeroCalculator.prototype = {
    computeBarGraphPercentages: function(record)
    {
        var scale = 100 / this.maximumBoundary;
        return {
            start: record._initiatorOffset * scale,
            end: (record._initiatorOffset + record.endTime - record.startTime) * scale,
            endWithChildren: (record._initiatorOffset + record._lastChildEndTime - record.startTime) * scale
        };
    },

    computeBarGraphWindowPosition: function(record, clientWidth)
    {
        const minWidth = 5;
        const borderWidth = 4;
        var workingArea = clientWidth - minWidth - borderWidth;
        var percentages = this.computeBarGraphPercentages(record);
        var left = percentages.start / 100 * workingArea;
        var width = (percentages.end - percentages.start) / 100 * workingArea + minWidth;
        var widthWithChildren =  (percentages.endWithChildren - percentages.start) / 100 * workingArea;
        if (percentages.endWithChildren > percentages.end)
            widthWithChildren += borderWidth + minWidth;

        return {left: left, width: width, widthWithChildren: widthWithChildren};
    },

    calculateWindow: function()
    {
        this.minimumBoundary = this._absoluteMinimumBoundary;
        this.maximumBoundary = this._absoluteMaximumBoundary * 1.05;
        this.boundarySpan = this.maximumBoundary >= 0 ? this.maximumBoundary : 0;
    },

    reset: function()
    {
        this._absoluteMinimumBoundary = -1;
        this._absoluteMaximumBoundary = -1;
    },

    updateBoundaries: function(record)
    {
        var lowerBound = record.startTime;
        if (this._absoluteMinimumBoundary === -1 || lowerBound < this._absoluteMinimumBoundary)
            this._absoluteMinimumBoundary = lowerBound;

        const minimumTimeFrame = 0.001;
        var upperBound = Math.max(record.endTime - record.startTime, minimumTimeFrame);
        if (this._absoluteMaximumBoundary === -1 || upperBound > this._absoluteMaximumBoundary)
            this._absoluteMaximumBoundary = upperBound;
    },

    formatValue: function(value)
    {
        return Number.secondsToString(value, true);
    }
};

/**
 * @constructor
 */
WebInspector.TimelineRecordListRow = function()
{
    this.element = document.createElement("div");
    this.element.row = this;
    this.element.style.cursor = "pointer";
    var iconElement = document.createElement("span");
    iconElement.className = "timeline-tree-icon";
    this.element.appendChild(iconElement);

    this._typeElement = document.createElement("span");
    this._typeElement.className = "type";
    this.element.appendChild(this._typeElement);

    var separatorElement = document.createElement("span");
    separatorElement.className = "separator";
    separatorElement.textContent = " ";

    this._dataElement = document.createElement("span");
    this._dataElement.className = "data dimmed";

    this.element.appendChild(separatorElement);
    this.element.appendChild(this._dataElement);
}

WebInspector.TimelineRecordListRow.prototype = {
    update: function(record, isEven, calculator, offset)
    {
        this._record = record;
        this._calculator = calculator;
        this._offset = offset;

        this.element.className = "timeline-tree-item timeline-category-" + record.category.name + (isEven ? " even" : "");
        this._typeElement.textContent = record.title;

        if (this._dataElement.firstChild)
            this._dataElement.removeChildren();
        if (record.details) {
            var detailsContainer = document.createElement("span");
            if (typeof record.details === "object") {
                detailsContainer.appendChild(document.createTextNode("("));
                detailsContainer.appendChild(record.details);
                detailsContainer.appendChild(document.createTextNode(")"));
            } else
                detailsContainer.textContent = "(" + record.details + ")";
            this._dataElement.appendChild(detailsContainer);
        }
    },

    dispose: function()
    {
        this.element.parentElement.removeChild(this.element);
    }
}

/**
 * @constructor
 */
WebInspector.TimelineRecordGraphRow = function(graphContainer, scheduleRefresh)
{
    this.element = document.createElement("div");
    this.element.row = this;

    this._barAreaElement = document.createElement("div");
    this._barAreaElement.className = "timeline-graph-bar-area";
    this.element.appendChild(this._barAreaElement);

    this._barWithChildrenElement = document.createElement("div");
    this._barWithChildrenElement.className = "timeline-graph-bar with-children";
    this._barWithChildrenElement.row = this;
    this._barAreaElement.appendChild(this._barWithChildrenElement);

    this._barElement = document.createElement("div");
    this._barElement.className = "timeline-graph-bar";
    this._barElement.row = this;
    this._barAreaElement.appendChild(this._barElement);

    this._expandElement = new WebInspector.TimelineExpandableElement(graphContainer);
    this._expandElement._element.addEventListener("click", this._onClick.bind(this));

    this._scheduleRefresh = scheduleRefresh;
}

WebInspector.TimelineRecordGraphRow.prototype = {
    update: function(record, isEven, calculator, clientWidth, expandOffset, index)
    {
        this._record = record;
        this.element.className = "timeline-graph-side timeline-category-" + record.category.name + (isEven ? " even" : "");
        var barPosition = calculator.computeBarGraphWindowPosition(record, clientWidth - expandOffset);
        this._barWithChildrenElement.style.left = barPosition.left + expandOffset + "px";
        this._barWithChildrenElement.style.width = barPosition.widthWithChildren + "px";
        this._barElement.style.left = barPosition.left + expandOffset + "px";
        this._barElement.style.width =  barPosition.width + "px";
        this._expandElement._update(record, index, barPosition);
    },

    _onClick: function(event)
    {
        this._record.collapsed = !this._record.collapsed;
        this._scheduleRefresh(false);
    },

    dispose: function()
    {
        this.element.parentElement.removeChild(this.element);
        this._expandElement._dispose();
    }
}

/**
 * @constructor
 */
WebInspector.TimelinePanel.FormattedRecord = function(record, parentRecord, panel, scriptDetails)
{
    this._panel = panel;
    var recordTypes = WebInspector.TimelineAgent.RecordType;
    var style = panel._recordStyles[record.type];
    this.parent = parentRecord;
    if (parentRecord)
        parentRecord.children.push(this);
    this.category = style.category;
    this.title = style.title;
    this.startTime = record.startTime / 1000;
    this.data = record.data;
    this.type = record.type;
    this.endTime = (typeof record.endTime !== "undefined") ? record.endTime / 1000 : this.startTime;
    this._selfTime = this.endTime - this.startTime;
    this._lastChildEndTime = this.endTime;
    this._initiatorOffset = (parentRecord && parentRecord !== panel._rootRecord) ?
        parentRecord._initiatorOffset + this.startTime - parentRecord.startTime : 0;

    if (record.stackTrace && record.stackTrace.length)
        this.stackTrace = record.stackTrace;
    this.totalHeapSize = record.totalHeapSize;
    this.usedHeapSize = record.usedHeapSize;
    if (record.data && record.data["url"])
        this.url = record.data["url"];
    if (scriptDetails) {
        this.scriptName = scriptDetails.scriptName;
        this.scriptLine = scriptDetails.scriptLine;
    }
    // Make resource receive record last since request was sent; make finish record last since response received.
    if (record.type === recordTypes.ResourceSendRequest) {
        panel._sendRequestRecords[record.data["requestId"]] = this;
    } else if (record.type === recordTypes.ScheduleResourceRequest) {
        panel._scheduledResourceRequests[record.data["url"]] = this;
    } else if (record.type === recordTypes.ResourceReceiveResponse) {
        var sendRequestRecord = panel._sendRequestRecords[record.data["requestId"]];
        if (sendRequestRecord) { // False if we started instrumentation in the middle of request.
            this.url = sendRequestRecord.url;
            // Now that we have resource in the collection, recalculate details in order to display short url.
            sendRequestRecord._refreshDetails();
            if (sendRequestRecord.parent !== panel._rootRecord && sendRequestRecord.parent.type === recordTypes.ScheduleResourceRequest)
                sendRequestRecord.parent._refreshDetails();
        }
    } else if (record.type === recordTypes.ResourceReceivedData || record.type === recordTypes.ResourceFinish) {
        var sendRequestRecord = panel._sendRequestRecords[record.data["requestId"]];
        if (sendRequestRecord) // False for main resource.
            this.url = sendRequestRecord.url;
    } else if (record.type === recordTypes.TimerInstall) {
        this.timeout = record.data["timeout"];
        this.singleShot = record.data["singleShot"];
        panel._timerRecords[record.data["timerId"]] = this;
    } else if (record.type === recordTypes.TimerFire) {
        var timerInstalledRecord = panel._timerRecords[record.data["timerId"]];
        if (timerInstalledRecord) {
            this.callSiteStackTrace = timerInstalledRecord.stackTrace;
            this.timeout = timerInstalledRecord.timeout;
            this.singleShot = timerInstalledRecord.singleShot;
        }
    } else if (record.type === recordTypes.RequestAnimationFrame) {
        panel._requestAnimationFrameRecords[record.data["id"]] = this;
    } else if (record.type === recordTypes.FireAnimationFrame) {
        var requestAnimationRecord = panel._requestAnimationFrameRecords[record.data["id"]];
        if (requestAnimationRecord)
            this.callSiteStackTrace = requestAnimationRecord.stackTrace;
    }
    this._refreshDetails();
}

WebInspector.TimelinePanel.FormattedRecord.prototype = {
    isLong: function()
    {
        return (this._lastChildEndTime - this.startTime) > WebInspector.TimelinePanel.shortRecordThreshold;
    },

    get children()
    {
        if (!this._children)
            this._children = [];
        return this._children;
    },

    _generateAggregatedInfo: function()
    {
        var cell = document.createElement("span");
        cell.className = "timeline-aggregated-info";
        for (var index in this._aggregatedStats) {
            var label = document.createElement("div");
            label.className = "timeline-aggregated-category timeline-" + index;
            cell.appendChild(label);
            var text = document.createElement("span");
            text.textContent = Number.secondsToString(this._aggregatedStats[index], true);
            cell.appendChild(text);
        }
        return cell;
    },

    _generatePopupContent: function(calculator, categories)
    {
        var contentHelper = new WebInspector.TimelinePanel.PopupContentHelper(this.title, this._panel);

        if (this._children && this._children.length) {
            contentHelper._appendTextRow(WebInspector.UIString("Self Time"), Number.secondsToString(this._selfTime, true));
            contentHelper._appendElementRow(WebInspector.UIString("Aggregated Time"), this._generateAggregatedInfo());
        }
        var text = WebInspector.UIString("%s (at %s)", Number.secondsToString(this._lastChildEndTime - this.startTime, true),
            calculator.formatValue(this.startTime - calculator.minimumBoundary));
        contentHelper._appendTextRow(WebInspector.UIString("Duration"), text);

        const recordTypes = WebInspector.TimelineAgent.RecordType;

        switch (this.type) {
            case recordTypes.GCEvent:
                contentHelper._appendTextRow(WebInspector.UIString("Collected"), Number.bytesToString(this.data["usedHeapSizeDelta"]));
                break;
            case recordTypes.TimerInstall:
            case recordTypes.TimerFire:
            case recordTypes.TimerRemove:
                contentHelper._appendTextRow(WebInspector.UIString("Timer ID"), this.data["timerId"]);
                if (typeof this.timeout === "number") {
                    contentHelper._appendTextRow(WebInspector.UIString("Timeout"), Number.secondsToString(this.timeout / 1000));
                    contentHelper._appendTextRow(WebInspector.UIString("Repeats"), !this.singleShot);
                }
                break;
            case recordTypes.FireAnimationFrame:
                contentHelper._appendTextRow(WebInspector.UIString("Callback ID"), this.data["id"]);
                break;
            case recordTypes.FunctionCall:
                contentHelper._appendLinkRow(WebInspector.UIString("Location"), this.scriptName, this.scriptLine);
                break;
            case recordTypes.ScheduleResourceRequest:
            case recordTypes.ResourceSendRequest:
            case recordTypes.ResourceReceiveResponse:
            case recordTypes.ResourceReceivedData:
            case recordTypes.ResourceFinish:
                contentHelper._appendLinkRow(WebInspector.UIString("Resource"), this.url);
                if (this.data["requestMethod"])
                    contentHelper._appendTextRow(WebInspector.UIString("Request Method"), this.data["requestMethod"]);
                if (typeof this.data["statusCode"] === "number")
                    contentHelper._appendTextRow(WebInspector.UIString("Status Code"), this.data["statusCode"]);
                if (this.data["mimeType"])
                    contentHelper._appendTextRow(WebInspector.UIString("MIME Type"), this.data["mimeType"]);
                break;
            case recordTypes.EvaluateScript:
                if (this.data && this.url)
                    contentHelper._appendLinkRow(WebInspector.UIString("Script"), this.url, this.data["lineNumber"]);
                break;
            case recordTypes.Paint:
                contentHelper._appendTextRow(WebInspector.UIString("Location"), WebInspector.UIString("(%d, %d)", this.data["x"], this.data["y"]));
                contentHelper._appendTextRow(WebInspector.UIString("Dimensions"), WebInspector.UIString("%d × %d", this.data["width"], this.data["height"]));
            case recordTypes.RecalculateStyles: // We don't want to see default details.
                break;
            default:
                if (this.details)
                    contentHelper._appendTextRow(WebInspector.UIString("Details"), this.details);
                break;
        }

        if (this.scriptName && this.type !== recordTypes.FunctionCall)
            contentHelper._appendLinkRow(WebInspector.UIString("Function Call"), this.scriptName, this.scriptLine);

        if (this.usedHeapSize)
            contentHelper._appendTextRow(WebInspector.UIString("Used Heap Size"), WebInspector.UIString("%s of %s", Number.bytesToString(this.usedHeapSize), Number.bytesToString(this.totalHeapSize)));

        if (this.callSiteStackTrace && this.callSiteStackTrace.length)
            contentHelper._appendStackTrace(WebInspector.UIString("Call Site stack"), this.callSiteStackTrace);

        if (this.stackTrace)
            contentHelper._appendStackTrace(WebInspector.UIString("Call Stack"), this.stackTrace);

        return contentHelper._contentTable;
    },

    _refreshDetails: function()
    {
        this.details = this._getRecordDetails();
    },

    _getRecordDetails: function()
    {
        switch (this.type) {
            case WebInspector.TimelineAgent.RecordType.GCEvent:
                return WebInspector.UIString("%s collected", Number.bytesToString(this.data["usedHeapSizeDelta"]));
            case WebInspector.TimelineAgent.RecordType.TimerFire:
                return this.scriptName ? this._panel._linkifyLocation(this.scriptName, this.scriptLine, 0) : this.data["timerId"];
            case WebInspector.TimelineAgent.RecordType.FunctionCall:
                return this.scriptName ? this._panel._linkifyLocation(this.scriptName, this.scriptLine, 0) : null;
            case WebInspector.TimelineAgent.RecordType.FireAnimationFrame:
                return this.scriptName ? this._panel._linkifyLocation(this.scriptName, this.scriptLine, 0) : this.data["id"];
            case WebInspector.TimelineAgent.RecordType.EventDispatch:
                return this.data ? this.data["type"] : null;
            case WebInspector.TimelineAgent.RecordType.Paint:
                return this.data["width"] + "\u2009\u00d7\u2009" + this.data["height"];
            case WebInspector.TimelineAgent.RecordType.TimerInstall:
            case WebInspector.TimelineAgent.RecordType.TimerRemove:
                return this.stackTrace ? this._panel._linkifyCallFrame(this.stackTrace[0]) : this.data["timerId"];
            case WebInspector.TimelineAgent.RecordType.RequestAnimationFrame:
            case WebInspector.TimelineAgent.RecordType.CancelAnimationFrame:
                return this.stackTrace ? this._panel._linkifyCallFrame(this.stackTrace[0]) : this.data["id"];
            case WebInspector.TimelineAgent.RecordType.ParseHTML:
            case WebInspector.TimelineAgent.RecordType.RecalculateStyles:
                return this.stackTrace ? this._panel._linkifyCallFrame(this.stackTrace[0]) : null;
            case WebInspector.TimelineAgent.RecordType.EvaluateScript:
                return this.url ? this._panel._linkifyLocation(this.url, this.data["lineNumber"], 0) : null;
            case WebInspector.TimelineAgent.RecordType.XHRReadyStateChange:
            case WebInspector.TimelineAgent.RecordType.XHRLoad:
            case WebInspector.TimelineAgent.RecordType.ScheduleResourceRequest:
            case WebInspector.TimelineAgent.RecordType.ResourceSendRequest:
            case WebInspector.TimelineAgent.RecordType.ResourceReceivedData:
            case WebInspector.TimelineAgent.RecordType.ResourceReceiveResponse:
            case WebInspector.TimelineAgent.RecordType.ResourceFinish:
                return WebInspector.displayNameForURL(this.url);
            case WebInspector.TimelineAgent.RecordType.TimeStamp:
                return this.data["message"];
            default:
                return null;
        }
    },

    _calculateAggregatedStats: function(categories)
    {
        this._aggregatedStats = {};
        for (var category in categories)
            this._aggregatedStats[category] = 0;

        if (this._children) {
            for (var index = this._children.length; index; --index) {
                var child = this._children[index - 1];
                for (var category in categories)
                    this._aggregatedStats[category] += child._aggregatedStats[category];
            }
        }
        this._aggregatedStats[this.category.name] += this._selfTime;
    },

    get aggregatedStats()
    {
        return this._aggregatedStats;
    }
}

/**
 * @constructor
 */
WebInspector.TimelinePanel.PopupContentHelper = function(title, panel)
{
    this._panel = panel;
    this._contentTable = document.createElement("table");;
    var titleCell = this._createCell(WebInspector.UIString("%s - Details", title), "timeline-details-title");
    titleCell.colSpan = 2;
    var titleRow = document.createElement("tr");
    titleRow.appendChild(titleCell);
    this._contentTable.appendChild(titleRow);
}

WebInspector.TimelinePanel.PopupContentHelper.prototype = {
    /**
     * @param {string=} styleName
     */
    _createCell: function(content, styleName)
    {
        var text = document.createElement("label");
        text.appendChild(document.createTextNode(content));
        var cell = document.createElement("td");
        cell.className = "timeline-details";
        if (styleName)
            cell.className += " " + styleName;
        cell.textContent = content;
        return cell;
    },

    _appendTextRow: function(title, content)
    {
        var row = document.createElement("tr");
        row.appendChild(this._createCell(title, "timeline-details-row-title"));
        row.appendChild(this._createCell(content, "timeline-details-row-data"));
        this._contentTable.appendChild(row);
    },

    /**
     * @param {string=} titleStyle
     */
    _appendElementRow: function(title, content, titleStyle)
    {
        var row = document.createElement("tr");
        var titleCell = this._createCell(title, "timeline-details-row-title");
        if (titleStyle)
            titleCell.addStyleClass(titleStyle);
        row.appendChild(titleCell);
        var cell = document.createElement("td");
        cell.className = "timeline-details";
        cell.appendChild(content);
        row.appendChild(cell);
        this._contentTable.appendChild(row);
    },

    /**
     * @param {number=} scriptLine
     */
    _appendLinkRow: function(title, scriptName, scriptLine)
    {
        var link = this._panel._linkifyLocation(scriptName, scriptLine, 0, "timeline-details");
        this._appendElementRow(title, link);
    },

    _appendStackTrace: function(title, stackTrace)
    {
        this._appendTextRow("", "");
        var framesTable = document.createElement("table");
        for (var i = 0; i < stackTrace.length; ++i) {
            var stackFrame = stackTrace[i];
            var row = document.createElement("tr");
            row.className = "timeline-details";
            row.appendChild(this._createCell(stackFrame.functionName ? stackFrame.functionName : WebInspector.UIString("(anonymous function)"), "timeline-function-name"));
            row.appendChild(this._createCell(" @ "));
            var linkCell = document.createElement("td");
            var urlElement = this._panel._linkifyCallFrame(stackFrame);
            linkCell.appendChild(urlElement);
            row.appendChild(linkCell);
            framesTable.appendChild(row);
        }
        this._appendElementRow(title, framesTable, "timeline-stacktrace-title");
    }
}

/**
 * @constructor
 */
WebInspector.TimelineExpandableElement = function(container)
{
    this._element = document.createElement("div");
    this._element.className = "timeline-expandable";

    var leftBorder = document.createElement("div");
    leftBorder.className = "timeline-expandable-left";
    this._element.appendChild(leftBorder);

    container.appendChild(this._element);
}

WebInspector.TimelineExpandableElement.prototype = {
    _update: function(record, index, barPosition)
    {
        const rowHeight = WebInspector.TimelinePanel.rowHeight;
        if (record._visibleChildrenCount || record._invisibleChildrenCount) {
            this._element.style.top = index * rowHeight + "px";
            this._element.style.left = barPosition.left + "px";
            this._element.style.width = Math.max(12, barPosition.width + 25) + "px";
            if (!record.collapsed) {
                this._element.style.height = (record._visibleChildrenCount + 1) * rowHeight + "px";
                this._element.addStyleClass("timeline-expandable-expanded");
                this._element.removeStyleClass("timeline-expandable-collapsed");
            } else {
                this._element.style.height = rowHeight + "px";
                this._element.addStyleClass("timeline-expandable-collapsed");
                this._element.removeStyleClass("timeline-expandable-expanded");
            }
            this._element.removeStyleClass("hidden");
        } else
            this._element.addStyleClass("hidden");
    },

    _dispose: function()
    {
        this._element.parentElement.removeChild(this._element);
    }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.TimelineModel = function()
{
    this._records = [];
    this._collectionEnabled = false;

    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, this._onRecordAdded, this);
}

WebInspector.TimelineModel.Events = {
    RecordAdded: "RecordAdded",
    RecordsCleared: "RecordsCleared"
}

WebInspector.TimelineModel.prototype = {
    startRecord: function()
    {
        if (this._collectionEnabled)
            return;
        this._reset();
        WebInspector.timelineManager.start(30);
        this._collectionEnabled = true;
    },

    stopRecord: function()
    {
        if (!this._collectionEnabled)
            return;
        WebInspector.timelineManager.stop();
        this._collectionEnabled = false;
    },

    get records()
    {
        return this._records;
    },

    _onRecordAdded: function(event)
    {
        if (this._collectionEnabled)
            this._addRecord(event.data);
    },

    _addRecord: function(record)
    {
        this._records.push(record);
        this.dispatchEventToListeners(WebInspector.TimelineModel.Events.RecordAdded, record);
    },

    _loadNextChunk: function(data, index)
    {
        for (var i = 0; i < 20 && index < data.length; ++i, ++index)
            this._addRecord(data[index]);

        if (index !== data.length)
            setTimeout(this._loadNextChunk.bind(this, data, index), 0);
    },

    _loadFromFile: function(file)
    {
        function onLoad(e)
        {
            var data = JSON.parse(e.target.result);
            this._reset();
            this._loadNextChunk(data, 1);
        }

        function onError(e)
        {
            switch(e.target.error.code) {
            case e.target.error.NOT_FOUND_ERR:
                WebInspector.log(WebInspector.UIString('Timeline.loadFromFile: File "%s" not found.', file.name));
            break;
            case e.target.error.NOT_READABLE_ERR:
                WebInspector.log(WebInspector.UIString('Timeline.loadFromFile: File "%s" is not readable', file.name));
            break;
            case e.target.error.ABORT_ERR:
                break;
            default:
                WebInspector.log(WebInspector.UIString('Timeline.loadFromFile: An error occurred while reading the file "%s"', file.name));
            }
        }

        var reader = new FileReader();
        reader.onload = onLoad.bind(this);
        reader.onerror = onError;
        reader.readAsText(file);
    },

    _saveToFile: function()
    {
        var records = ['[' + JSON.stringify(new String(window.navigator.appVersion))];
        for (var i = 0; i < this._records.length; ++i)
            records.push(JSON.stringify(this._records[i]));

        records[records.length - 1] = records[records.length - 1] + "]";

        var now = new Date();
        var suggestedFileName = "TimelineRawData-" + now.toISO8601Compact() + ".json";
        InspectorFrontendHost.saveAs(suggestedFileName, records.join(",\n"));
    },

    _reset: function()
    {
        this.stopRecord();
        this._records = [];
        this.dispatchEventToListeners(WebInspector.TimelineModel.Events.RecordsCleared);
    }
}

WebInspector.TimelineModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.TimelinePresentationModel = function()
{
    this._categories = {};
    this.reset();
}

WebInspector.TimelinePresentationModel.Events = {
    WindowChanged: "WindowChanged",
    CategoryVisibilityChanged: "CategoryVisibilityChanged"
}

WebInspector.TimelinePresentationModel.prototype = {
    reset: function()
    {
        this.windowLeft = 0.0;
        this.windowRight = 1.0;
        this.windowIndexLeft = 0;
        this.windowIndexRight = null;
    },

    get categories()
    {
        return this._categories;
    },

    /**
     * @param {WebInspector.TimelineCategory} category
     */
    addCategory: function(category)
    {
        this._categories[category.name] = category;
    },

    /**
     * @param {number} left
     * @param {number} right
     */
    setWindowPosition: function(left, right)
    {
        this.windowLeft = left;
        this.windowRight = right;
        this.dispatchEventToListeners(WebInspector.TimelinePresentationModel.Events.WindowChanged);
    },

    /**
     * @param {number} left
     * @param {?number} right
     */
    setWindowIndices: function(left, right)
    {
        this.windowIndexLeft = left;
        this.windowIndexRight = right;
        this.dispatchEventToListeners(WebInspector.TimelinePresentationModel.Events.WindowChanged);
    },

    /**
     * @param {WebInspector.TimelineCategory} category
     * @param {boolean} visible
     */
    setCategoryVisibility: function(category, visible)
    {
        category.hidden = !visible;
        this.dispatchEventToListeners(WebInspector.TimelinePresentationModel.Events.CategoryVisibilityChanged, category);
    }
}

/**
 * @constructor
 * @param {WebInspector.TimelineCalculator} calculator
 * @param {boolean} showShortEvents
 */
WebInspector.TimelineRecordFilter = function(calculator, showShortEvents)
{
    this._calculator = calculator;
    this._showShortEvents = showShortEvents;
}

WebInspector.TimelineRecordFilter.prototype = {
    /**
     * @param {WebInspector.TimelinePanel.FormattedRecord} record
     */
    accept: function(record)
    {
        if (record.category.hidden)
            return false;
        if (!this._showShortEvents && !record.isLong())
            return false;
        var percentages = this._calculator.computeBarGraphPercentages(record);
        return percentages.start <= 100 && percentages.endWithChildren >= 0;
    }
}

/**
 * @constructor
 * @param {WebInspector.TimelinePresentationModel} model
 * @param {Object} rootRecord
 * @param {boolean} showShortEvents
 */
WebInspector.TimelineStartAtZeroRecordFilter = function(model, rootRecord, showShortEvents)
{
    this._windowIndexLeft = model.windowIndexLeft;
    this._windowIndexRight = model.windowIndexRight;
    this._rootRecord = rootRecord;
    this._topLevelRecordIndex = 0;
    this._showShortEvents = showShortEvents;
}

WebInspector.TimelineStartAtZeroRecordFilter.prototype = {
    /**
     * @param {WebInspector.TimelinePanel.FormattedRecord} record
     */
    accept: function(record)
    {
        if (record.category.hidden)
            return false;
        if (record.parent === this._rootRecord)
            ++this._topLevelRecordIndex;
        if (!this._showShortEvents && !record.isLong())
            return false;
        return this._topLevelRecordIndex > this._windowIndexLeft &&
            (typeof this._windowIndexRight !== "number" || this._topLevelRecordIndex <= this._windowIndexRight);
    }
}

WebInspector.TimelinePresentationModel.prototype.__proto__ = WebInspector.Object.prototype;
