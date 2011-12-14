// NOTE: You must include fast/js/resources/js-test-pre.js before this file in a test case since
// this file makes use of functions in js-test-pre.js.

var replayEventQueue = []; // Global queue of recorded events.
var registeredElementsAndEventsStruct; // Global structure of registered elements and events.

function registerElementsAndEventsToRecord(elementsToRecord, eventsToRecord)
{
    registeredElementsAndEventsStruct = {"elementsToRecord": elementsToRecord, "eventsToRecord": eventsToRecord};
}

function beginRecordingEvents()
{
    function callback(element, eventName)
    {
        element.addEventListener(eventName, _recordEvent, false);
    }
    _processEachRegisteredElement(callback);
}

function endRecordingEvents()
{
    function callback(element, eventName)
    {
        element.removeEventListener(eventName, _recordEvent, false);
    }
    _processEachRegisteredElement(callback);
}

function _processEachRegisteredElement(callback)
{
    if (!registeredElementsAndEventsStruct)
        return;
    var elements = registeredElementsAndEventsStruct.elementsToRecord;
    var events = registeredElementsAndEventsStruct.eventsToRecord;
    for (var i = 0; i < elements.length; ++i) {
        for (var j = 0; j < events.length; ++j)
            callback(elements[i], events[j])
    }
}

function _recordEvent(event)
{
    replayEventQueue.push(event);
}

function checkThatEventsFiredInOrder(expectedOrderQueue)
{
    function eventTarget(event)
    {
        // In Internet Explorer an Event object does not have a "target" property.
        // The analagous property is called "srcElement".
        return event.target || event.srcElement;
    }

    function elementIdOrTagName(element)
    {
        return element.id || element.tagName;
    }

    while (replayEventQueue.length && expectedOrderQueue.length) {
        var replayedEvent = replayEventQueue.shift();
        var expectedEvent = expectedOrderQueue.shift();
        var replayedEventTargetName = elementIdOrTagName(eventTarget(replayedEvent));
        if (replayedEventTargetName === expectedEvent[0] && replayedEvent.type === expectedEvent[1])
            testPassed('fired event is (' + replayedEventTargetName + ', ' + replayedEvent.type + ').');
        else {
            testFailed('fired event is (' + replayedEventTargetName + ', ' + replayedEvent.type + '). ' +
                       'Should be (' + expectedEvent[0] + ', ' + expectedEvent[1] + ').');
        }
    }
    while (replayEventQueue.length) {
        var replayedEvent = replayEventQueue.shift();
        testFailed('should not have fired event (' + elementIdOrTagName(eventTarget(replayedEvent)) + ', ' + replayedEvent.type + '). But did.');
    }
    while (expectedOrderQueue.length) {
        var expectedEvent = expectedOrderQueue.shift();
        testFailed('should have fired event (' + expectedEvent[0] + ', ' + expectedEvent[1] + '). But did not.');
    }
}
