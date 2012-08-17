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
    totalHeapSize: "formatAsTypeName",
    mimeType: "formatAsTypeName",
    id: "formatAsTypeName",
    counters: "formatAsTypeName",
    timerId: "formatAsTypeName",
    scriptLine: "formatAsTypeName",
    lineNumber: "formatAsTypeName",
    frameId: "formatAsTypeName",
    encodedDataLength: "formatAsTypeName"
};

InspectorTest.startTimeline = function(callback)
{
    InspectorTest._timelineRecords = [];
    WebInspector.panel("timeline").toggleTimelineButton.toggled = true;
    WebInspector.panel("timeline")._model._collectionEnabled = true;
    TimelineAgent.start(callback);
    function addRecord(record)
    {
        InspectorTest._timelineRecords.push(record);
        for (var i = 0; record.children && i < record.children.length; ++i)
            addRecord(record.children[i]);
    }
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, function(event) {
        addRecord(event.data);
    });
};


InspectorTest.waitForRecordType = function(recordType, callback)
{
    WebInspector.timelineManager.addEventListener(WebInspector.TimelineManager.EventTypes.TimelineEventRecorded, function(event) {
            addRecord(event.data);
    });

    function addRecord(record)
    {
        if (record.type !== WebInspector.TimelineModel.RecordType[recordType]) {
            for (var i = 0; record.children && i < record.children.length; ++i)
                addRecord(record.children[i]);
            return ;
        }
        callback(record);
    }
}

InspectorTest.stopTimeline = function(callback)
{
    function didStop()
    {
        WebInspector.panel("timeline").toggleTimelineButton.toggled = false;
        WebInspector.panel("timeline")._model._collectionEnabled = false;
        callback(InspectorTest._timelineRecords);
    }
    TimelineAgent.stop(didStop);
};

InspectorTest.performActionsAndPrint = function(actions, typeName, includeTimeStamps)
{
    InspectorTest.startTimeline(step1);
    function step1()
    {
        InspectorTest.evaluateInPage(actions, step2);
    }

    function step2()
    {
        InspectorTest.stopTimeline(step3);
    }

    function step3()
    {
        InspectorTest.printTimelineRecords(typeName);
        if (includeTimeStamps) {
            InspectorTest.addResult("Timestamp records: ");
            InspectorTest.printTimestampRecords(typeName);
        }
        InspectorTest.completeTest();
    }
};

InspectorTest.printTimelineRecords = function(typeName, formatter)
{
    InspectorTest.innerPrintTimelineRecords(InspectorTest._timelineRecords, typeName, formatter);
};

InspectorTest.printTimestampRecords = function(typeName, formatter)
{
    InspectorTest.innerPrintTimelineRecords(WebInspector.panels.timeline._timeStampRecords.select("_record"), typeName, formatter);
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

};
