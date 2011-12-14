description("Tests one requestAnimationFrame callback cancelling a second");

var e = document.getElementById("e");
var secondCallbackId;

window.webkitRequestAnimationFrame(function() {
    window.webkitCancelRequestAnimationFrame(secondCallbackId);
}, e);

secondCallbackId = window.webkitRequestAnimationFrame(function() {
    testFailed("callback invoked after cancel");
}, e);

if (window.layoutTestController)
    layoutTestController.display();

var successfullyParsed = true;
