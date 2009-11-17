var timelineAgentRecordType = {};

// Scrub values when printing out these properties in the record or data field.
var timelineNonDeterministicProps = { 
    children : 1,
    endTime : 1, 
    startTime : 1,
    url : 1
};

function printTimelineRecords(performActions, typeName, formatter)
{
    if (performActions) {
        if (window.layoutTestController)
            layoutTestController.setTimelineProfilingEnabled(true);
        performActions();
    }

    evaluateInWebInspector("WebInspector.TimelineAgent.RecordType", function(result) {
        timelineAgentRecordType = result;
    });

    evaluateInWebInspector("frontend_getTimelineResults", function(timelineRecords) {
        if (typeof(timelineRecords) === "string")
            output("Error fetching Timeline results: " + timelineRecords);
        else {
            for (var i = 0; i < timelineRecords.length; ++i) {
                if (typeName && timelineRecords[i].type === timelineAgentRecordType[typeName])
                    printTimelineRecordProperties(timelineRecords[i]);
                if (formatter)
                    formatter(timelineRecords[i]);
            }
        }
        if (window.layoutTestController)
            layoutTestController.setTimelineProfilingEnabled(false);
        notifyDone();
    });
}

// Dump just the record name, indenting output on separate lines for subrecords
function dumpTimelineRecord(record, level) 
{
    if (typeof level !== "number")
        level = 0;
    var prefix = "";
    var suffix = "";
    for (var i = 0; i < level ; ++i)
        prefix = "----" + prefix;
    if (level > 0)
        prefix = prefix + "> ";
    if (record.type === timelineAgentRecordType.MarkTimeline) {
        suffix = " : " + record.data.message;
    }
    output(prefix + timelineAgentTypeToString(record.type) + suffix);

    var numChildren = record.children ? record.children.length : 0;
    for (var i = 0; i < numChildren; ++i)
        dumpTimelineRecord(record.children[i], level + 1);
}

function dumpTimelineRecords(timelineRecords) {
    for (var i = 0; i < timelineRecords.length; ++i)
        dumpTimelineRecord(timelineRecords[i], 0);
}

function printTimelineRecordProperties(record)
{
    output(timelineAgentTypeToString(record.type) + " Properties:");
    // Use this recursive routine to print the properties
    printProps(record, 0);
}

function isNonDeterministicProp(propName)
{
    if (timelineNonDeterministicProps[propName])
        return true;
    return false;
}

function printProps(record, level)
{
    var props = new Array();
    for (var prop in record) {
        props.push(prop);
    }

    var prefix = "+";
    for (var i = 0; i < level ; i++) {
        prefix = prefix + "-";
    }

    prefix = prefix + " ";
    for (var prop in props) {
        var propName = props[prop];
        var propValue = record[propName];
        if (isNonDeterministicProp(propName))
            output(prefix + propName + " : " + (propValue === undefined ? "<undefined>" : " * DEFINED *"));
        else if (typeof propValue === "object") {
            output(prefix + propName + " : {");
            printProps(propValue, level + 1);
            output(prefix + "}");
        } else
            output(prefix + propName + " : " + propValue);
    }
}

function timelineAgentTypeToString(numericType)
{
    for (var prop in timelineAgentRecordType) {
        if (timelineAgentRecordType[prop] === numericType)
            return prop;
    }
    return undefined;
}

// Injected into Inspector window
function frontend_getTimelineResults() {
    var result = [];
    var items = WebInspector.panels.timeline._items;
    for (var i = 0; i < items.length; ++i) {
        result.push(items[i].record);
    }
    return result;
}
