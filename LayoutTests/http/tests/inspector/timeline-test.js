var initialize_Timeline = function() {

// Scrub values when printing out these properties in the record or data field.
InspectorTest.timelinePropertyFormatters = {
    children: "formatAsTypeName",
    endTime: "formatAsTypeName",
    height: "formatAsTypeName",
    requestId: "formatAsTypeName",
    startTime: "formatAsTypeName",
    width: "formatAsTypeName",
    stackTrace: "formatAsTypeName",
    url: "formatAsTypeName",
    scriptName: "formatAsTypeName",
    usedHeapSize: "formatAsTypeName",
    usedHeapSizeDelta: "skip",
    mimeType: "formatAsTypeName",
    id: "formatAsTypeName",
    counters: "formatAsTypeName",
    timerId: "formatAsTypeName",
    scriptLine: "formatAsTypeName",
    lineNumber: "formatAsTypeName",
    frameId: "formatAsTypeName",
    encodedDataLength: "formatAsTypeName",
    identifier: "formatAsTypeName"    
};

InspectorTest.startTimeline = function(callback)
{
    InspectorTest._timelineRecords = [];
    WebInspector.panel("timeline").toggleTimelineButton.toggled = true;
    WebInspector.panel("timeline")._model._collectionEnabled = true;
    TimelineAgent.start(5, true, false, callback);
    function addRecord(record)
    {
        InspectorTest._timelineRecords.push(record);
        for (var i = 0; record.children && i < record.children.length; ++i)
            addRecord(record.children[i]);
    }
    InspectorTest._addTimelineEvent = function(event)
    {
        addRecord(event.data);
    }
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, InspectorTest._addTimelineEvent);
};


InspectorTest.waitForRecordType = function(recordType, callback)
{
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, addEvent);

    function addEvent(event)
    {
        addRecord(event.data);
    }
    function addRecord(record)
    {
        if (record.type !== WebInspector.TimelineModel.RecordType[recordType]) {
            for (var i = 0; record.children && i < record.children.length; ++i)
                addRecord(record.children[i]);
            return;
        }
        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, addEvent);
        callback(record);
    }
}

InspectorTest.stopTimeline = function(callback)
{
    function didStop()
    {
        WebInspector.timelineManager.removeEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, InspectorTest._addTimelineEvent);
        WebInspector.panel("timeline").toggleTimelineButton.toggled = false;
        WebInspector.panel("timeline")._model._collectionEnabled = false;
        callback(InspectorTest._timelineRecords);
    }
    TimelineAgent.stop(didStop);
};

InspectorTest.evaluateWithTimeline = function(actions, doneCallback)
{
    InspectorTest.startTimeline(step1);
    function step1()
    {
        InspectorTest.evaluateInPage(actions, step2);
    }

    function step2()
    {
        InspectorTest.stopTimeline(doneCallback);
    }

}

InspectorTest.performActionsAndPrint = function(actions, typeName, includeTimeStamps)
{
    function callback()
    {
        InspectorTest.printTimelineRecords(typeName);
        if (includeTimeStamps) {
            InspectorTest.addResult("Timestamp records: ");
            InspectorTest.printTimestampRecords(typeName);
        }
        InspectorTest.completeTest();
    }
    InspectorTest.evaluateWithTimeline(actions, callback)
};

InspectorTest.printTimelineRecords = function(typeName, formatter)
{
    InspectorTest.innerPrintTimelineRecords(InspectorTest._timelineRecords, typeName, formatter);
};

InspectorTest.printTimestampRecords = function(typeName, formatter)
{
    InspectorTest.innerPrintTimelineRecords(WebInspector.panels.timeline._presentationModel.eventDividerRecords().select("_record"), typeName, formatter);
};

InspectorTest.innerPrintTimelineRecords = function(records, typeName, formatter)
{
    for (var i = 0; i < records.length; ++i) {
        if (typeName && records[i].type === WebInspector.TimelineModel.RecordType[typeName])
            InspectorTest.printTimelineRecordProperties(records[i]);
        if (formatter)
            formatter(records[i]);
    }
};

// Dump just the record name, indenting output on separate lines for subrecords
InspectorTest.dumpTimelineRecord = function(record, level) 
{
    if (typeof level !== "number")
        level = 0;
    var prefix = "";
    var suffix = "";
    for (var i = 0; i < level ; ++i)
        prefix = "----" + prefix;
    if (level > 0)
        prefix = prefix + "> ";
    if (record.type === WebInspector.TimelineModel.RecordType.TimeStamp) {
        suffix = " : " + record.data.message;
    }
    InspectorTest.addResult(prefix + InspectorTest._timelineAgentTypeToString(record.type) + suffix);

    var numChildren = record.children ? record.children.length : 0;
    for (var i = 0; i < numChildren; ++i)
        InspectorTest.dumpTimelineRecord(record.children[i], level + 1);
};

InspectorTest.dumpTimelineRecords = function(timelineRecords)
{
    for (var i = 0; i < timelineRecords.length; ++i)
        InspectorTest.dumpTimelineRecord(timelineRecords[i], 0);
};

InspectorTest.printTimelineRecordProperties = function(record)
{
    InspectorTest.addResult(InspectorTest._timelineAgentTypeToString(record.type) + " Properties:");
    // Use this recursive routine to print the properties
    InspectorTest.addObject(record, InspectorTest.timelinePropertyFormatters);
};

InspectorTest._timelineAgentTypeToString = function(numericType)
{
    for (var prop in WebInspector.TimelineModel.RecordType) {
        if (WebInspector.TimelineModel.RecordType[prop] === numericType)
            return prop;
    }
    return undefined;
};

InspectorTest.findPresentationRecord = function(type)
{
    var result;
    function findByType(record)
    {
        if (record.type !== type)
            return false;
        result = record;
        return true;
    }
    var records = WebInspector.panel("timeline")._rootRecord().children;
    WebInspector.TimelinePresentationModel.forAllRecords(records, findByType);
    return result;
}

InspectorTest.FakeFileReader = function(input, delegate, callback)
{
    this._delegate = delegate;
    this._callback = callback;
    this._input = input;
    this._loadedSize = 0;
    this._fileSize = input.length;
};

InspectorTest.FakeFileReader.prototype = {
    start: function(output)
    {
        this._delegate.onTransferStarted(this);

        var length = this._input.length;
        var half = (length + 1) >> 1;

        var chunk = this._input.substring(0, half);
        this._loadedSize += chunk.length;
        output.write(chunk);
        this._delegate.onChunkTransferred(this);

        chunk = this._input.substring(half);
        this._loadedSize += chunk.length;
        output.write(chunk);
        this._delegate.onChunkTransferred(this);

        output.close();
        this._delegate.onTransferFinished(this);

        this._callback();
    },

    cancel: function() { },

    loadedSize: function()
    {
        return this._loadedSize;
    },

    fileSize: function()
    {
        return this._fileSize;
    },

    fileName: function()
    {
        return "fakeFile";
    }
};

};
