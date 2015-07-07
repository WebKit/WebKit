description("Tests that requestAnimationFrame is disabled when the setting/preference is false. Note: since the setting is true by default, this usually can't be tested directly in a browser.");

var callbackInvoked = false;
window.requestAnimationFrame(function() {
    callbackInvoked = true;
});

if (window.testRunner)
    testRunner.display();

setTimeout(function() {
    shouldBeFalse("callbackInvoked");
}, 100);

if (window.testRunner)
    testRunner.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.testRunner)
        testRunner.notifyDone();
}, 200);
