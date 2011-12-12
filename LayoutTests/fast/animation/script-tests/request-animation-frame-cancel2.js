description("Tests one requestAnimationFrame callback cancelling a second");

var e = document.getElementById("e");
var secondCallbackId;
var callbackFired = false;
var cancelFired = false;

window.webkitRequestAnimationFrame(function() {
    cancelFired = true;
    window.webkitCancelAnimationFrame(secondCallbackId);
}, e);

secondCallbackId = window.webkitRequestAnimationFrame(function() {
    callbackFired = true;
}, e);

if (window.layoutTestController)
    layoutTestController.display();

setTimeout(function() {
    shouldBeFalse("callbackFired");
    shouldBeTrue("cancelFired");
}, 100);

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}, 200);
