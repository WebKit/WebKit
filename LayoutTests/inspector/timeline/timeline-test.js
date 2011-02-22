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

InspectorTest.performActionsAndPrint = function(actions, typeName)
{
    TimelineAgent.start(step1);
    function step1()
    {
        InspectorTest.evaluateInPage(actions, step2);
    }

    function step2()
    {
        TimelineAgent.stop(step3);
    }

    function step3()
    {
        InspectorTest.printTimelineRecords(typeName);
        InspectorTest.completeTest();
    }
}

InspectorTest.printTimelineRecords = function(typeName, formatter)
{
    var timelineRecords = InspectorTest._timelineResults();
    for (var i = 0; i < timelineRecords.length; ++i) {
        var record = timelineRecords[i];
        if (typeName && record.type === WebInspector.TimelineAgent.RecordType[typeName])
            InspectorTest.printTimelineRecordProperties(record);
        if (formatter)
            formatter(record);
    }
};

InspectorTest._timelineResults = function() {
    var result = [];
    function addRecords(records)
    {
        if (!records)
            return;
        for (var i = 0; i < records.length; ++i) {
            result.push(records[i].originalRecordForTests);
            addRecords(records[i].children);
        }
    }
    addRecords(WebInspector.panels.timeline._rootRecord.children);
    return result;
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
