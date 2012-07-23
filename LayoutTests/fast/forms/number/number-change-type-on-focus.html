<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<script src="../../../fast/js/resources/js-test-pre.js"></script>
</head>
<body>
<p id="description"></p>

<input id=number type=number value=0>

<div id="console"></div>
<script>
description('Bug 46950: Assertion failure by changing type from type=number in focus event.');

var input = document.getElementById('number');
var spinX = input.offsetLeft + input.offsetWidth - 6;
var middleX = input.offsetLeft + input.offsetWidth / 2
var middleY = input.offsetTop + input.offsetHeight / 4;

function changeType(event) {
    this.type = 'text';
}
input.addEventListener('focus', changeType, false);
if (window.eventSender) {
    // Click the spin button.
    eventSender.mouseMoveTo(spinX, middleY);
    eventSender.mouseDown(); // This made an assertion fail.
    eventSender.mouseUp();
} else {
    debug('Manual testing: Click the spin button, and see if the browser crashes or not.');
}
testPassed('Not crashed.');

// Click the input element. The event should not be captured by the spin button.
if (window.eventSender) {
    var mouseDownCount = 0;
    input.addEventListener('mousedown', function(event) {
        mouseDownCount++;
    }, false);
    eventSender.mouseMoveTo(middleX, middleY);
    eventSender.mouseDown();
    eventSender.mouseUp();
    shouldBe('mouseDownCount', '1');
}
</script>
<script src="../../../fast/js/resources/js-test-post.js"></script>
</body>
</html>
