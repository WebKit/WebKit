var initialize_Timeline = function() {

// Scrub values when printing out these properties in the record or data field.
InspectorTest.timelineNonDeterministicProps = { 
    children : 1,
    endTime : 1, 
    height : 1,
    identifier : 1,
    startTime : 1,
    width : 1,
    stackTrace: 1,
    url : 1,
    usedHeapSize: 1,
    totalHeapSize: 1,
    mimeType : 1
};

InspectorTest.startTimeline = function(callback)
{
    InspectorTest._timelineRecords = [];
    TimelineAgent.start(callback);
    function addRecord(record)
    {
        InspectorTest._timelineRecords.push(record);
        for (var i = 0; record.children && i < record.children.length; ++i)
            addRecord(record.children[i]);
    }
    InspectorTest.addSniffer(WebInspector.TimelineDispatcher.prototype, "addRecordToTimeline", addRecord, true);
};

InspectorTest.stopTimeline = function(callback)
{
    TimelineAgent.stop(callback);
};

InspectorTest.performActionsAndPrint = function(actions, typeName)
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
        InspectorTest.completeTest();
    }
};

InspectorTest.printTimelineRecords = function(typeName, formatter)
{
    for (var i = 0; i < InspectorTest._timelineRecords.length; ++i) {
        var record = InspectorTest._timelineRecords[i];
        if (typeName && record.type === WebInspector.TimelineAgent.RecordType[typeName])
            InspectorTest.printTimelineRecordProperties(record);
        if (formatter)
            formatter(record);
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
    if (record.type === WebInspector.TimelineAgent.RecordType.MarkTimeline) {
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
    InspectorTest.addObject(record, InspectorTest.timelineNonDeterministicProps);
};

InspectorTest._timelineAgentTypeToString = function(numericType)
{
    for (var prop in WebInspector.TimelineAgent.RecordType) {
        if (WebInspector.TimelineAgent.RecordType[prop] === numericType)
            return prop;
    }
    return undefined;
};

};
