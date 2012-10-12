description("Tests the timestamps provided to requestAnimationFrame callbacks");

function busyWait(millis) {
    var start = Date.now();
    while (Date.now()-start < millis) {}
}

var firstTimestamp = undefined;

window.requestAnimationFrame(function(timestamp) {
    firstTimestamp = timestamp;
    shouldBeDefined("firstTimestamp");
    busyWait(10);
});

var secondTimestamp = undefined;
window.requestAnimationFrame(function(timestamp) {
    secondTimestamp = timestamp;
    shouldBeDefined("secondTimestamp");
    shouldBe("firstTimestamp", "secondTimestamp");
});

if (window.testRunner)
    testRunner.display();

if (window.testRunner)
    testRunner.waitUntilDone();

setTimeout(function() {
    shouldBeDefined("firstTimestamp");
}, 100);

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}, 200);
