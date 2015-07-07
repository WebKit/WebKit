window.performance = window.performance || {};
var navigation = performance.navigation || {};
var timing = performance.timing || {};
var originalTiming = {};

window.addEventListener("load", function() { setTimeout(testTimingWithDocumentOpen, 0); }, false);

function testTimingWithDocumentOpen()
{
    for (property in timing) {
        originalTiming[property] = timing[property];
    }

    document.open();
    document.write("<html>");
    document.write("<head>");
    document.write("<script src=\"../../resources/js-test-pre.js\"></script>");
    document.write("</head>");
    document.write("<body>");
    document.write("<script src=\"../../resources/js-test-post.js\"></script>");
    document.write("</body>");
    document.write("</html>");
    document.close();

    description("This test verifies that the NavigationTimings don't change after a document.open().");

    setTimeout(finishTest, 0);
}

function finishTest() {
    var keys = [];
    for (var key in timing)
        keys.push(key);
    keys.sort();

    for (var i = 0; i < keys.length; ++i) {
        shouldBe("timing." + keys[i], "originalTiming." + keys[i]);
    }

    finishJSTest();
}

jsTestIsAsync = true;
