// Force activating pixel tests - this variable is used in fast/js/resources/js-test-pre.js, when calling setDumpAsText().
window.enablePixelTesting = true;

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
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    };

    script.src = "../../fast/js/resources/js-test-post.js";
    document.body.appendChild(script);
}
