description("Tests that requestAnimationFrame is disabled when the setting/preference is false. Note: since the setting is true by default, this usually can't be tested directly in a browser.");

var callbackInvoked = false;
window.webkitRequestAnimationFrame(function() {
    callbackInvoked = true;
});

if (window.layoutTestController)
    layoutTestController.display();

setTimeout(function() {
    shouldBeFalse("callbackInvoked");
}, 100);

if (window.layoutTestController)
    layoutTestController.waitUntilDone();

setTimeout(function() {
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}, 200);
