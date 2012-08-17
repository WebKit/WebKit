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
 * @extends {WebInspector.Object}
 */
WebInspector.TimelinePresentationModel = function()
{
    this._linkifier = new WebInspector.Linkifier();
    this._glueRecords = false;
    this._filters = [];
    this.reset();
}

WebInspector.TimelinePresentationModel.shortRecordThreshold = 0.015;

WebInspector.TimelinePresentationModel.categories = function()
{
    if (WebInspector.TimelinePresentationModel._categories)
        return WebInspector.TimelinePresentationModel._categories;
    WebInspector.TimelinePresentationModel._categories = {
        program: new WebInspector.TimelineCategory("program", WebInspector.UIString("Program"), -1, "#BBBBBB", "#DDDDDD", "#FFFFFF"),
        loading: new WebInspector.TimelineCategory("loading", WebInspector.UIString("Loading"), 0, "#5A8BCC", "#8EB6E9", "#70A2E3"),
        scripting: new WebInspector.TimelineCategory("scripting", WebInspector.UIString("Scripting"), 1, "#D8AA34", "#F3D07A", "#F1C453"),
        rendering: new WebInspector.TimelineCategory("rendering", WebInspector.UIString("Rendering"), 2, "#8266CC", "#AF9AEB", "#9A7EE6"),
        painting: new WebInspector.TimelineCategory("painting", WebInspector.UIString("Painting"), 2, "#5FA050", "#8DC286", "#71B363")
    };
    return WebInspector.TimelinePresentationModel._categories;
};

/**
 * @return {!Object.<string, {title: string, category}>}
 */
WebInspector.TimelinePresentationModel.initRecordStyles_ = function()
{
    if (WebInspector.TimelinePresentationModel._recordStylesMap)
        return WebInspector.TimelinePresentationModel._recordStylesMap;

    var recordTypes = WebInspector.TimelineModel.RecordType;
    var categories = WebInspector.TimelinePresentationModel.categories();

    var recordStyles = {};
    recordStyles[recordTypes.Root] = { title: "#root", category: categories["loading"] };
    recordStyles[recordTypes.Program] = { title: WebInspector.UIString("Program"), category: categories["program"] };
    recordStyles[recordTypes.EventDispatch] = { title: WebInspector.UIString("Event"), category: categories["scripting"] };
    recordStyles[recordTypes.BeginFrame] = { title: WebInspector.UIString("Frame Start"), category: categories["rendering"] };
    recordStyles[recordTypes.Layout] = { title: WebInspector.UIString("Layout"), category: categories["rendering"] };
    recordStyles[recordTypes.RecalculateStyles] = { title: WebInspector.UIString("Recalculate Style"), category: categories["rendering"] };
    recordStyles[recordTypes.Paint] = { title: WebInspector.UIString("Paint"), category: categories["painting"] };
    recordStyles[recordTypes.DecodeImage] = { title: WebInspector.UIString("Image Decode"), category: categories["painting"] };
    recordStyles[recordTypes.ResizeImage] = { title: WebInspector.UIString("Image Resize"), category: categories["painting"] };
    recordStyles[recordTypes.CompositeLayers] = { title: WebInspector.UIString("Composite Layers"), category: categories["painting"] };
    recordStyles[recordTypes.ParseHTML] = { title: WebInspector.UIString("Parse"), category: categories["loading"] };
    recordStyles[recordTypes.TimerInstall] = { title: WebInspector.UIString("Install Timer"), category: categories["scripting"] };
    recordStyles[recordTypes.TimerRemove] = { title: WebInspector.UIString("Remove Timer"), category: categories["scripting"] };
    recordStyles[recordTypes.TimerFire] = { title: WebInspector.UIString("Timer Fired"), category: categories["scripting"] };
    recordStyles[recordTypes.XHRReadyStateChange] = { title: WebInspector.UIString("XHR Ready State Change"), category: categories["scripting"] };
    recordStyles[recordTypes.XHRLoad] = { title: WebInspector.UIString("XHR Load"), category: categories["scripting"] };
    recordStyles[recordTypes.EvaluateScript] = { title: WebInspector.UIString("Evaluate Script"), category: categories["scripting"] };
    recordStyles[recordTypes.ResourceSendRequest] = { title: WebInspector.UIString("Send Request"), category: categories["loading"] };
    recordStyles[recordTypes.ResourceReceiveResponse] = { title: WebInspector.UIString("Receive Response"), category: categories["loading"] };
    recordStyles[recordTypes.ResourceFinish] = { title: WebInspector.UIString("Finish Loading"), category: categories["loading"] };
    recordStyles[recordTypes.FunctionCall] = { title: WebInspector.UIString("Function Call"), category: categories["scripting"] };
    recordStyles[recordTypes.ResourceReceivedData] = { title: WebInspector.UIString("Receive Data"), category: categories["loading"] };
    recordStyles[recordTypes.GCEvent] = { title: WebInspector.UIString("GC Event"), category: categories["scripting"] };
    recordStyles[recordTypes.MarkDOMContent] = { title: WebInspector.UIString("DOMContent event"), category: categories["scripting"] };
    recordStyles[recordTypes.MarkLoad] = { title: WebInspector.UIString("Load event"), category: categories["scripting"] };
    recordStyles[recordTypes.TimeStamp] = { title: WebInspector.UIString("Stamp"), category: categories["scripting"] };
    recordStyles[recordTypes.Time] = { title: WebInspector.UIString("Time"), category: categories["scripting"] };
    recordStyles[recordTypes.TimeEnd] = { title: WebInspector.UIString("Time End"), category: categories["scripting"] };
    recordStyles[recordTypes.ScheduleResourceRequest] = { title: WebInspector.UIString("Schedule Request"), category: categories["loading"] };
    recordStyles[recordTypes.RequestAnimationFrame] = { title: WebInspector.UIString("Request Animation Frame"), category: categories["scripting"] };
    recordStyles[recordTypes.CancelAnimationFrame] = { title: WebInspector.UIString("Cancel Animation Frame"), category: categories["scripting"] };
    recordStyles[recordTypes.FireAnimationFrame] = { title: WebInspector.UIString("Animation Frame Fired"), category: categories["scripting"] };

    WebInspector.TimelinePresentationModel._recordStylesMap = recordStyles;
    return recordStyles;
}

