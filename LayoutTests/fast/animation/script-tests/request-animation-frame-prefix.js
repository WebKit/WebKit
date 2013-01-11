description("Tests the timestamps provided to prefixed webkitRequestAnimationFrame callbacks");

var firstTimestamp = undefined;
var secondTimestamp = undefined;
var legacyFirstTimestamp = undefined;
var legacySecondTimestamp = undefined;
var deltaError = undefined;

function busyWait(millis) {
    var start = Date.now();
    while (Date.now()-start < millis) {}
}

window.requestAnimationFrame(function(timestamp) {
    firstTimestamp = timestamp;
});

window.webkitRequestAnimationFrame(function(timestamp) {
    legacyFirstTimestamp = timestamp;

    window.requestAnimationFrame(function(timestamp) {
        secondTimestamp = timestamp;
    });

    window.webkitRequestAnimationFrame(function(timestamp) {
        legacySecondTimestamp = timestamp;

        shouldBeGreaterThanOrEqual("legacyFirstTimestamp", "firstTimestamp");
        shouldBeGreaterThanOrEqual("legacySecondTimestamp", "secondTimestamp");
        deltaError = Math.abs((legacySecondTimestamp - legacyFirstTimestamp) - (secondTimestamp - firstTimestamp));
        shouldBeTrue("deltaError < 0.001");
        testRunner.notifyDone();
    });

    busyWait(10);
    if (window.testRunner)
        testRunner.display();
});

if (window.testRunner)
    window.setTimeout(function() {
        testRunner.display();
    });

if (window.testRunner)
    testRunner.waitUntilDone();
