description('Tests that pages that use DeviceMotion are not put in the page cache.');

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.setCanOpenWindows();
    layoutTestController.overridePreference('WebKitUsesPageCachePreferenceKey', 1);
} else
    debug('This test can not be run without the LayoutTestController');

var pageOneOnloadCount = 0;
function reportPageOneOnload() {
    ++pageOneOnloadCount;
    debug('resources/cached-page-1.html onload fired, count = ' + pageOneOnloadCount);
    if (pageOneOnloadCount == 2) {
        finishJSTest();
    }
    return pageOneOnloadCount;
}

debug("Main page opening resources/cached-page-1.html");
window.open("resources/cached-page-1.html");

window.jsTestIsAsync = true;
window.successfullyParsed = true;
