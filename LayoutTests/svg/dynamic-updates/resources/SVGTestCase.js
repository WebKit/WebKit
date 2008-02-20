var svgNS = "http://www.w3.org/2000/svg";
var xlinkNS = "http://www.w3.org/1999/xlink";

var rootSVGElement;

function createSVGElement(name) {
    return document.createElementNS(svgNS, "svg:" + name);
}

function createSVGTestCase() {
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();

    rootSVGElement = createSVGElement("svg");
    rootSVGElement.setAttribute("width", "300");
    rootSVGElement.setAttribute("height", "300");

    var bodyElement = document.documentElement.lastChild;
    bodyElement.insertBefore(rootSVGElement, document.getElementById("description"));
}

function sendMouseEvent() {
    if (window.eventSender) {
        eventSender.mouseMoveTo(150, 200);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function triggerUpdate() {
    window.setTimeout("sendMouseEvent()", 0);
}

function waitForClickEvent(obj) {
    if (window.layoutTestController)
        obj.setAttribute("onclick", "layoutTestController.notifyDone()");
}
