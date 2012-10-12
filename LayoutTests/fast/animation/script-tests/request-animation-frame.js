description("Tests basic use of requestAnimationFrame");

var e = document.getElementById("e");
var callbackInvoked = false;
window.requestAnimationFrame(function() {
    callbackInvoked = true;
}, e);

if (window.testRunner)
    testRunner.display();

setTimeout(function() {
    shouldBeTrue("callbackInvoked");
}, 100);

if (window.testRunner)
    testRunner.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}, 200);
