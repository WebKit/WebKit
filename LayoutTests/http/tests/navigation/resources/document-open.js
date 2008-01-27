// Common testing functions for document-open history tests.

function startTest()
{
    if (window.layoutTestController) {
        layoutTestController.dumpBackForwardList();
        layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
        setTimeout("runTest()", 0);
    }
}

function stopTest()
{
    // We expect the test to set window.firstVisit to true before it open
    // the document.  When we navigate away, this variable will be cleared.
    if (window.firstVisit) {
        // This is the first time we loaded this page.
        // Navigate away and back to ensure the generated contents remain intact. 
        window.location = "resources/document-open-page-2.html";
        window.firstVisit = false;  // just to be explicit.
    } else {
        // We are now returning from page-2.  End the test.
        if (window.layoutTestController) {
            layoutTestController.notifyDone();
        }
    }
}