/**
 * @param {Object} record
 */
WebInspector.TimelinePresentationModel.recordStyle = function(record)
{
    var recordStyles = WebInspector.TimelinePresentationModel.initRecordStyles_();
    var result = recordStyles[record.type];
    if (!result) {
        result = {
            title: WebInspector.UIString("Unknown: %s", record.type),
            category: WebInspector.TimelinePresentationModel.categories()["program"]
        };
        recordStyles[record.type] = result;
    }
    return result;
}

WebInspector.TimelinePresentationModel.categoryForRecord = function(record)
{
    return WebInspector.TimelinePresentationModel.recordStyle(record).category;
}

WebInspector.TimelinePresentationModel.isEventDivider = function(record)
{
    var recordTypes = WebInspector.TimelineModel.RecordType;
    if (record.type === recordTypes.TimeStamp)
        return true;
    if (record.type === recordTypes.MarkDOMContent || record.type === recordTypes.MarkLoad) {
        var mainFrame = WebInspector.resourceTreeModel.mainFrame;
        if (mainFrame && mainFrame.id === record.frameId)
            return true;
    }
    return false;
}

/**
 * @param {Array} recordsArray
 * @param {?function(*)} preOrderCallback
 * @param {function(*)=} postOrderCallback
 */
WebInspector.TimelinePresentationModel.forAllRecords = function(recordsArray, preOrderCallback, postOrderCallback)
{
    if (!recordsArray)
        return;
    var stack = [{array: recordsArray, index: 0}];
    while (stack.length) {
        var entry = stack[stack.length - 1];
        var records = entry.array;
        if (entry.index < records.length) {
             var record = records[entry.index];
             if (preOrderCallback && preOrderCallback(record))
                 return;
             if (record.children)
                 stack.push({array: record.children, index: 0, record: record});
             else if (postOrderCallback && postOrderCallback(record))
                return;
             ++entry.index;
        } else {
            if (entry.record && postOrderCallback && postOrderCallback(entry.record))
                return;
            stack.pop();
        }
    }
}

/**
 * @param {string=} recordType
 * @return {boolean}
 */
WebInspector.TimelinePresentationModel.needsPreviewElement = function(recordType)
{
    if (!recordType)
        return false;
    const recordTypes = WebInspector.TimelineModel.RecordType;
    switch (recordType) {
    case recordTypes.ScheduleResourceRequest:
    case recordTypes.ResourceSendRequest:
    case recordTypes.ResourceReceiveResponse:
    case recordTypes.ResourceReceivedData:
    case recordTypes.ResourceFinish:
        return true;
    default:
        return false;
    }
}

