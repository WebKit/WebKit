description("Tests basic use of requestAnimationFrame");

var e = document.getElementById("e");
var callbackInvoked = false;
window.webkitRequestAnimationFrame(function() {
    callbackInvoked = true;
}, e);

if (window.layoutTestController)
    layoutTestController.display();

shouldBeTrue("callbackInvoked");
var successfullyParsed = true;

