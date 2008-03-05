var svgNS = "http://www.w3.org/2000/svg";
var xlinkNS = "http://www.w3.org/1999/xlink";
var xhtmlNS = "http://www.w3.org/1999/xhtml";

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

function triggerUpdate() {
    if (window.eventSender) {
        eventSender.mouseMoveTo(150, 200);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function waitForClickEvent(obj) {
    if (window.layoutTestController)
        obj.setAttribute("onclick", "layoutTestController.notifyDone()");
}
