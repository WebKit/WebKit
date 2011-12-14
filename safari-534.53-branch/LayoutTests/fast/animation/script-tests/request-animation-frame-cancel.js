description("This tests cancelling a webkitRequestAnimationFrame callback");

var e = document.getElementById("e");
var id = window.webkitRequestAnimationFrame(function() {
    testFailed("callback invoked after cancelled");
}, e);

window.webkitCancelRequestAnimationFrame(id);

if (window.layoutTestController)
    layoutTestController.display();

var successfullyParsed = true;
