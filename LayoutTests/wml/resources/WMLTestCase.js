var wmlNS = "http://www.wapforum.org/DTD/wml_1.1.xml";
var xhtmlNS = "http://www.w3.org/1999/xhtml";

var testDocument;
var iframeElement;
var bodyElement;

function createWMLElement(name) {
    return testDocument.createElementNS(wmlNS, "wml:" + name);
}

function createWMLTestCase(testDescription, substitutesVariables, testName, executeImmediately) {
    // Setup default test options
    if (substitutesVariables == null)
        substitutesVariables = true;

    // Setup default test name
    if (testName == null) {
        executeImmediately = false; // Only honored, when testName != null
        testName = relativePathToLayoutTests + "/wml/resources/test-document.wml";
    } else if (executeImmediately == null)
        executeImmediately = true;

    // Initialize JS test
    description(testDescription);
    bodyElement = document.getElementsByTagName("body")[0];

    // Clear variable state & history
    document.resetWMLPageState();

    // Setup DRT specific settings
    if (window.layoutTestController) {
        layoutTestController.dumpChildFramesAsText();
        layoutTestController.waitUntilDone();
    }

    // Create container element to load the WML document
    iframeElement = document.createElementNS(xhtmlNS, "iframe");
    iframeElement.src = testName;

    // Process load events, taking care of setting up the native WML testcase
    iframeElement.onload = function() {
        if (testDocument) {
            // Variable substitition mode: resetup test document, substituting all variables stored in the current WML page state.
            // Variable substitution only happens after the initial document has been loaded and after a reload was triggered.        
            if (substitutesVariables) {
                testDocument = iframeElement.contentDocument;
                setupTestDocument();
            }

            assureLayout();
            executeTest();
            return;
        }

        testDocument = iframeElement.contentDocument;
        setupTestDocument();
        prepareTest();

        // In a regular WML document, this would happen after the parsing finished.
        // Though as we dynamically create testcases, we have to take care of initializing WML variable state manually.
        testDocument.initializeWMLPageState();

        // Handle the case of a special WML test document. That's always the case for static WML testcases
        // (which are NOT created manually using JS in prepareTest). Fire off test immediately for those.
        if (executeImmediately) {
            assureLayout();
            executeTest();
        }
    }

    bodyElement.insertBefore(iframeElement, document.getElementById("description"));
}

function assureLayout() {
    // Assure initial layout finished (variable substitions happen on attach time)
    var cards = testDocument.getElementsByTagName("card");
    for (var i = 0; i < cards.length; ++i) {
        cards[i].offsetTop;
    }
}

function startTest(x, y) {
    assureLayout();
    triggerMouseEvent(x, y);
}

function triggerMouseEvent(x, y) {
    // Translation due to HTML content above the WML document in the iframe
    x = x + iframeElement.offsetLeft;
    y = y + iframeElement.offsetTop;

    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function completeTest() {
    var script = document.createElementNS(xhtmlNS, "script");

    script.onload = function() {
        if (window.layoutTestController)
            window.setTimeout("layoutTestController.notifyDone()", 0);
    };

    script.src = relativePathToLayoutTests + "/fast/js/resources/js-test-post.js";
    successfullyParsed = true;

    bodyElement.appendChild(script);
}
