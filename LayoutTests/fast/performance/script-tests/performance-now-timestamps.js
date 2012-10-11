description("Tests the timestamps from performance.now increase and are in milliseconds");

function busyWait(millis) {
    var start = Date.now();
    while (Date.now() - start < millis) {}
}

var firstTimestamp = window.performance.now();
shouldBeDefined("firstTimestamp");
shouldBeGreaterThanOrEqual("firstTimestamp", "0");

// Verify returned value is in milliseconds since navigationStart. This
// generously assumes this JS will run within the first 5 seconds while ruling
// out the possibility that the returned value is in milliseconds since epoch.
shouldBeTrue("firstTimestamp < 5000");

var waitTime = 10;
busyWait(waitTime);

var secondTimestamp = window.performance.now();
shouldBeDefined("secondTimestamp");
shouldBeGreaterThanOrEqual("secondTimestamp", "firstTimestamp + (waitTime / 2)");

// Verify that the difference is in the milliseconds range, keeping
// the range test broad to avoid issues on overloaded bots.
var elapsed = secondTimestamp - firstTimestamp;
shouldBeGreaterThanOrEqual("elapsed", "1");
shouldBeTrue("elapsed < 100");
