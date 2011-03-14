description("Tests the assertion that the GeolocationClient should not be updating<br>" +
            "when the GeolocationController is destroyed.<br>" +
            "See https://bugs.webkit.org/show_bug.cgi?id=52216");

var otherWindow;

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.setCanOpenWindows();
    layoutTestController.setCloseRemainingWindowsWhenComplete(true);
} else
    testFailed('This test can not be run without the LayoutTestController');

function gotPosition(p)
{
    testPassed("Received Geoposition.");
    otherWindow.close();
    window.setTimeout(waitForWindowToClose, 0);
}

function waitForWindowToClose()
{
    if (!otherWindow.closed) {
        window.setTimeout(waitForWindowToClose, 0);
        return;
    }
    testPassed("Success - no crash!");
    finishJSTest();
}

debug("Main page opening resources/window-close-popup.html");
otherWindow = window.open("resources/window-close-popup.html");

window.jsTestIsAsync = true;
window.successfullyParsed = true;
