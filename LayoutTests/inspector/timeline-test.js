// Used to mark timeline records as belonging to the test framework.
var timelineOverheadMark = "***Overhead***";

// TimelineAgent record type definitions from the inspector
var timelineAgentRecordType = {};

// Scrub values when printing out these properties in the record or data field.
var timelineNonDeterministicProps = { 
    children : 1,
    endTime : 1, 
    startTime : 1,
    url : 1
};

// Call this function from the doit() function in the main test page.
// Pass a function that will get an array of timeline data to process.
function retrieveTimelineData(analyzeFunction)
{
    evaluateInWebInspector("WebInspector.TimelineAgent.RecordType", function(result) {
        timelineAgentRecordType = result;
    });

    evaluateInWebInspector("frontend_getTimelineResults()", function(timelineRecords) {
        if (typeof(timelineRecords) === "string")
            output("Error fetching Timeline results: " + timelineRecords);
        else 
            analyzeFunction(timelineRecords);
        notifyDone();
    });
    markTimelineRecordAsOverhead("onload" + (ignoreLoad ? ":ignoreLoad": ""));
}

// Dump just the record name, indenting output on separate lines for subrecords
function dumpTimelineRecord(record, level) 
{
    var prefix = "";
    var suffix = "";
    for (var i = 0; i < level ; ++i)
        prefix = "----" + prefix;
    if (level > 0)
        prefix = prefix + "> ";
    if (record.type == timelineAgentRecordType.MarkTimeline) {
        suffix = " : " + record.data.message;
    }
    output(prefix + timelineAgentTypeToString(record.type) + suffix);

    var numChildren = record.children ? record.children.length : 0;
    for (var i = 0; i < numChildren; ++i)
        dumpTimelineRecord(record.children[i], level + 1);
}

// Dumps an entire list of records, culling out those marked as overhead
function dumpTimelineRecords(timelineRecords) {
    var numRecords = timelineRecords.length;
    for (var i = 0; i < numRecords; ++i) {
        var record = timelineRecords[i];
        if (!isTimelineOverheadRecord(record))
            dumpTimelineRecord(record, 0);
    }
}

// Sort the fields, then strip out startTime and endTime, they are not deterministic.
// Also remove children - that field isn't important for the printout.
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

function printProps (record, level)
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

// Records that have been marked with console.markTimeline(timelineMark)
// are a part of the test framework and not a part of the test being performed
function isTimelineOverheadRecord(record) {
    if (record.type === 11 && record.data.message.indexOf(timelineOverheadMark) !== -1)
        return true;
    var numChildren = record.children ?  record.children.length : 0;
    for (var i = 0; i < numChildren; ++i) {
        if (isTimelineOverheadRecord(record.children[i]))
            return true;
    }
    return false;
}

// This mark will help the test analysis cull out records that are overhead.
function markTimelineRecordAsOverhead(arg)
{
    var suffix = '';
    if (arg)
        suffix = ": " + arg;
    console.markTimeline(timelineOverheadMark + suffix);
}

function timelineAgentTypeToString(numericType)
{
    for (var prop in timelineAgentRecordType) {
        if (timelineAgentRecordType[prop] == numericType)
            return prop;
    }
    return undefined;
}

// Injected into Inspector window
function frontend_startTimelineProfiler()
{
    window.timelineResults = new Array();
    window.WebInspector.addRecordToTimeline = function(arg) {
        window.timelineResults.push(arg);
    };
    window.InspectorController.startTimelineProfiler();
    return 'done';
}

// Injected into Inspector window
function frontend_getTimelineResults() {
    return window.timelineResults;
}

// Injected into Inspector window
// frontend_setup always gets called before the page is reloaded.
function frontend_setup() {
    frontend_startTimelineProfiler();
}
