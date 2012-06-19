description("This tests cancelling a webkitRequestAnimationFrame callback");

var callbackFired = false;
var e = document.getElementById("e");
var id = window.webkitRequestAnimationFrame(function() {
}, e);

window.webkitCancelAnimationFrame(id);

if (window.testRunner)
    testRunner.display();

setTimeout(function() {
    shouldBeFalse("callbackFired");
}, 100);

if (window.testRunner)
    testRunner.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}, 200);
