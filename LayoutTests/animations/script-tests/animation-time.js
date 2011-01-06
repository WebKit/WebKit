description("Tests that window.webkitAnimationTime remains the same during a script run and that all animations started within a script run are synchronized");
window.jsTestIsAsync = true;

var div1 = document.getElementById("div1");
var div2 = document.getElementById("div2");

div1.classList.add("animated");
document.body.offsetTop; // Force a style recalc to start the first animation.

var startAnimationTime = window.webkitAnimationTime;
shouldBeDefined('startAnimationTime');

var startTime = Date.now();
while (Date.now() - startTime < 10) {}

var endAnimationTime = window.webkitAnimationTime;
// Test that the webkitAnimationTime value hasn't changed out from under us.
shouldBe('startAnimationTime', 'endAnimationTime');

// Start a second declarative animation ~10ms after the first one.  They should be in sync.
div2.classList.add("animated");

window.setTimeout(function() {
    shouldBe('window.getComputedStyle(div1).left', 'window.getComputedStyle(div2).left');
    finishJSTest();
}, 50);

var successfullyParsed = true;
