// Force activating pixel tests - this variable is used in resources/js-test.js, when calling setDumpAsText().
window.enablePixelTesting = true;

var svgNS = "http://www.w3.org/2000/svg";
var xlinkNS = "http://www.w3.org/1999/xlink";
var xhtmlNS = "http://www.w3.org/1999/xhtml";

var rootSVGElement;
var iframeElement;

function createSVGElement(name) {
    return document.createElementNS(svgNS, "svg:" + name);
}

function createSVGTestCase() {
    window.jsTestIsAsync = true;
    rootSVGElement = createSVGElement("svg");
    rootSVGElement.setAttribute("width", "300");
    rootSVGElement.setAttribute("height", "300");

    var bodyElement = document.documentElement.lastChild;
    bodyElement.insertBefore(rootSVGElement, document.getElementById("description"));
}

function embedSVGTestCase(uri) {
    window.jsTestIsAsync = true;
    iframeElement = document.createElement("iframe");
    iframeElement.setAttribute("style", "width: 300px; height: 300px;");
    iframeElement.setAttribute("src", uri);
    iframeElement.setAttribute("onload", "iframeLoaded()");

    var bodyElement = document.documentElement.lastChild;
    bodyElement.insertBefore(iframeElement, document.getElementById("description"));
}

function iframeLoaded() {
    rootSVGElement = iframeElement.getSVGDocument().rootElement;
}

function clickAt(x, y) {
    // Translation due to <h1> above us
    var clientRect = rootSVGElement.getBoundingClientRect();
    x = x + clientRect.left;
    y = y + clientRect.top;

    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function completeTest() {
    finishJSTest();
}
