description("Tests requestAnimationFrame callback handling of display: property changed within another callback");

var e = document.getElementById("e");
e.style.display="none";
var callbackInvokedOnA=false;
window.webkitRequestAnimationFrame(function() {
    callbackInvokedOnA=true;
}, e);

var f = document.getElementById("f");
window.webkitRequestAnimationFrame(function() {
    e.style.display="";
}, f);

if (window.layoutTestController)
    layoutTestController.display();

shouldBeTrue("callbackInvokedOnA");

var successfullyParsed = true;
