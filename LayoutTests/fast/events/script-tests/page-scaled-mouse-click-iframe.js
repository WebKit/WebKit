description("This tests that page scaling and CSS transforms do not affect mouse event pageX and pageY coordinates for " +
            "content embedded in an iframe.");

var html = document.documentElement;

var iframe = document.createElement("iframe");
iframe.style.border = "none";
iframe.style.width = "200px";
iframe.style.height = "200px";
iframe.style.background = "green";

var div = document.createElement("div");
div.style.width = "100px";
div.style.height = "100px";
div.style.backgroundColor = "blue";

var eventLog = "";

function appendEventLog(event) {
    var msg = event.type + "(" + event.pageX + ", " + event.pageY + ")";

    if (window.eventSender) {
        eventLog += msg;
    } else {
        debug(msg);
    }
}

function clearEventLog() {
    eventLog = "";
}

function sendEvents(button) {
    if (!window.eventSender) {
        debug("This test requires the EventSender API (provided by, for example, DumpRenderTree or WebKitTestRunner). Click on the blue rect with the left mouse button to log the mouse coordinates.")
        return;
    }
    eventSender.mouseDown(button);
    eventSender.mouseUp(button);
}

function testEvents(button, description, expectedString) {
    sendEvents(button);
    debug(description);
    shouldBeEqualToString("eventLog", expectedString);
    clearEventLog();
    debug("");
}

function iframeLoaded() {
  // Add the div to the iframe.
  div.addEventListener("click", appendEventLog, false);
  iframe.contentWindow.document.body.insertBefore(div, iframe.contentWindow.document.body.firstChild);

  if (window.eventSender) {
    eventSender.mouseMoveTo(10, 10);
    // We are clicking in the same position on screen. As we scale or transform the page,
    // we expect the pageX and pageY event coordinates to change because different
    // parts of the document are under the mouse.

    testEvents(0, "Unscaled", "click(10, 10)");

    eventSender.scalePageBy(0.5, 0, 0);
    testEvents(0, "setPageScale(0.5)", "click(20, 20)");

    eventSender.scalePageBy(1.0, 0, 0);
    html.style["-webkit-transform"] = "scale(0.5, 2.0)";
    html.style["-webkit-transform-origin"] = "0 0";
    testEvents(0, "CSS scale(0.5, 2.0)", "click(20, 5)");

    eventSender.scalePageBy(0.5, 0, 0);
    testEvents(0, "setPageScale(0.5), CSS scale(0.5, 2.0)", "click(40, 10)");
  }

  finishJSTest();
}

// Add the iframe to the document.
iframe.src = "resources/page-scaled-mouse-click-iframe-inner.html";
document.body.insertBefore(iframe, document.body.firstChild);

window.jsTestIsAsync = true;
