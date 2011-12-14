description("Tests that pages that use Geolocation are not put in the page cache.<br><br>Currently, Geolocation does not work with the page cache so pages that use Geolocation are explicitly prevented from entering the cache. This test checks for accidental enabling of the page Cache for Geolocation. See https://bugs.webkit.org/show_bug.cgi?id=43956 for details.");

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.setCanOpenWindows();
    layoutTestController.overridePreference("WebKitUsesPageCachePreferenceKey", 1);
} else
    debug('This test can not be run without the LayoutTestController');

var pageOneOnloadCount = 0;
function reportPageOneOnload() {
    ++pageOneOnloadCount;
    debug('resources/cached-page-1.html fired, count = ' + pageOneOnloadCount);
    if (pageOneOnloadCount == 2) {
        finishJSTest();
    }
    return pageOneOnloadCount;
}

debug("Main page opening resources/cached-page-1.html");
window.open("resources/cached-page-1.html");

window.jsTestIsAsync = true;
window.successfullyParsed = true;
