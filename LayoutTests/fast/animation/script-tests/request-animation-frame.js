description("Tests basic use of requestAnimationFrame");

var e = document.getElementById("e");
var callbackInvoked = false;
window.webkitRequestAnimationFrame(function() {
    callbackInvoked = true;
}, e);

if (window.layoutTestController)
    layoutTestController.display();

setTimeout(function() {
    shouldBeTrue("callbackInvoked");
}, 100);

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

var successfullyParsed = true;

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}, 200);
