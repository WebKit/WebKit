description('This tests checks the event order for the context click. It should be mousedown, mouseup, and contextmenu with an event.button == 2. Right click on this text to see the results.');

var previousEventType = "START";
var event = 0;
function clickHandler(e)
{
    if (e.type == "contextmenu") {
        // Prevent the context menu from being displayed.
        e.returnValue = false;
        if (e.stopPropagation)
            e.stopPropagation();
    }
    event = e;
    shouldBe("event.button", "2");

    // Check that the events are happening in the expected order.
    switch (previousEventType) {
    case "START":
        shouldBeEqualToString("event.type", "mousedown");
        break;
    case "mousedown":
        shouldBeEqualToString("event.type", "mouseup");
        break;
    case "mouseup":
        shouldBeEqualToString("event.type", "contextmenu");
        break;
    case "contextmenu":
        testFailed('event.type is "' + event.type + '".  None were expected.');
        break;
    }
    previousEventType = event.type;
}

function traceMouseEvent(target, eventName)
{
    if (target.addEventListener)
        target.addEventListener(eventName, clickHandler, false);
    else if (target.attachEvent) /*Internet Explorer*/
        target.attachEvent("on" + eventName, clickHandler);
    else
        testFailed("Failed registering " + eventName);    
}

var target = document.getElementById("description");
traceMouseEvent(target, "click");
traceMouseEvent(target, "mousedown");
traceMouseEvent(target, "mouseup");
traceMouseEvent(target, "contextmenu");

if (window.layoutTestController) {
     var box = document.getElementById("description");
     var x = box.offsetParent.offsetLeft + box.offsetLeft + box.offsetWidth / 2;
     var y = box.offsetParent.offsetTop + box.offsetTop + box.offsetHeight / 2;
     eventSender.mouseMoveTo(x, y);
     eventSender.contextClick();
     layoutTestController.dumpAsText();
}


var successfullyParsed = true;
