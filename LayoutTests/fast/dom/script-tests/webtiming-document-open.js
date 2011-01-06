var performance = window.performance || {};
var navigation = performance.navigation || {};
var timing = performance.timing || {};
var originalTiming = {};

window.addEventListener("load", testTimingWithDocumentOpen, false);

function testTimingWithDocumentOpen()
{
    for (property in timing) {
        originalTiming[property] = timing[property];
    }

    document.open();
    document.write("<html>");
    document.write("<head>");
    document.write("<link rel=\"stylesheet\" href=\"../js/resources/js-test-style.css\">");
    document.write("<script src=\"../js/resources/js-test-pre.js\"></script>");
    document.write("</head>");
    document.write("<body>");
    document.write("<p id=\"description\"></p>");
    document.write("<div id=\"console\"></div>");
    document.write("<script src=\"../js/resources/js-test-post.js\"></script>");
    document.write("</body>");
    document.write("</html>");
    document.close();

    description("This test verifies that the NavigationTimings don't change after a document.open().");

    keys = Object.keys(timing).sort();
    for (var i = 0; i < keys.length; ++i) {
        shouldBe("timing." + keys[i], "originalTiming." + keys[i]);
    }

    finishJSTest();
}

jsTestIsAsync = true;

var successfullyParsed = true;
