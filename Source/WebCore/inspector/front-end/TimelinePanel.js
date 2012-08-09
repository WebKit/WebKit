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

    this._model = new WebInspector.TimelineModel();
    this._presentationModel = new WebInspector.TimelinePresentationModel();

    this._overviewPane = new WebInspector.TimelineOverviewPane(this._model);
    this._overviewPane.addEventListener(WebInspector.TimelineOverviewPane.Events.WindowChanged, this._scheduleRefresh.bind(this, false));
    this._overviewPane.addEventListener(WebInspector.TimelineOverviewPane.Events.ModeChanged, this._overviewModeChanged, this);
    this._overviewPane.show(this.element);

    this.element.addEventListener("contextmenu", this._contextMenu.bind(this), false);
    this.element.tabIndex = 0;

    this._sidebarBackgroundElement = document.createElement("div");
    this._sidebarBackgroundElement.className = "sidebar split-view-sidebar-left timeline-sidebar-background";
    this.element.appendChild(this._sidebarBackgroundElement);

    this.createSplitViewWithSidebarTree();
    this.element.appendChild(this.splitView.sidebarResizerElement);

    this._containerElement = this.splitView.element;
    this._containerElement.id = "timeline-container";
    this._containerElement.addEventListener("scroll", this._onScroll.bind(this), false);

    this._timelineMemorySplitter = this.element.createChild("div");
    this._timelineMemorySplitter.id = "timeline-memory-splitter";
    WebInspector.installDragHandle(this._timelineMemorySplitter, this._startSplitterDragging.bind(this), this._splitterDragging.bind(this), this._endSplitterDragging.bind(this), "ns-resize");
    this._timelineMemorySplitter.addStyleClass("hidden");
    this._memoryStatistics = new WebInspector.MemoryStatistics(this, this._model, this.splitView.preferredSidebarWidth());
    WebInspector.settings.memoryCounterGraphsHeight = WebInspector.settings.createSetting("memoryCounterGraphsHeight", 150);

    var itemsTreeElement = new WebInspector.SidebarSectionTreeElement(WebInspector.UIString("RECORDS"), {}, true);
    this.sidebarTree.appendChild(itemsTreeElement);

    this._sidebarListElement = document.createElement("div");
    this.sidebarElement.appendChild(this._sidebarListElement);

    this._containerContentElement = this.splitView.mainElement;
    this._containerContentElement.id = "resources-container-content";

    this._timelineGrid = new WebInspector.TimelineGrid();
    this._itemsGraphsElement = this._timelineGrid.itemsGraphsElement;
    this._itemsGraphsElement.id = "timeline-graphs";
    this._containerContentElement.appendChild(this._timelineGrid.element);
    this._timelineGrid.gridHeaderElement.id = "timeline-grid-header";
    this._memoryStatistics.setMainTimelineGrid(this._timelineGrid);
    this.element.appendChild(this._timelineGrid.gridHeaderElement);

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

    this._calculator = new WebInspector.TimelineCalculator(this._model);
    var shortRecordThresholdTitle = Number.secondsToString(WebInspector.TimelinePresentationModel.shortRecordThreshold);
    this._showShortRecordsTitleText = WebInspector.UIString("Show the records that are shorter than %s", shortRecordThresholdTitle);
    this._hideShortRecordsTitleText = WebInspector.UIString("Hide the records that are shorter than %s", shortRecordThresholdTitle);
    this._createStatusBarItems();

    this._frameMode = false;
    this._boundariesAreValid = true;
    this._scrollTop = 0;

    this._popoverHelper = new WebInspector.PopoverHelper(this.element, this._getPopoverAnchor.bind(this), this._showPopover.bind(this));
    this.element.addEventListener("mousemove", this._mouseMove.bind(this), false);
    this.element.addEventListener("mouseout", this._mouseOut.bind(this), false);

    // Disable short events filter by default.
    this.toggleFilterButton.toggled = true;
    this._showShortEvents = this.toggleFilterButton.toggled;
    this._overviewPane.setShowShortEvents(this._showShortEvents);

    this._timeStampRecords = [];
    this._expandOffset = 15;

    this._headerLineCount = 1;

    this._mainThreadTasks = [];
    this._mainThreadMonitoringEnabled = false;
    if (WebInspector.experimentsSettings.mainThreadMonitoring.isEnabled())
        this._enableMainThreadMonitoring();

    this._createFileSelector();

    this._model.addEventListener(WebInspector.TimelineModel.Events.RecordAdded, this._onTimelineEventRecorded, this);
    this._model.addEventListener(WebInspector.TimelineModel.Events.RecordsCleared, this._onRecordsCleared, this);

    this._registerShortcuts();

    this._allRecordsCount = 0;

    this._presentationModel.addFilter(this._overviewPane);
    this._presentationModel.addFilter(new WebInspector.TimelineCategoryFilter()); 
    this._presentationModel.addFilter(new WebInspector.TimelineIsLongFilter(this)); 

    this._overviewModeSetting = WebInspector.settings.createSetting("timelineOverviewMode", WebInspector.TimelineOverviewPane.Mode.Events);
}

