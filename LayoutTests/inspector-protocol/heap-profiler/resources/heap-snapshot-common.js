// This script is supposed to be evaluated in dummy inspector front-end which is loaded from
// ../../../http/tests/inspector-protocol/resources/protocol-test.html and the relative paths
// below are relative to that location.

if (!window.WebInspector)
    window.WebInspector = {};
InspectorTest.importScript("../../../../../Source/WebCore/inspector/front-end/HeapSnapshot.js");
InspectorTest.importScript("../../../../../Source/WebCore/inspector/front-end/JSHeapSnapshot.js");

InspectorTest.takeHeapSnapshot = function(callback)
{
    InspectorTest.eventHandler["HeapProfiler.addProfileHeader"] = function(messageObject)
    {
        var profileId = messageObject["params"]["header"]["uid"];
        InspectorTest.sendCommand("HeapProfiler.getHeapSnapshot", { "uid": profileId }, didGetHeapSnapshot);

        function didGetHeapSnapshot(messageObject)
        {
            InspectorTest.log("SUCCESS: didGetHeapSnapshot");
            InspectorTest.completeTest();
        }
    }

    var chunks = [];
    InspectorTest.eventHandler["HeapProfiler.addHeapSnapshotChunk"] = function(messageObject)
    {
        chunks.push(messageObject["params"]["chunk"]);
    }

    InspectorTest.eventHandler["HeapProfiler.finishHeapSnapshot"] = function(messageObject)
    {
        var serializedSnapshot = chunks.join("");
        var parsed = JSON.parse(serializedSnapshot);
        var snapshot = new WebInspector.JSHeapSnapshot(parsed);
        callback(snapshot);
    }

    InspectorTest.sendCommand("HeapProfiler.takeHeapSnapshot", {});
}
