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
        isSuccessfullyParsed();
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    });
    busyWait(10);
    if (window.layoutTestController)
        layoutTestController.display();
});


if (window.layoutTestController)
    window.setTimeout(function() {
        layoutTestController.display();
    });


if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var successfullyParsed = true;
