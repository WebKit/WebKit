description("Tests that when a request is made on a Geolocation object after it has been adopted into " +
            "a different page, position unavailable errors are sent to the watches of the original page, and that " +
            "watches created after the adoption do receive position updates.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

if (window.layoutTestController) {
    layoutTestController.setCanOpenWindows();
}

window.jsTestIsAsync = true;

var window2, iframe;

function window2Loaded() {
    testPassed("Loaded window 2.");

    // Configure the mock after window has been created, so that it will apply to all windows.
    if (layoutTestController) {
        layoutTestController.setGeolocationPermission(true);
        layoutTestController.setMockGeolocationPosition(mockLatitude, mockLongitude, mockAccuracy);
    }

    // Start the first watch using the second page's iframe's geolocation.
    iframe = window2.document.getElementById("iframe");
    iframe.contentWindow.navigator.geolocation.watchPosition(firstWatchUpdate, firstWatchError);
}

var numPositionsReceived = 0;
function firstWatchUpdate() {
    numPositionsReceived++;
    if (numPositionsReceived > 1) {
        testFailed("Should not have received position update after frame reparenting.");
        return;
    }

    testPassed("First watch received position update.");

    // Reparent the iframe
    window.document.adoptNode(iframe);
    window.document.body.appendChild(iframe);
    testPassed("Adopted iframe from other page.");

    // Add another watch
    iframe.contentWindow.navigator.geolocation.watchPosition(secondWatchUpdate, secondWatchError);

    // Send through a fresh position
    if (layoutTestController)
        layoutTestController.setMockGeolocationPosition(++mockLatitude, ++mockLongitude, ++mockAccuracy);
}

var receivedUpdate = false;
function secondWatchUpdate() {
    testPassed("Second watch received position update.");

    receivedUpdate = true;
    maybeFinish();
}

var receivedError = false;
function firstWatchError(e) {
    if (numPositionsReceived == 0) {
        testFailed("First watch should not received any errors.");
        finishJSTest();
        return;
    }

    receivedError = true;
    maybeFinish();
}

function secondWatchError(e) {
    testFailed("Second watch should not received any errors.");
    finishJSTest();
}

function maybeFinish() {
    if (receivedUpdate && receivedError) {
        testPassed("First watch received error and second watch received update.");
        finishJSTest();
    }
}

window2 = window.open("resources/iframe-reparent-page.html");
window2.addEventListener("load", window2Loaded, false);

window.successfullyParsed = true;
