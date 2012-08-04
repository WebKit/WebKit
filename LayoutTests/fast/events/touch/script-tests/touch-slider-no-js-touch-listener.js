var div = document.createElement("div");
div.style.width = "200%";
var slider = document.createElement("input");
slider.setAttribute("type", "range");
slider.style.width = "200px";
slider.style.height = "30px";

document.body.insertBefore(div, document.getElementById('console'));
div.appendChild(slider);


var checkPosition = (function() {

  var nCheck = 0;

  var expectedPositions = [50, 0];

  return function() {
    shouldBeEqualToString("slider.value", String(expectedPositions[nCheck++]));
  };

})();

function onKeyDown() {
    checkPosition();
    testRunner.notifyDone();
    isSuccessfullyParsed();
}

document.addEventListener('keydown', onKeyDown);

description("Tests to ensure that touch events are delivered to an input element with type=range even when there are no touch event handlers in Javascript. This test is only expected to pass if ENABLE_TOUCH_SLIDER is defined.");

if (window.testRunner) {
    testRunner.waitUntilDone();
}

if (window.eventSender) {
    var x = slider.offsetLeft;
    var y = slider.offsetTop + slider.clientHeight/2;
    var w = slider.clientWidth;

    checkPosition();

    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(x + w/2, y);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, x, y);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();

    eventSender.keyDown(' ');

} else {
    debug('This test requires DRT.');
}
