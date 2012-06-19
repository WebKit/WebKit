jsTestIsAsync = true;

description("Tests the timestamps provided to requestAnimationFrame callbacks advance");

function busyWait(millis) {
    var start = Date.now();
    while (Date.now()-start < millis) {}
}

var firstTimestamp = undefined;
var secondTimestamp = undefined;

window.webkitRequestAnimationFrame(function(timestamp) {
    firstTimestamp = timestamp;
    shouldBeDefined("firstTimestamp");
    window.webkitRequestAnimationFrame(function(timestamp) {
        secondTimestamp = timestamp;
        shouldBeDefined("secondTimestamp");
        shouldBeTrue("secondTimestamp > firstTimestamp");
        finishJSTest();
    });
    busyWait(10);
    if (window.testRunner)
        testRunner.display();
});


if (window.testRunner)
    window.setTimeout(function() {
        testRunner.display();
    });