// Define row height, should be in sync with styles for timeline graphs.
WebInspector.TimelinePanel.rowHeight = 18;

WebInspector.TimelinePanel.prototype = {
    /**
     * @param {Event} event
     * @return {boolean}
     */
    _startSplitterDragging: function(event)
    {
        this._dragOffset = this._timelineMemorySplitter.offsetTop + 2 - event.pageY;
        return true;
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
        this._memoryStatistics.show();
        WebInspector.settings.memoryCounterGraphsHeight.set(this.splitView.element.offsetHeight);
    },

    _setSplitterPosition: function(top)
    {
        const overviewHeight = 90;
        const sectionMinHeight = 100;
        top = Number.constrain(top, overviewHeight + sectionMinHeight, this.element.offsetHeight - sectionMinHeight);

        this.splitView.element.style.height = (top - overviewHeight) + "px";
        this._timelineMemorySplitter.style.top = (top - 2) + "px";
        this._memoryStatistics.setTopPosition(top);
        this._containerElementHeight = this._containerElement.clientHeight;
        this.onResize();
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
        return this._statusBarButtons.select("element").concat([
            this._miscStatusBarItems,
            this.recordsCounter
        ]);
    },

    defaultFocusedElement: function()
    {
        return this.element;
    },

    _createStatusBarItems: function()
    {
        this._statusBarButtons = [];

        this.toggleFilterButton = new WebInspector.StatusBarButton(this._hideShortRecordsTitleText, "timeline-filter-status-bar-item");
        this.toggleFilterButton.addEventListener("click", this._toggleFilterButtonClicked, this);
        this._statusBarButtons.push(this.toggleFilterButton);

        this.toggleTimelineButton = new WebInspector.StatusBarButton(WebInspector.UIString("Record"), "record-profile-status-bar-item");
        this.toggleTimelineButton.addEventListener("click", this._toggleTimelineButtonClicked, this);
        this._statusBarButtons.push(this.toggleTimelineButton);

        this.clearButton = new WebInspector.StatusBarButton(WebInspector.UIString("Clear"), "clear-status-bar-item");
        this.clearButton.addEventListener("click", this._clearPanel, this);
        this._statusBarButtons.push(this.clearButton);

        this.garbageCollectButton = new WebInspector.StatusBarButton(WebInspector.UIString("Collect Garbage"), "garbage-collect-status-bar-item");
        this.garbageCollectButton.addEventListener("click", this._garbageCollectButtonClicked, this);
        this._statusBarButtons.push(this.garbageCollectButton);

        this._glueParentButton = new WebInspector.StatusBarButton(WebInspector.UIString("Glue asynchronous events to causes"), "glue-async-status-bar-item");
        this._glueParentButton.toggled = true;
        this._presentationModel.setGlueRecords(true);
        this._glueParentButton.addEventListener("click", this._glueParentButtonClicked, this);
        this._statusBarButtons.push(this._glueParentButton);

        this._miscStatusBarItems = document.createElement("div");
        this._miscStatusBarItems.className = "status-bar-items";

        this._statusBarFilters = this._miscStatusBarItems.createChild("div");
        var categories = WebInspector.TimelinePresentationModel.categories();
        for (var categoryName in categories) {
            var category = categories[categoryName];
            if (category.overviewStripGroupIndex < 0)
                continue;
            this._statusBarFilters.appendChild(this._createTimelineCategoryStatusBarCheckbox(category, this._onCategoryCheckboxClicked.bind(this, category)));
        }

        this.recordsCounter = document.createElement("span");
        this.recordsCounter.className = "timeline-records-counter";
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
        category.hidden = !event.target.checked;
        this._scheduleRefresh(true);
    },

    /**
     * @param {?WebInspector.ProgressIndicator} indicator
     */
    _setOperationInProgress: function(indicator)
    {
        this._operationInProgress = !!indicator;
        for (var i = 0; i < this._statusBarButtons.length; ++i)
            this._statusBarButtons[i].disabled = this._operationInProgress;
        this._glueParentButton.disabled = this._operationInProgress || !!this._frameController;
        this._miscStatusBarItems.removeChildren();
        this._miscStatusBarItems.appendChild(indicator ? indicator.element : this._statusBarFilters);
    },

    _registerShortcuts: function()
    {
        var shortcut = WebInspector.KeyboardShortcut;
        var modifiers = shortcut.Modifiers;
        var section = WebInspector.shortcutsScreen.section(WebInspector.UIString("Timeline Panel"));

        this._shortcuts[shortcut.makeKey("e", modifiers.CtrlOrMeta)] = this._toggleTimelineButtonClicked.bind(this);
        section.addKey(shortcut.shortcutToString("e", modifiers.CtrlOrMeta), WebInspector.UIString("Start/stop recording"));

        if (InspectorFrontendHost.canSave()) {
            this._shortcuts[shortcut.makeKey("s", modifiers.CtrlOrMeta)] = this._saveToFile.bind(this);
            section.addKey(shortcut.shortcutToString("s", modifiers.CtrlOrMeta), WebInspector.UIString("Save timeline data"));
        }

        this._shortcuts[shortcut.makeKey("o", modifiers.CtrlOrMeta)] = this._fileSelectorElement.click.bind(this._fileSelectorElement);
        section.addKey(shortcut.shortcutToString("o", modifiers.CtrlOrMeta), WebInspector.UIString("Load timeline data"));
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
        if (InspectorFrontendHost.canSave())
            contextMenu.appendItem(WebInspector.UIString("Save Timeline data\u2026"), this._saveToFile.bind(this), this._operationInProgress);
        contextMenu.appendItem(WebInspector.UIString("Load Timeline data\u2026"), this._fileSelectorElement.click.bind(this._fileSelectorElement), this._operationInProgress);
        contextMenu.show(event);
    },

    _saveToFile: function()
    {
        if (this._operationInProgress)
            return;
        this._model.saveToFile();
    },

    _loadFromFile: function()
    {
        if (this._operationInProgress)
            return;
        if (this.toggleTimelineButton.toggled) {
            this.toggleTimelineButton.toggled = false;
            this._model.stopRecord();
        }
        var progressIndicator = new WebInspector.ProgressIndicator();
        progressIndicator.addEventListener(WebInspector.ProgressIndicator.Events.Done, this._setOperationInProgress.bind(this, null));
        this._setOperationInProgress(progressIndicator);
        this._model.loadFromFile(this._fileSelectorElement.files[0], progressIndicator);
        this._createFileSelector();
    },

    _rootRecord: function()
    {
        return this._presentationModel.rootRecord();
    },

    _updateRecordsCounter: function(recordsInWindowCount)
    {
        this.recordsCounter.textContent = WebInspector.UIString("%d of %d captured records are visible", recordsInWindowCount, this._allRecordsCount);
    },

    _updateEventDividers: function()
    {
        this._timelineGrid.removeEventDividers();
        var clientWidth = this._graphRowsElementWidth;
        var dividers = [];

        for (var i = 0; i < this._timeStampRecords.length; ++i) {
            var record = this._timeStampRecords[i];
            var positions = this._calculator.computeBarGraphWindowPosition(record);
            var dividerPosition = Math.round(positions.left);
            if (dividerPosition < 0 || dividerPosition >= clientWidth || dividers[dividerPosition])
                continue;
            var divider = WebInspector.TimelinePresentationModel.createEventDivider(record.type, record.title);
            divider.style.left = dividerPosition + "px";
            dividers[dividerPosition] = divider;
        }
        this._timelineGrid.addEventDividers(dividers);
    },

    _shouldShowFrames: function()
    {
        return this._frameMode && this._presentationModel.frames().length > 0 && this.calculator.boundarySpan < 1.0;
    },

    _updateFrames: function()
    {
        var frames = this._presentationModel.frames();
        var clientWidth = this._graphRowsElementWidth;
        if (this._frameContainer)
            this._frameContainer.removeChildren();
        else {
            const frameContainerBorderWidth = 1;
            this._frameContainer = document.createElement("div");
            this._frameContainer.addStyleClass("fill");
            this._frameContainer.addStyleClass("timeline-frame-container");
            this._frameContainer.style.height = this._headerLineCount * WebInspector.TimelinePanel.rowHeight + frameContainerBorderWidth + "px";
            this._frameContainer.addEventListener("dblclick", this._onFrameDoubleClicked.bind(this), false);
        }

        var dividers = [ this._frameContainer ];

        for (var i = 0; i < frames.length; ++i) {
            var frame = frames[i];
            var frameStart = this._calculator.computePosition(frame.startTime);
            var frameEnd = this._calculator.computePosition(frame.endTime);
            if (frameEnd <= 0 || frameStart >= clientWidth)
                continue;

            var frameStrip = document.createElement("div");
            frameStrip.className = "timeline-frame-strip";
            var actualStart = Math.max(frameStart, 0);
            var width = frameEnd - actualStart;
            frameStrip.style.left = actualStart + "px";
            frameStrip.style.width = width + "px";
            frameStrip._frame = frame;

            const minWidthForFrameInfo = 60;
            if (width > minWidthForFrameInfo)
                frameStrip.textContent = Number.secondsToString(frame.endTime - frame.startTime, true);

            this._frameContainer.appendChild(frameStrip);

            if (actualStart > 0) {
                var frameMarker = WebInspector.TimelinePresentationModel.createEventDivider(WebInspector.TimelineModel.RecordType.BeginFrame);
                frameMarker.style.left = frameStart + "px";
                dividers.push(frameMarker);
            }
        }
        this._timelineGrid.addEventDividers(dividers);
    },

    _onFrameDoubleClicked: function(event)
    {
        var frameBar = event.target.enclosingNodeOrSelfWithClass("timeline-frame-strip");
        if (!frameBar)
            return;
        this._overviewPane.zoomToFrame(frameBar._frame);
    },

    _overviewModeChanged: function(event)
    {
        var mode = event.data;
        var shouldShowMemory = mode === WebInspector.TimelineOverviewPane.Mode.Memory;
        var frameMode = mode === WebInspector.TimelineOverviewPane.Mode.Frames;
        this._overviewModeSetting.set(mode);
        if (frameMode !== this._frameMode) {
            this._frameMode = frameMode;
            this._glueParentButton.disabled = frameMode;
            this._presentationModel.setGlueRecords(this._glueParentButton.toggled && !frameMode);
            this._repopulateRecords();

            if (frameMode) {
                this.element.addStyleClass("timeline-frame-overview");
                this._frameController = new WebInspector.TimelineFrameController(this._model, this._overviewPane, this._presentationModel);
            } else {
                this._frameController.dispose();
                this._frameController = null;
                this.element.removeStyleClass("timeline-frame-overview");
            }
        }
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
            this._setSplitterPosition(WebInspector.settings.memoryCounterGraphsHeight.get());
        }
    },

    _toggleTimelineButtonClicked: function()
    {
        if (this._operationInProgress)
            return;
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
        this._overviewPane.setShowShortEvents(this._showShortEvents);
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
        this._presentationModel.setGlueRecords(this._glueParentButton.toggled);
        this._repopulateRecords();
    },

    _repopulateRecords: function()
    {
        this._resetPanel();
        var records = this._model.records;
        for (var i = 0; i < records.length; ++i)
            this._innerAddRecordToTimeline(records[i], this._rootRecord());
        this._scheduleRefresh(false);
    },

    _onTimelineEventRecorded: function(event)
    {
        if (this._innerAddRecordToTimeline(event.data, this._rootRecord()))
            this._scheduleRefresh(false);
    },

    _innerAddRecordToTimeline: function(record, parentRecord)
    {
        if (record.type === WebInspector.TimelineModel.RecordType.Program) {
            this._mainThreadTasks.push({
                startTime: WebInspector.TimelineModel.startTimeInSeconds(record),
                endTime: WebInspector.TimelineModel.endTimeInSeconds(record)
            });
        }

        var records = this._presentationModel.addRecord(record, parentRecord);
        this._allRecordsCount += records.length;
        var timeStampRecords = this._timeStampRecords;
        var hasVisibleRecords = false;
        var presentationModel = this._presentationModel;
        function processRecord(record)
        {
            if (WebInspector.TimelinePresentationModel.isEventDivider(record))
                timeStampRecords.push(record);
            hasVisibleRecords |= presentationModel.isVisible(record);
        }
        WebInspector.TimelinePresentationModel.forAllRecords(records, processRecord);

        function isAdoptedRecord(record)
        {
            return record.parent !== presentationModel.rootRecord;
        }
        // Tell caller update is necessary either if we added a visible record or if we re-parented a record.
        return hasVisibleRecords || records.some(isAdoptedRecord);
    },

    sidebarResized: function(event)
    {
        var width = event.data;
        this._sidebarBackgroundElement.style.width = width + "px";
        this.onResize();
        this._overviewPane.sidebarResized(width);
        // Min width = <number of buttons on the left> * 31
        this._miscStatusBarItems.style.left = Math.max((this._statusBarButtons.length + 3) * 31, width) + "px";
        this._memoryStatistics.setSidebarWidth(width);
        this._timelineGrid.gridHeaderElement.style.left = width + "px";
    },

    onResize: function()
    {
        this._closeRecordDetails();
        this._scheduleRefresh(false);
        this._graphRowsElementWidth = this._graphRowsElement.offsetWidth;
        this._timelineGrid.gridHeaderElement.style.width = this._itemsGraphsElement.offsetWidth + "px";
        this._containerElementHeight = this._containerElement.clientHeight;
    },

    _clearPanel: function()
    {
        this._model.reset();
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
        this._boundariesAreValid = false;
        this._adjustScrollPosition(0);
        this._closeRecordDetails();
        this._allRecordsCount = 0;
        this._automaticallySizeWindow = true;
        this._mainThreadTasks = [];
    },

    elementsToRestoreScrollPositionsFor: function()
    {
        return [this._containerElement];
    },

    wasShown: function()
    {
        WebInspector.Panel.prototype.wasShown.call(this);
        if (!WebInspector.TimelinePanel._categoryStylesInitialized) {
            WebInspector.TimelinePanel._categoryStylesInitialized = true;
            this._injectCategoryStyles();
        }
        this._overviewPane.setMode(this._overviewModeSetting.get());
        this._refresh();
    },

    willHide: function()
    {
        this._closeRecordDetails();
        WebInspector.Panel.prototype.willHide.call(this);
    },

    _onScroll: function(event)
    {
        this._closeRecordDetails();
        this._scrollTop = this._containerElement.scrollTop;
        var dividersTop = Math.max(0, this._scrollTop);
        this._timelineGrid.setScrollAndDividerTop(this._scrollTop, dividersTop);
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
                this._refreshTimeout = setTimeout(this._refresh.bind(this), 300);
        }
    },

    _refresh: function()
    {
        if (this._refreshTimeout) {
            clearTimeout(this._refreshTimeout);
            delete this._refreshTimeout;
        }

        this._timelinePaddingLeft = !this._overviewPane.windowLeft() ? this._expandOffset : 0;
        this._calculator.setWindow(this._overviewPane.windowStartTime(), this._overviewPane.windowEndTime());
        this._calculator.setDisplayWindow(this._timelinePaddingLeft, this._graphRowsElementWidth);

        var recordsInWindowCount = this._refreshRecords();
        this._updateRecordsCounter(recordsInWindowCount);
        if(!this._boundariesAreValid) {
            this._updateEventDividers();
            if (this._shouldShowFrames()) {
                this._timelineGrid.removeDividers();
                this._updateFrames();
            } else {
                this._timelineGrid.updateDividers(this._calculator);
            }
            if (this._mainThreadMonitoringEnabled)
                this._refreshMainThreadBars();
        }
        if (this._memoryStatistics.visible())
            this._memoryStatistics.refresh();
        this._boundariesAreValid = true;
    },

    revealRecordAt: function(time)
    {
        var recordsInWindow = this._presentationModel.filteredRecords();
        var recordToReveal;
        for (var i = 0; i < recordsInWindow.length; ++i) {
            var record = recordsInWindow[i];
            if (record.containsTime(time)) {
                recordToReveal = record;
                break;
            }
            // If there is no record containing the time than use the latest one before that time.
            if (!recordToReveal || record.endTime < time && recordToReveal.endTime < record.endTime)
                recordToReveal = record;
        }

        // The record ends before the window left bound so scroll to the top.
        if (!recordToReveal) {
            this._containerElement.scrollTop = 0;
            return;
        }

        // Expand all ancestors.
        for (var parent = recordToReveal.parent; parent !== this._rootRecord(); parent = parent.parent)
            parent.collapsed = false;
        var index = recordsInWindow.indexOf(recordToReveal);
        this._containerElement.scrollTop = index * WebInspector.TimelinePanel.rowHeight;
    },

    _refreshRecords: function()
    {
        var recordsInWindow = this._presentationModel.filteredRecords();

        // Calculate the visible area.
        var visibleTop = this._scrollTop;
        var visibleBottom = visibleTop + this._containerElementHeight;

        const rowHeight = WebInspector.TimelinePanel.rowHeight;

        // Convert visible area to visible indexes. Always include top-level record for a visible nested record.
        var startIndex = Math.max(0, Math.min(Math.floor(visibleTop / rowHeight) - this._headerLineCount, recordsInWindow.length - 1));
        var endIndex = Math.min(recordsInWindow.length, Math.ceil(visibleBottom / rowHeight));
        var lastVisibleLine = Math.max(0, Math.floor(visibleBottom / rowHeight) - this._headerLineCount);
        if (this._automaticallySizeWindow && recordsInWindow.length > lastVisibleLine) {
            this._automaticallySizeWindow = false;
            // If we're at the top, always use real timeline start as a left window bound so that expansion arrow padding logic works.
            var windowStartTime = startIndex ? recordsInWindow[startIndex].startTime : this._model.minimumRecordTime();
            this._overviewPane.setWindowTimes(windowStartTime, recordsInWindow[Math.max(0, lastVisibleLine - 1)].endTime);
            recordsInWindow = this._presentationModel.filteredRecords();
            endIndex = Math.min(recordsInWindow.length, lastVisibleLine);
        }

        // Resize gaps first.
        const top = (startIndex * rowHeight) + "px";
        this._topGapElement.style.height = top;
        this.sidebarElement.style.top = top;
        this._bottomGapElement.style.height = (recordsInWindow.length - endIndex) * rowHeight + "px";

        // Update visible rows.
        var listRowElement = this._sidebarListElement.firstChild;
        var width = this._graphRowsElementWidth;
        this._itemsGraphsElement.removeChild(this._graphRowsElement);
        var graphRowElement = this._graphRowsElement.firstChild;
        var scheduleRefreshCallback = this._scheduleRefresh.bind(this, true);
        this._itemsGraphsElement.removeChild(this._expandElements);
        this._expandElements.removeChildren();

        for (var i = 0; i < endIndex; ++i) {
            var record = recordsInWindow[i];
            var isEven = !(i % 2);

            if (i < startIndex) {
                var lastChildIndex = i + record.visibleChildrenCount;
                if (lastChildIndex >= startIndex && lastChildIndex < endIndex) {
                    var expandElement = new WebInspector.TimelineExpandableElement(this._expandElements);
                    var positions = this._calculator.computeBarGraphWindowPosition(record);
                    expandElement._update(record, i, positions.left - this._expandOffset, positions.width);
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

                listRowElement.row.update(record, isEven, visibleTop);
                graphRowElement.row.update(record, isEven, this._calculator, this._expandOffset, i);

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
        this._adjustScrollPosition((recordsInWindow.length + this._headerLineCount) * rowHeight);

        return recordsInWindow.length;
    },

    _refreshMainThreadBars: function()
    {
        const barOffset = 3;
        const minGap = 3;

        var minWidth = WebInspector.TimelineCalculator._minWidth;
        var widthAdjustment = minWidth / 2;

        var width = this._graphRowsElementWidth;
        var boundarySpan = this._overviewPane.windowEndTime() - this._overviewPane.windowStartTime();
        var scale = boundarySpan / (width - minWidth - this._timelinePaddingLeft);
        var startTime = this._overviewPane.windowStartTime() - this._timelinePaddingLeft * scale;
        var endTime = startTime + width * scale;

        var tasks = this._mainThreadTasks;

        function compareEndTime(value, task)
        {
            return value < task.endTime ? -1 : 1;
        }

        var taskIndex = insertionIndexForObjectInListSortedByFunction(startTime, tasks, compareEndTime);

        var container = this._cpuBarsElement;
        var element = container.firstChild.nextSibling;
        var lastElement;
        var lastLeft;
        var lastRight;

        while (taskIndex < tasks.length) {
            var task = tasks[taskIndex];
            if (task.startTime > endTime)
                break;
            taskIndex++;

            var left = Math.max(0, this._calculator.computePosition(task.startTime) + barOffset - widthAdjustment);
            var right = Math.min(width, this._calculator.computePosition(task.endTime) + barOffset + widthAdjustment);

            if (lastElement) {
                var gap = Math.floor(left) - Math.ceil(lastRight);
                if (gap < minGap) {
                    lastRight = right;
                    continue;
                }
                lastElement.style.width = (lastRight - lastLeft) + "px";
            }

            if (!element)
                element = container.createChild("div", "timeline-graph-bar");

            element.style.left = left + "px";
            lastLeft = left;
            lastRight = right;

            lastElement = element;
            element = element.nextSibling;
        }

        if (lastElement)
            lastElement.style.width = (lastRight - lastLeft) + "px";

        while (element) {
            var nextElement = element.nextSibling;
            container.removeChild(element);
            element = nextElement;
        }
    },

    _enableMainThreadMonitoring: function()
    {
        ++this._headerLineCount;

        var container = this._timelineGrid.gridHeaderElement;
        this._cpuBarsElement = container.createChild("div", "timeline-cpu-bars timeline-category-program");
        var cpuBarsLabel = this._cpuBarsElement.createChild("span", "timeline-cpu-bars-label");
        cpuBarsLabel.textContent = WebInspector.UIString("CPU");

        const headerBorderWidth = 1;
        const headerMargin = 2;

        var headerHeight = this._headerLineCount * WebInspector.TimelinePanel.rowHeight;
        this.sidebarElement.firstChild.style.height = headerHeight + "px";
        this._timelineGrid.dividersLabelBarElement.style.height = headerHeight + headerMargin + "px";
        this._itemsGraphsElement.style.top = headerHeight + headerBorderWidth + "px";

        this._mainThreadMonitoringEnabled = true;
    },

    _adjustScrollPosition: function(totalHeight)
    {
        // Prevent the container from being scrolled off the end.
        if ((this._scrollTop + this._containerElementHeight) > totalHeight + 1)
            this._containerElement.scrollTop = (totalHeight - this._containerElement.offsetHeight);
    },

    _getPopoverAnchor: function(element)
    {
        return element.enclosingNodeOrSelfWithClass("timeline-graph-bar") ||
            element.enclosingNodeOrSelfWithClass("timeline-tree-item") ||
            element.enclosingNodeOrSelfWithClass("timeline-frame-strip");
    },

    _mouseOut: function(e)
    {
        this._hideRectHighlight();
    },

    _mouseMove: function(e)
    {
        var anchor = this._getPopoverAnchor(e.target);

        if (anchor && anchor.row && anchor.row._record.type === "Paint")
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

    /**
     * @param {Element} anchor
     * @param {WebInspector.Popover} popover
     */
    _showPopover: function(anchor, popover)
    {
        if (anchor.hasStyleClass("timeline-frame-strip")) {
            var frame = anchor._frame;
            popover.show(WebInspector.TimelinePresentationModel.generatePopupContentForFrame(frame), anchor);
        } else {
            if (anchor.row && anchor.row._record)
                popover.show(anchor.row._record.generatePopupContent(), anchor);
        }
    },

    _closeRecordDetails: function()
    {
        this._popoverHelper.hidePopover();
    },

    _injectCategoryStyles: function()
    {
        var style = document.createElement("style");
        var categories = WebInspector.TimelinePresentationModel.categories();

        style.textContent = Object.values(categories).map(WebInspector.TimelinePresentationModel.createStyleRuleForCategory).join("\n");
        document.head.appendChild(style);
    }
}

WebInspector.TimelinePanel.prototype.__proto__ = WebInspector.Panel.prototype;

/**
 * @constructor
 * @param {WebInspector.TimelineModel} model
 */
WebInspector.TimelineCalculator = function(model)
{
    this._model = model;
}

WebInspector.TimelineCalculator._minWidth = 5;

WebInspector.TimelineCalculator.prototype = {
    /**
     * @param {number} time
     */
    computePosition: function(time)
    {
        return (time - this.minimumBoundary) / this.boundarySpan * this._workingArea + this.paddingLeft;
    },

    computeBarGraphPercentages: function(record)
    {
        var start = (record.startTime - this.minimumBoundary) / this.boundarySpan * 100;
        var end = (record.startTime + record.selfTime - this.minimumBoundary) / this.boundarySpan * 100;
        var endWithChildren = (record.lastChildEndTime - this.minimumBoundary) / this.boundarySpan * 100;
        var cpuWidth = record.cpuTime / this.boundarySpan * 100;
        return {start: start, end: end, endWithChildren: endWithChildren, cpuWidth: cpuWidth};
    },

    computeBarGraphWindowPosition: function(record)
    {
        var percentages = this.computeBarGraphPercentages(record);
        var widthAdjustment = 0;

        var left = this.computePosition(record.startTime);
        var width = (percentages.end - percentages.start) / 100 * this._workingArea;
        if (width < WebInspector.TimelineCalculator._minWidth) {
            widthAdjustment = WebInspector.TimelineCalculator._minWidth - width;
            left -= widthAdjustment / 2;
            width += widthAdjustment;
        }
        var widthWithChildren = (percentages.endWithChildren - percentages.start) / 100 * this._workingArea + widthAdjustment;
        var cpuWidth = percentages.cpuWidth / 100 * this._workingArea + widthAdjustment;
        if (percentages.endWithChildren > percentages.end)
            widthWithChildren += widthAdjustment;
        return {left: left, width: width, widthWithChildren: widthWithChildren, cpuWidth: cpuWidth};
    },

    setWindow: function(minimumBoundary, maximumBoundary)
    {
        this.minimumBoundary = minimumBoundary;
        this.maximumBoundary = maximumBoundary;
        this.boundarySpan = this.maximumBoundary - this.minimumBoundary;
    },

    /**
     * @param {number} paddingLeft
     * @param {number} clientWidth
     */
    setDisplayWindow: function(paddingLeft, clientWidth)
    {
        this._workingArea = clientWidth - WebInspector.TimelineCalculator._minWidth - paddingLeft;
        this.paddingLeft = paddingLeft;
    },

    formatTime: function(value)
    {
        return Number.secondsToString(value + this.minimumBoundary - this._model.minimumRecordTime());
    }
}

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
    update: function(record, isEven, offset)
    {
        this._record = record;
        this._offset = offset;

        this.element.className = "timeline-tree-item timeline-category-" + record.category.name + (isEven ? " even" : "");
        this._typeElement.textContent = record.title;

        if (this._dataElement.firstChild)
            this._dataElement.removeChildren();
        var details = record.details();
        if (details) {
            var detailsContainer = document.createElement("span");
            if (typeof details === "object") {
                detailsContainer.appendChild(document.createTextNode("("));
                detailsContainer.appendChild(details);
                detailsContainer.appendChild(document.createTextNode(")"));
            } else
                detailsContainer.textContent = "(" + details + ")";
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

    this._barCpuElement = document.createElement("div");
    this._barCpuElement.className = "timeline-graph-bar cpu"
    this._barCpuElement.row = this;
    this._barAreaElement.appendChild(this._barCpuElement);

    this._barElement = document.createElement("div");
    this._barElement.className = "timeline-graph-bar";
    this._barElement.row = this;
    this._barAreaElement.appendChild(this._barElement);

    this._expandElement = new WebInspector.TimelineExpandableElement(graphContainer);
    this._expandElement._element.addEventListener("click", this._onClick.bind(this));

    this._scheduleRefresh = scheduleRefresh;
}

WebInspector.TimelineRecordGraphRow.prototype = {
    update: function(record, isEven, calculator, expandOffset, index)
    {
        this._record = record;
        this.element.className = "timeline-graph-side timeline-category-" + record.category.name + (isEven ? " even" : "");
        var barPosition = calculator.computeBarGraphWindowPosition(record);
        this._barWithChildrenElement.style.left = barPosition.left + "px";
        this._barWithChildrenElement.style.width = barPosition.widthWithChildren + "px";
        this._barElement.style.left = barPosition.left + "px";
        this._barElement.style.width = barPosition.width + "px";
        this._barCpuElement.style.left = barPosition.left + "px";
        this._barCpuElement.style.width = barPosition.cpuWidth + "px";
        this._expandElement._update(record, index, barPosition.left - expandOffset, barPosition.width);
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
    _update: function(record, index, left, width)
    {
        const rowHeight = WebInspector.TimelinePanel.rowHeight;
        if (record.visibleChildrenCount || record.invisibleChildrenCount) {
            this._element.style.top = index * rowHeight + "px";
            this._element.style.left = left + "px";
            this._element.style.width = Math.max(12, width + 25) + "px";
            if (!record.collapsed) {
                this._element.style.height = (record.visibleChildrenCount + 1) * rowHeight + "px";
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
 * @implements {WebInspector.TimelinePresentationModel.Filter}
 */
WebInspector.TimelineCategoryFilter = function()
{
}

WebInspector.TimelineCategoryFilter.prototype = {
    /**
     * @param {WebInspector.TimelinePresentationModel.Record} record
     */
    accept: function(record)
    {
        return !record.category.hidden && record.type !== WebInspector.TimelineModel.RecordType.BeginFrame;
    }
}

/**
 * @param {WebInspector.TimelinePanel} panel
 * @constructor
 * @implements {WebInspector.TimelinePresentationModel.Filter}
 */
WebInspector.TimelineIsLongFilter = function(panel)
{
    this._panel = panel;
}

WebInspector.TimelineIsLongFilter.prototype = {
    /**
     * @param {WebInspector.TimelinePresentationModel.Record} record
     */
    accept: function(record)
    {
        return this._panel._showShortEvents || record.isLong();
    }
}
