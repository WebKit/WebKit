description("Tests that requestAnimationFrame callbacks aren't called too rapidly (above 1000FPS)");

var numCallbacks = 0;
var e = document.getElementById("e");

function next() {
    window.requestAnimationFrame(function () {
        numCallbacks++;
        next();
    }, e);
}

next();

if (window.testRunner)
    testRunner.waitUntilDone();

// Set the timeout after the first callback.
setTimeout(function() {
    isSuccessfullyParsed();
    shouldBeGreaterThanOrEqual("100", "numCallbacks");
    if (window.testRunner)
        window.testRunner.notifyDone();
}, 100);


