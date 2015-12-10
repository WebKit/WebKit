var eventFrequencyTable;

function resetTest()
{
    eventFrequencyTable = {};
}

function log(message)
{
    document.getElementById("console").appendChild(document.createTextNode(message + "\n"));
}

function checkEvent(event)
{
    if (document.visibilityState === "visible")
        return;
    var type = event.type;
    if (!eventFrequencyTable[type])
        eventFrequencyTable[type] = 0;
    ++eventFrequencyTable[type];
}

function handleVisibilityChange()
{
    if (document.visibilityState === "hidden")
        return;
    var receivedEventsMessageParts = [];
    for (var type in eventFrequencyTable)
        receivedEventsMessageParts.push(type + " (\u00D7 " + eventFrequencyTable[type] + ")");
    if (receivedEventsMessageParts.length)
        log("Expected to receive no events when the page was hidden, but received: " + receivedEventsMessageParts.join(", ") + ".");
    else
        log("Received no events when the page was hidden.");
    resetTest();
}

resetTest();

window.addEventListener("devicemotion", checkEvent, false);
window.addEventListener("deviceorientation", checkEvent, false);
document.addEventListener("visibilitychange", handleVisibilityChange, false);
