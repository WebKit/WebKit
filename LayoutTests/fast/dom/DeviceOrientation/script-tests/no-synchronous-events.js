description('Tests that events are never fired synchronously from a call to window.addEventListener().');

if (window.testRunner)
    testRunner.setMockDeviceOrientation(true, 1.1, true, 2.2, true, 3.3);
else
    debug('This test can not be run without the TestRunner');

var hasAddEventListenerReturned = false;
window.addEventListener('deviceorientation', function() {
    shouldBeTrue('hasAddEventListenerReturned');
    finishJSTest();
});
hasAddEventListenerReturned = true;

window.jsTestIsAsync = true;
