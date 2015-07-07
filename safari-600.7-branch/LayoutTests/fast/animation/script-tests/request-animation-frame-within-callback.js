description("Tests adding one callback within another");

var e = document.getElementById("e");
var sameFrame;
window.requestAnimationFrame(function() {
    sameFrame = true;
}, e);
window.requestAnimationFrame(function() {
    window.requestAnimationFrame(function() {
        shouldBeFalse("sameFrame");
    }, e);
}, e);
window.requestAnimationFrame(function() {
    sameFrame = false;
}, e);

// This should fire the three already registered callbacks, but not the one dynamically registered.
if (window.testRunner)
    testRunner.display();
// This should fire the dynamically registered callback.
if (window.testRunner)
    testRunner.display();

if (window.testRunner)
    testRunner.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}, 200);
