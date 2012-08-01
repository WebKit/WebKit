// Force activating pixel tests - this variable is used in fast/js/resources/js-test-pre.js, when calling setDumpAsText().
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
    if (window.testRunner)
        testRunner.waitUntilDone();

    rootSVGElement = createSVGElement("svg");
    rootSVGElement.setAttribute("width", "300");
    rootSVGElement.setAttribute("height", "300");

    var bodyElement = document.documentElement.lastChild;
    bodyElement.insertBefore(rootSVGElement, document.getElementById("description"));
}

function embedSVGTestCase(uri) {
    if (window.testRunner)
        testRunner.waitUntilDone();

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
    x = x + rootSVGElement.offsetLeft;
    y = y + rootSVGElement.offsetTop;

    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function completeTest() {
    var script = document.createElement("script");

    script.onload = function() {
        if (window.testRunner)
            testRunner.notifyDone();
    };

    script.src = "../../fast/js/resources/js-test-post.js";
    document.body.appendChild(script);
}
