description('Tests that events are never fired sycnhronously from a call to window.addEventListener().');

if (window.layoutTestController)
    layoutTestController.setMockDeviceOrientation(true, 1.1, true, 2.2, true, 3.3);
else
    debug('This test can not be run without the LayoutTestController');

var hasAddEventListenerReturned = false;
window.addEventListener('deviceorientation', function() {
    shouldBeTrue('hasAddEventListenerReturned');
    finishJSTest();
});
hasAddEventListenerReturned = true;

window.jsTestIsAsync = true;
window.successfullyParsed = true;
