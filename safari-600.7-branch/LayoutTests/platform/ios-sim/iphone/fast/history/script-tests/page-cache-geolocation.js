description('Tests that a page that makes use of simple geolocation can use the page cache.');

window.addEventListener("pageshow", function(event) {
    debug("pageshow - " + (event.persisted ? "" : "not ") + "from cache");
    if (event.persisted) {
        debug("PASS - Page did enter and was restored from the page cache");
        finishJSTest();
        window.testRunner.notifyDone();
    }
}, false);

window.addEventListener("pagehide", function(event) {
    debug("pagehide - " + (event.persisted ? "" : "not ") + "entering cache");
    if (!event.persisted) {
        debug("FAIL - Page did not enter the page cache.");
        finishJSTest();
        window.testRunner.notifyDone();
    }
}, false);

window.addEventListener('load', function() {

    // Enable the PageCache and make this an async test.
    if (window.testRunner) {
        window.testRunner.overridePreference("WebKitUsesPageCachePreferenceKey", 1);
        window.testRunner.waitUntilDone();
    }

    // Access geolocation. It is enough to create the geolocation object.
    debug(navigator.geolocation);

    // Force a back navigation back to this page.
    setTimeout(function() {
        window.location.href = "resources/page-cache-helper.html";
    }, 0);

}, false);

var successfullyParsed = true;
var jsTestIsAsync = true;