/**
 * @param {string} recordType
 * @param {string=} title
 */
WebInspector.TimelinePresentationModel.createEventDivider = function(recordType, title)
{
    var eventDivider = document.createElement("div");
    eventDivider.className = "resources-event-divider";
    var recordTypes = WebInspector.TimelineModel.RecordType;

    if (recordType === recordTypes.MarkDOMContent)
        eventDivider.className += " resources-blue-divider";
    else if (recordType === recordTypes.MarkLoad)
        eventDivider.className += " resources-red-divider";
    else if (recordType === recordTypes.TimeStamp)
        eventDivider.className += " resources-orange-divider";
    else if (recordType === recordTypes.BeginFrame)
        eventDivider.className += " timeline-frame-divider";

    if (title)
        eventDivider.title = title;

    return eventDivider;
}

WebInspector.TimelinePresentationModel.prototype = {
    /**
     * @param {WebInspector.TimelinePresentationModel.Filter} filter
     */
    addFilter: function(filter)
    {
        this._filters.push(filter);
    },

    rootRecord: function()
    {
        return this._rootRecord;
    },

    frames: function()
    {
        return this._frames;
    },

    reset: function()
    {
        this._linkifier.reset();
        this._rootRecord = new WebInspector.TimelinePresentationModel.Record(this, { type: WebInspector.TimelineModel.RecordType.Root }, null, null);
        this._sendRequestRecords = {};
        this._scheduledResourceRequests = {};
        this._timerRecords = {};
        this._requestAnimationFrameRecords = {};
        this._timeRecords = {};
        this._frames = [];
        this._minimumRecordTime = -1;
    },

    addFrame: function(frame)
    {
        this._frames.push(frame);
    },

    addRecord: function(record, parentRecord)
    {
        if (this._minimumRecordTime === -1 || record.startTime < this._minimumRecordTime)
            this._minimumRecordTime = WebInspector.TimelineModel.startTimeInSeconds(record);

        var records;
        if (record.type === WebInspector.TimelineModel.RecordType.Program)
            records = record.children;
        else
            records = [record];

        var formattedRecords = [];
        var recordsCount = records.length;
        for (var i = 0; i < recordsCount; ++i)
            formattedRecords.push(this._innerAddRecord(records[i], parentRecord));
        return formattedRecords;
    },

    _innerAddRecord: function(record, parentRecord)
    {
        var connectedToOldRecord = false;
        var recordTypes = WebInspector.TimelineModel.RecordType;

        switch (record.type) {
        // No bar entry for load events.
        case recordTypes.MarkDOMContent:
        case recordTypes.MarkLoad:
            parentRecord = null;
            break;
        case recordTypes.Time:
            parentRecord = this._rootRecord;
            break;
        default:
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

        var formattedRecord = new WebInspector.TimelinePresentationModel.Record(this, record, parentRecord, scriptDetails);

        if (record.type === recordTypes.MarkDOMContent || record.type === recordTypes.MarkLoad)
            return formattedRecord;

        formattedRecord.collapsed = (parentRecord === this._rootRecord);

        var childrenCount = children ? children.length : 0;
        for (var i = 0; i < childrenCount; ++i)
            this._innerAddRecord(children[i], formattedRecord);

        formattedRecord.calculateAggregatedStats(WebInspector.TimelinePresentationModel.categories());

        if (connectedToOldRecord) {
            record = formattedRecord;
            do {
                var parent = record.parent;
                parent._cpuTime += formattedRecord._cpuTime;
                if (parent.lastChildEndTime < record.lastChildEndTime)
                    parent.lastChildEndTime = record.lastChildEndTime;
                for (var category in formattedRecord.aggregatedStats)
                    parent.aggregatedStats[category] += formattedRecord.aggregatedStats[category];
                record = parent;
            } while (record.parent);
        } else {
            if (parentRecord !== this._rootRecord)
                parentRecord.selfTime -= formattedRecord.endTime - formattedRecord.startTime;
        }
        return formattedRecord;
    },

    _findParentRecord: function(record)
    {
        if (!this._glueRecords)
            return null;
        var recordTypes = WebInspector.TimelineModel.RecordType;

        switch (record.type) {
        case recordTypes.ResourceReceiveResponse:
        case recordTypes.ResourceFinish:
        case recordTypes.ResourceReceivedData:
            return this._sendRequestRecords[record.data["requestId"]];

        case recordTypes.TimerFire:
            return this._timerRecords[record.data["timerId"]];

        case recordTypes.ResourceSendRequest:
            return this._scheduledResourceRequests[record.data["url"]];

        case recordTypes.FireAnimationFrame:
            return this._requestAnimationFrameRecords[record.data["id"]];

        case recordTypes.TimeEnd:
            return this._timeRecords[record.data["message"]];
        }
    },

    setGlueRecords: function(glue)
    {
        this._glueRecords = glue;
    },

    filteredRecords: function()
    {
        var recordsInWindow = [];

        var stack = [{children: this._rootRecord.children, index: 0, parentIsCollapsed: false}];
        while (stack.length) {
            var entry = stack[stack.length - 1];
            var records = entry.children;
            if (records && entry.index < records.length) {
                 var record = records[entry.index];
                 ++entry.index;

                 if (this.isVisible(record)) {
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

    isVisible: function(record)
    {
        for (var i = 0; i < this._filters.length; ++i) {
            if (!this._filters[i].accept(record))
                return false;
        }
        return true;
    }
}

WebInspector.TimelinePresentationModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 */
WebInspector.TimelinePresentationModel.Record = function(presentationModel, record, parentRecord, scriptDetails)
{
    this._linkifier = presentationModel._linkifier;
    this._aggregatedStats = [];
    this._record = record;
    this._children = [];
    this.parent = parentRecord;
    if (parentRecord)
        parentRecord.children.push(this);

    this._selfTime = this.endTime - this.startTime;
    this._lastChildEndTime = this.endTime;
    this._startTimeOffset = this.startTime - presentationModel._minimumRecordTime;

    if (record.data && record.data["url"])
        this.url = record.data["url"];
    if (scriptDetails) {
        this.scriptName = scriptDetails.scriptName;
        this.scriptLine = scriptDetails.scriptLine;
    }

    var recordTypes = WebInspector.TimelineModel.RecordType;
    switch (record.type) {
    case recordTypes.ResourceSendRequest:
        // Make resource receive record last since request was sent; make finish record last since response received.
        presentationModel._sendRequestRecords[record.data["requestId"]] = this;
        break;

    case recordTypes.ScheduleResourceRequest:
        presentationModel._scheduledResourceRequests[record.data["url"]] = this;
        break;

    case recordTypes.ResourceReceiveResponse:
        var sendRequestRecord = presentationModel._sendRequestRecords[record.data["requestId"]];
        if (sendRequestRecord) { // False if we started instrumentation in the middle of request.
            this.url = sendRequestRecord.url;
            // Now that we have resource in the collection, recalculate details in order to display short url.
            sendRequestRecord._refreshDetails();
            if (sendRequestRecord.parent !== presentationModel._rootRecord && sendRequestRecord.parent.type === recordTypes.ScheduleResourceRequest)
                sendRequestRecord.parent._refreshDetails();
        }
        break;

    case recordTypes.ResourceReceivedData:
    case recordTypes.ResourceFinish:
        var sendRequestRecord = presentationModel._sendRequestRecords[record.data["requestId"]];
        if (sendRequestRecord) // False for main resource.
            this.url = sendRequestRecord.url;
        break;

    case recordTypes.TimerInstall:
        this.timeout = record.data["timeout"];
        this.singleShot = record.data["singleShot"];
        presentationModel._timerRecords[record.data["timerId"]] = this;
        break;

    case recordTypes.TimerFire:
        var timerInstalledRecord = presentationModel._timerRecords[record.data["timerId"]];
        if (timerInstalledRecord) {
            this.callSiteStackTrace = timerInstalledRecord.stackTrace;
            this.timeout = timerInstalledRecord.timeout;
            this.singleShot = timerInstalledRecord.singleShot;
        }
        break;

    case recordTypes.RequestAnimationFrame:
        presentationModel._requestAnimationFrameRecords[record.data["id"]] = this;
        break;

    case recordTypes.FireAnimationFrame:
        var requestAnimationRecord = presentationModel._requestAnimationFrameRecords[record.data["id"]];
        if (requestAnimationRecord)
            this.callSiteStackTrace = requestAnimationRecord.stackTrace;
        break;

    case recordTypes.Time:
        presentationModel._timeRecords[record.data["message"]] = this;
        break;

    case recordTypes.TimeEnd:
        var timeRecord = presentationModel._timeRecords[record.data["message"]];
        if (timeRecord) {
            var intervalDuration = this.startTime - timeRecord.startTime;
            this.intervalDuration = intervalDuration;
            timeRecord.intervalDuration = intervalDuration;
        }
        break;
    }
}

WebInspector.TimelinePresentationModel.Record.prototype = {
    get lastChildEndTime()
    {
        return this._lastChildEndTime;
    },

    set lastChildEndTime(time)
    {
        this._lastChildEndTime = time;
    },

    get selfTime()
    {
        return this._selfTime;
    },

    set selfTime(time)
    {
        this._selfTime = time;
    },

    get cpuTime()
    {
        return this._cpuTime;
    },

    isLong: function()
    {
        return (this._lastChildEndTime - this.startTime) > WebInspector.TimelinePresentationModel.shortRecordThreshold;
    },

    /**
     * @return {Array.<WebInspector.TimelinePresentationModel.Record>}
     */
    get children()
    {
        return this._children;
    },

    /**
     * @return {number}
     */
    get visibleChildrenCount()
    {
        return this._visibleChildrenCount || 0;
    },

    /**
     * @return {number}
     */
    get invisibleChildrenCount()
    {
        return this._invisibleChildrenCount || 0;
    },

    /**
     * @return {WebInspector.TimelineCategory}
     */
    get category()
    {
        return WebInspector.TimelinePresentationModel.recordStyle(this._record).category
    },

    /**
     * @return {string}
     */
    get title()
    {
        return this.type === WebInspector.TimelineModel.RecordType.TimeStamp ? this._record.data["message"] :
            WebInspector.TimelinePresentationModel.recordStyle(this._record).title;
    },

    /**
     * @return {number}
     */
    get startTime()
    {
        return WebInspector.TimelineModel.startTimeInSeconds(this._record);
    },

    /**
     * @return {number}
     */
    get endTime()
    {
        return WebInspector.TimelineModel.endTimeInSeconds(this._record);
    },

    /**
     * @return {Object}
     */
    get data()
    {
        return this._record.data;
    },

    /**
     * @return {string}
     */
    get type()
    {
        return this._record.type;
    },

    /**
     * @return {string}
     */
    get frameId()
    {
        return this._record.frameId;
    },

    /**
     * @return {number}
     */
    get totalHeapSize()
    {
        return this._record.totalHeapSize;
    },

    /**
     * @return {number}
     */
    get usedHeapSize()
    {
        return this._record.usedHeapSize;
    },

    /**
     * @return {Array.<DebuggerAgent.CallFrame>?}
     */
    get stackTrace()
    {
        if (this._record.stackTrace && this._record.stackTrace.length)
            return this._record.stackTrace;
        return null;
    },

    containsTime: function(time)
    {
        return this.startTime <= time && time <= this.endTime;
    },

    /**
     * @param {function(Element)} callback
     */
    generatePopupContent: function(callback)
    {
        if (WebInspector.TimelinePresentationModel.needsPreviewElement(this.type))
            WebInspector.buildImagePreviewContents(this.url, false, this._generatePopupContentWithImagePreview.bind(this, callback));
        else
            this._generatePopupContentWithImagePreview(callback);
    },

    /**
     * @param {function(Element)} callback
     * @param {Element=} previewElement
     */
    _generatePopupContentWithImagePreview: function(callback, previewElement)
    {
        var contentHelper = new WebInspector.TimelinePresentationModel.PopupContentHelper(this.title);
        var text = WebInspector.UIString("%s (at %s)", Number.secondsToString(this._lastChildEndTime - this.startTime, true),
            Number.secondsToString(this._startTimeOffset));
        contentHelper._appendTextRow(WebInspector.UIString("Duration"), text);

        if (this._children.length) {
            contentHelper._appendTextRow(WebInspector.UIString("Self Time"), Number.secondsToString(this._selfTime, true));
            contentHelper._appendTextRow(WebInspector.UIString("CPU Time"), Number.secondsToString(this._cpuTime, true));
            contentHelper._appendElementRow(WebInspector.UIString("Aggregated Time"),
                WebInspector.TimelinePresentationModel._generateAggregatedInfo(this._aggregatedStats));
        }
        const recordTypes = WebInspector.TimelineModel.RecordType;

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
                contentHelper._appendElementRow(WebInspector.UIString("Location"), this._linkifyScriptLocation());
                break;
            case recordTypes.ScheduleResourceRequest:
            case recordTypes.ResourceSendRequest:
            case recordTypes.ResourceReceiveResponse:
            case recordTypes.ResourceReceivedData:
            case recordTypes.ResourceFinish:
                contentHelper._appendElementRow(WebInspector.UIString("Resource"), this._linkifyLocation(this.url));
                if (previewElement)
                    contentHelper._appendElementRow(WebInspector.UIString("Preview"), previewElement);
                if (this.data["requestMethod"])
                    contentHelper._appendTextRow(WebInspector.UIString("Request Method"), this.data["requestMethod"]);
                if (typeof this.data["statusCode"] === "number")
                    contentHelper._appendTextRow(WebInspector.UIString("Status Code"), this.data["statusCode"]);
                if (this.data["mimeType"])
                    contentHelper._appendTextRow(WebInspector.UIString("MIME Type"), this.data["mimeType"]);
                if (this.data["encodedDataLength"])
                    contentHelper._appendTextRow(WebInspector.UIString("Encoded Data Length"), WebInspector.UIString("%d Bytes", this.data["encodedDataLength"]));
                break;
            case recordTypes.EvaluateScript:
                if (this.data && this.url)
                    contentHelper._appendElementRow(WebInspector.UIString("Script"), this._linkifyLocation(this.url, this.data["lineNumber"]));
                break;
            case recordTypes.Paint:
                contentHelper._appendTextRow(WebInspector.UIString("Location"), WebInspector.UIString("(%d, %d)", this.data["x"], this.data["y"]));
                contentHelper._appendTextRow(WebInspector.UIString("Dimensions"), WebInspector.UIString("%d × %d", this.data["width"], this.data["height"]));
                break;
            case recordTypes.RecalculateStyles: // We don't want to see default details.
                break;
            case recordTypes.Time:
            case recordTypes.TimeEnd:
                if (typeof this.intervalDuration === "number")
                    contentHelper._appendTextRow(WebInspector.UIString("Interval Duration"), Number.secondsToString(this.intervalDuration, true));
                break;
            default:
                if (this.details())
                    contentHelper._appendTextRow(WebInspector.UIString("Details"), this.details());
                break;
        }

        if (this.scriptName && this.type !== recordTypes.FunctionCall)
            contentHelper._appendElementRow(WebInspector.UIString("Function Call"), this._linkifyScriptLocation());

        if (this.usedHeapSize)
            contentHelper._appendTextRow(WebInspector.UIString("Used Heap Size"), WebInspector.UIString("%s of %s", Number.bytesToString(this.usedHeapSize), Number.bytesToString(this.totalHeapSize)));

        if (this.callSiteStackTrace && this.callSiteStackTrace.length)
            contentHelper._appendStackTrace(WebInspector.UIString("Call Site stack"), this.callSiteStackTrace, this._linkifyCallFrame.bind(this));

        if (this.stackTrace)
            contentHelper._appendStackTrace(WebInspector.UIString("Call Stack"), this.stackTrace, this._linkifyCallFrame.bind(this));

        callback(contentHelper._contentTable);
    },

    _refreshDetails: function()
    {
        delete this._details;
    },

    /**
     * @return {Object?|string}
     */
    details: function()
    {
        if (!this._details)
            this._details = this._getRecordDetails();
        return this._details;
    },

    _getRecordDetails: function()
    {
        switch (this.type) {
            case WebInspector.TimelineModel.RecordType.GCEvent:
                return WebInspector.UIString("%s collected", Number.bytesToString(this.data["usedHeapSizeDelta"]));
            case WebInspector.TimelineModel.RecordType.TimerFire:
                return this._linkifyScriptLocation(this.data["timerId"]);
            case WebInspector.TimelineModel.RecordType.FunctionCall:
                return this._linkifyScriptLocation();
            case WebInspector.TimelineModel.RecordType.FireAnimationFrame:
                return this._linkifyScriptLocation(this.data["id"]);
            case WebInspector.TimelineModel.RecordType.EventDispatch:
                return this.data ? this.data["type"] : null;
            case WebInspector.TimelineModel.RecordType.Paint:
                return this.data["width"] + "\u2009\u00d7\u2009" + this.data["height"];
            case WebInspector.TimelineModel.RecordType.DecodeImage:
                return this.data["imageType"];
            case WebInspector.TimelineModel.RecordType.ResizeImage:
                return this.data["cached"] ? WebInspector.UIString("cached") : WebInspector.UIString("non-cached");
            case WebInspector.TimelineModel.RecordType.TimerInstall:
            case WebInspector.TimelineModel.RecordType.TimerRemove:
                return this._linkifyTopCallFrame(this.data["timerId"]);
            case WebInspector.TimelineModel.RecordType.RequestAnimationFrame:
            case WebInspector.TimelineModel.RecordType.CancelAnimationFrame:
                return this._linkifyTopCallFrame(this.data["id"]);
            case WebInspector.TimelineModel.RecordType.ParseHTML:
            case WebInspector.TimelineModel.RecordType.RecalculateStyles:
                return this._linkifyTopCallFrame();
            case WebInspector.TimelineModel.RecordType.EvaluateScript:
                return this.url ? this._linkifyLocation(this.url, this.data["lineNumber"], 0) : null;
            case WebInspector.TimelineModel.RecordType.XHRReadyStateChange:
            case WebInspector.TimelineModel.RecordType.XHRLoad:
            case WebInspector.TimelineModel.RecordType.ScheduleResourceRequest:
            case WebInspector.TimelineModel.RecordType.ResourceSendRequest:
            case WebInspector.TimelineModel.RecordType.ResourceReceivedData:
            case WebInspector.TimelineModel.RecordType.ResourceReceiveResponse:
            case WebInspector.TimelineModel.RecordType.ResourceFinish:
                return WebInspector.displayNameForURL(this.url);
            case WebInspector.TimelineModel.RecordType.TimeStamp:
                return this.data["message"];
            default:
                return this._linkifyScriptLocation() || this._linkifyTopCallFrame() || null;
        }
    },

    /**
     * @param {string} url
     * @param {number=} lineNumber
     * @param {number=} columnNumber
     */
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

    /**
     * @param {string=} defaultValue
     */
    _linkifyTopCallFrame: function(defaultValue)
    {
        return this.stackTrace ? this._linkifyCallFrame(this.stackTrace[0]) : defaultValue;
    },

    /**
     * @param {string=} defaultValue
     */
    _linkifyScriptLocation: function(defaultValue)
    {
        return this.scriptName ? this._linkifyLocation(this.scriptName, this.scriptLine, 0) : defaultValue;
    },

    calculateAggregatedStats: function(categories)
    {
        this._aggregatedStats = {};
        for (var category in categories)
            this._aggregatedStats[category] = 0;
        this._cpuTime = this._selfTime;

        for (var index = this._children.length; index; --index) {
            var child = this._children[index - 1];
            for (var category in categories)
                this._aggregatedStats[category] += child._aggregatedStats[category];
        }
        for (var category in this._aggregatedStats)
            this._cpuTime += this._aggregatedStats[category];
        this._aggregatedStats[this.category.name] += this._selfTime;
    },

    get aggregatedStats()
    {
        return this._aggregatedStats;
    }
}

/**
 * @param {Object} aggregatedStats
 */
WebInspector.TimelinePresentationModel._generateAggregatedInfo = function(aggregatedStats)
{
    var cell = document.createElement("span");
    cell.className = "timeline-aggregated-info";
    for (var index in aggregatedStats) {
        var label = document.createElement("div");
        label.className = "timeline-aggregated-category timeline-" + index;
        cell.appendChild(label);
        var text = document.createElement("span");
        text.textContent = Number.secondsToString(aggregatedStats[index], true);
        cell.appendChild(text);
    }
    return cell;
}

/**
 * @constructor
 * @param {string} title
 */
WebInspector.TimelinePresentationModel.PopupContentHelper = function(title)
{
    this._contentTable = document.createElement("table");
    var titleCell = this._createCell(WebInspector.UIString("%s - Details", title), "timeline-details-title");
    titleCell.colSpan = 2;
    var titleRow = document.createElement("tr");
    titleRow.appendChild(titleCell);
    this._contentTable.appendChild(titleRow);
}

WebInspector.TimelinePresentationModel.PopupContentHelper.prototype = {
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

    _appendStackTrace: function(title, stackTrace, callFrameLinkifier)
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
            var urlElement = callFrameLinkifier(stackFrame);
            linkCell.appendChild(urlElement);
            row.appendChild(linkCell);
            framesTable.appendChild(row);
        }
        this._appendElementRow(title, framesTable, "timeline-stacktrace-title");
    }
}

WebInspector.TimelinePresentationModel.generatePopupContentForFrame = function(frame)
{
    var contentHelper = new WebInspector.TimelinePresentationModel.PopupContentHelper(WebInspector.UIString("Frame"));
    var durationInSeconds = frame.endTime - frame.startTime;
    var durationText = WebInspector.UIString("%s (at %s)", Number.secondsToString(frame.endTime - frame.startTime, true),
        Number.secondsToString(frame.startTimeOffset, true));
    contentHelper._appendTextRow(WebInspector.UIString("Duration"), durationText);
    contentHelper._appendTextRow(WebInspector.UIString("FPS"), Math.floor(1 / durationInSeconds));
    contentHelper._appendTextRow(WebInspector.UIString("CPU time"), Number.secondsToString(frame.cpuTime, true));
    contentHelper._appendElementRow(WebInspector.UIString("Aggregated Time"),
        WebInspector.TimelinePresentationModel._generateAggregatedInfo(frame.timeByCategory));

    return contentHelper._contentTable;
}

/**
 * @param {CanvasRenderingContext2D} context
 * @param {number} width
 * @param {number} height
 * @param {string} color0
 * @param {string} color1
 * @param {string} color2
 */
WebInspector.TimelinePresentationModel.createFillStyle = function(context, width, height, color0, color1, color2)
{
    var gradient = context.createLinearGradient(0, 0, width, height);
    gradient.addColorStop(0, color0);
    gradient.addColorStop(0.25, color1);
    gradient.addColorStop(0.75, color1);
    gradient.addColorStop(1, color2);
    return gradient;
}

/**
 * @param {CanvasRenderingContext2D} context
 * @param {number} width
 * @param {number} height
 * @param {WebInspector.TimelineCategory} category
 */
WebInspector.TimelinePresentationModel.createFillStyleForCategory = function(context, width, height, category)
{
    return WebInspector.TimelinePresentationModel.createFillStyle(context, width, height, category.fillColorStop0, category.fillColorStop1, category.borderColor);
}

/**
 * @param {WebInspector.TimelineCategory} category
 */
WebInspector.TimelinePresentationModel.createStyleRuleForCategory = function(category)
{
    var selector = ".timeline-category-" + category.name + " .timeline-graph-bar, " +
        ".timeline-category-statusbar-item.timeline-category-" + category.name + " .timeline-category-checkbox, " +
        ".popover .timeline-" + category.name + ", " +
        ".timeline-category-" + category.name + " .timeline-tree-icon"

    return selector + " { background-image: -webkit-linear-gradient(" +
       category.fillColorStop0 + ", " + category.fillColorStop1 + " 25%, " + category.fillColorStop1 + " 75%, " + category.borderColor + ");" +
       " border-color: " + category.borderColor +
       "}";
}

/**
 * @interface
 */
WebInspector.TimelinePresentationModel.Filter = function()
{
}

WebInspector.TimelinePresentationModel.Filter.prototype = {
    /**
     * @param {WebInspector.TimelinePresentationModel.Record} record
     */
    accept: function(record) { return false; }
}

/**
 * @constructor
 * @extends {WebInspector.Object}
 * @param {string} name
 * @param {string} title
 * @param {number} overviewStripGroupIndex
 * @param {string} borderColor
 * @param {string} fillColorStop0
 * @param {string} fillColorStop1
 */
WebInspector.TimelineCategory = function(name, title, overviewStripGroupIndex, borderColor, fillColorStop0, fillColorStop1)
{
    this.name = name;
    this.title = title;
    this.overviewStripGroupIndex = overviewStripGroupIndex;
    this.borderColor = borderColor;
    this.fillColorStop0 = fillColorStop0;
    this.fillColorStop1 = fillColorStop1;
    this.hidden = false;
}

WebInspector.TimelineCategory.Events = {
    VisibilityChanged: "VisibilityChanged"
};

WebInspector.TimelineCategory.prototype = {
    /**
     * @return {boolean}
     */
    get hidden()
    {
        return this._hidden;
    },

    set hidden(hidden)
    {
        this._hidden = hidden;
        this.dispatchEventToListeners(WebInspector.TimelineCategory.Events.VisibilityChanged, this);
    }
}

WebInspector.TimelineCategory.prototype.__proto__ = WebInspector.Object.prototype;
