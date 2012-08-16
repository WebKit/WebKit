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
WebInspector.TimelineModel = function()
{
    this._records = [];
    this._stringPool = new StringPool();
    this._minimumRecordTime = -1;
    this._maximumRecordTime = -1;
    this._collectionEnabled = false;

    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, this._onRecordAdded, this);
}

WebInspector.TimelineModel.RecordType = {
    Root: "Root",
    Program: "Program",
    EventDispatch: "EventDispatch",

    BeginFrame: "BeginFrame",
    Layout: "Layout",
    RecalculateStyles: "RecalculateStyles",
    Paint: "Paint",
    DecodeImage: "DecodeImage",
    ResizeImage: "ResizeImage",
    CompositeLayers: "CompositeLayers",

    ParseHTML: "ParseHTML",

    TimerInstall: "TimerInstall",
    TimerRemove: "TimerRemove",
    TimerFire: "TimerFire",

    XHRReadyStateChange: "XHRReadyStateChange",
    XHRLoad: "XHRLoad",
    EvaluateScript: "EvaluateScript",

    MarkLoad: "MarkLoad",
    MarkDOMContent: "MarkDOMContent",

    TimeStamp: "TimeStamp",
    Time: "Time",
    TimeEnd: "TimeEnd",

    ScheduleResourceRequest: "ScheduleResourceRequest",
    ResourceSendRequest: "ResourceSendRequest",
    ResourceReceiveResponse: "ResourceReceiveResponse",
    ResourceReceivedData: "ResourceReceivedData",
    ResourceFinish: "ResourceFinish",

    FunctionCall: "FunctionCall",
    GCEvent: "GCEvent",

    RequestAnimationFrame: "RequestAnimationFrame",
    CancelAnimationFrame: "CancelAnimationFrame",
    FireAnimationFrame: "FireAnimationFrame"
}

WebInspector.TimelineModel.Events = {
    RecordAdded: "RecordAdded",
    RecordsCleared: "RecordsCleared"
}

WebInspector.TimelineModel.startTimeInSeconds = function(record)
{
    return record.startTime / 1000;
}

WebInspector.TimelineModel.endTimeInSeconds = function(record)
{
    return (typeof record.endTime === "undefined" ? record.startTime : record.endTime) / 1000;
}

WebInspector.TimelineModel.durationInSeconds = function(record)
{
    return WebInspector.TimelineModel.endTimeInSeconds(record) - WebInspector.TimelineModel.startTimeInSeconds(record);
}

/**
 * @param {Object} total
 * @param {Object} rawRecord
 */
WebInspector.TimelineModel.aggregateTimeForRecord = function(total, rawRecord)
{
    var childrenTime = 0;
    var children = rawRecord["children"] || [];
    for (var i = 0; i < children.length; ++i)  {
        WebInspector.TimelineModel.aggregateTimeForRecord(total, children[i]);
        childrenTime += WebInspector.TimelineModel.durationInSeconds(children[i]);
    }
    var categoryName = WebInspector.TimelinePresentationModel.recordStyle(rawRecord).category.name;
    var ownTime = WebInspector.TimelineModel.durationInSeconds(rawRecord) - childrenTime;
    total[categoryName] = (total[categoryName] || 0) + ownTime;
}

WebInspector.TimelineModel.prototype = {
    startRecord: function()
    {
        if (this._collectionEnabled)
            return;
        this.reset();
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
        this._stringPool.internObjectStrings(record);
        this._records.push(record);
        this._updateBoundaries(record);
        this.dispatchEventToListeners(WebInspector.TimelineModel.Events.RecordAdded, record);
    },

    /**
     * @param {WebInspector.Progress} progress
     * @param {Array.<Object>} data
     * @param {number} index
     */
    _loadNextChunk: function(progress, data, index)
    {
        if (progress.isCanceled()) {
            this.reset();
            progress.done();
            return;
        }
        progress.setWorked(index);

        for (var i = 0; i < 100 && index < data.length; ++i, ++index)
            this._addRecord(data[index]);

        if (index !== data.length)
            setTimeout(this._loadNextChunk.bind(this, progress, data, index), 0);
        else
            progress.done();
    },

    /**
     * @param {!Blob} file
     * @param {WebInspector.Progress} progress
     */
    loadFromFile: function(file, progress)
    {
        var compositeProgress = new WebInspector.CompositeProgress(progress);
        var loadingProgress = compositeProgress.createSubProgress(1);
        var parsingProgress = compositeProgress.createSubProgress(1);
        var processingProgress = compositeProgress.createSubProgress(1);

        function parseAndImportData(data)
        {
            var records = JSON.parse(data);
            parsingProgress.done();
            this.reset();
            processingProgress.setTotalWork(records.length);
            this._loadNextChunk(processingProgress, records, 1);
        }

        function onLoad(e)
        {
            loadingProgress.done();
            parsingProgress.setTotalWork(1);
            setTimeout(parseAndImportData.bind(this, e.target.result), 0);
        }

        function onError(e)
        {
            progress.done();
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

        function onProgress(e)
        {
            if (e.lengthComputable)
                loadingProgress.setWorked(e.loaded / e.total);
        }

        var reader = new FileReader();
        reader.onload = onLoad.bind(this);
        reader.onerror = onError;
        reader.onprogress = onProgress;
        loadingProgress.setTitle(WebInspector.UIString("Loading\u2026"));
        loadingProgress.setTotalWork(1);
        reader.readAsText(file);
    },

    saveToFile: function()
    {
        var records = ['[' + JSON.stringify(new String(window.navigator.appVersion))];
        for (var i = 0; i < this._records.length; ++i)
            records.push(JSON.stringify(this._records[i]));

        records[records.length - 1] = records[records.length - 1] + "]";

        var now = new Date();
        var fileName = "TimelineRawData-" + now.toISO8601Compact() + ".json";
        WebInspector.fileManager.save(fileName, records.join(",\n"), true);

    },

    reset: function()
    {
        this._records = [];
        this._stringPool.reset();
        this._minimumRecordTime = -1;
        this._maximumRecordTime = -1;
        this.dispatchEventToListeners(WebInspector.TimelineModel.Events.RecordsCleared);
    },

    minimumRecordTime: function()
    {
        return this._minimumRecordTime;
    },

    maximumRecordTime: function()
    {
        return this._maximumRecordTime;
    },

    _updateBoundaries: function(record)
    {
        var startTime = WebInspector.TimelineModel.startTimeInSeconds(record);
        var endTime = WebInspector.TimelineModel.endTimeInSeconds(record);

        if (this._minimumRecordTime === -1 || startTime < this._minimumRecordTime)
            this._minimumRecordTime = startTime;
        if (this._maximumRecordTime === -1 || endTime > this._maximumRecordTime)
            this._maximumRecordTime = endTime;
    },

    /**
     * @param {Object} rawRecord
     */
    recordOffsetInSeconds: function(rawRecord)
    {
        return WebInspector.TimelineModel.startTimeInSeconds(rawRecord) - this._minimumRecordTime;
    }
}

WebInspector.TimelineModel.prototype.__proto__ = WebInspector.Object.prototype;
