<!DOCTYPE html>
<html>
<head>
<script src="../../../fast/js/resources/js-test-pre.js"></script>
</head>
<body>
<script>
description('Test for wheel operations for &lt;input type=number>');
var parent = document.createElement('div');
document.body.appendChild(parent);
parent.innerHTML = '<input type=number id=number value=0> <input id=another>';
var input = document.getElementById('number');
input.focus();

function dispatchWheelEvent(element, deltaX, deltaY)
{
    var event = document.createEvent('WheelEvent');
    var dontCare = 0;
    event.initWebKitWheelEvent(deltaX, deltaY, document.defaultView, dontCare, dontCare, dontCare, dontCare, false, false, false, false);
    element.dispatchEvent(event);
}

debug('Initial value is 0. We\'ll wheel up by 1:');
dispatchWheelEvent(input, 0, 1);
shouldBe('input.value', '"1"');

debug('Wheel up by 100:');
dispatchWheelEvent(input, 0, 100);
shouldBe('input.value', '"2"');

debug('Wheel down by 1:');
dispatchWheelEvent(input, 0, -1);
shouldBe('input.value', '"1"');

debug('Wheel down by 256:');
dispatchWheelEvent(input, 0, -256);
shouldBe('input.value', '"0"');

debug('Disabled input element:');
input.disabled = true;
dispatchWheelEvent(input, 0, 1);
shouldBe('input.value', '"0"');
input.removeAttribute('disabled');

debug('Read-only input element:');
input.readOnly = true;
dispatchWheelEvent(input, 0, 1);
shouldBe('input.value', '"0"');
input.readOnly = false;

debug('No focus:');
document.getElementById('another').focus();
dispatchWheelEvent(input, 0, 1);
shouldBe('input.value', '"0"');

parent.parentNode.removeChild(parent);
</script>
<script src="../../../fast/js/resources/js-test-post.js"></script>
</body>
</html>
