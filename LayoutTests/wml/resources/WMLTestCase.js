var wmlNS = "http://www.wapforum.org/DTD/wml_1.1.xml";
var xhtmlNS = "http://www.w3.org/1999/xhtml";

var testDocument;
var iframeElement;
var bodyElement;

function createWMLElement(name) {
    return testDocument.createElementNS(wmlNS, "wml:" + name);
}

function createWMLTestCase(desc) {
    description(desc);
    bodyElement = document.getElementsByTagName("body")[0];

    // Clear variable state
    document.resetWMLPageState();

    if (window.layoutTestController) {
        layoutTestController.dumpChildFramesAsText();
        layoutTestController.waitUntilDone();
    }

    iframeElement = document.createElementNS(xhtmlNS, "iframe");
    iframeElement.src = "data:text/vnd.wap.wml,<wml><card/></wml>";

    var loaded = false;
    iframeElement.onload = function() {
        testDocument = iframeElement.contentDocument;
        setupTestDocument();

        if (loaded) {
            executeTest();
            return;
        }

        loaded = true;
        prepareTest();
    }

    bodyElement.insertBefore(iframeElement, document.getElementById("description"));
}

function triggerUpdate(x, y) {
    // Translation due to HTML content above the WML document in the iframe
    x = x + iframeElement.offsetLeft;
    y = y + iframeElement.offsetTop;

    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function startTest(x, y) {
    // Assure first layout finished
    window.setTimeout("triggerUpdate(" + x + ", " + y + ")", 0);
}

function completeTest() {
    var script = document.createElementNS(xhtmlNS, "script");

    script.onload = function() {
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    };

    script.src = "../fast/js/resources/js-test-post.js";
    successfullyParsed = true;

    bodyElement.appendChild(script);
}
