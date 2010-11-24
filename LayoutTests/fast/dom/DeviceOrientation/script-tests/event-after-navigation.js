description('Tests for a crash where an event is fired after the page has been navigated away when the original page is in the page cache.');

if (window.layoutTestController)
    layoutTestController.overridePreference('WebKitUsesPageCachePreferenceKey', 1);
else
    debug('This test can not be run without the LayoutTestController');

document.body.onload = function() {
    window.addEventListener('deviceorientation', function() { } );
    window.location = "resources/event-after-navigation-new.html";
}

window.jsTestIsAsync = true;
window.successfullyParsed = true;
