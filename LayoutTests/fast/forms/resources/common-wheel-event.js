function dispatchWheelEvent(element, deltaX, deltaY)
{
    var event = document.createEvent('WheelEvent');
    var dontCare = 0;
    event.initWebKitWheelEvent(deltaX, deltaY, document.defaultView, dontCare, dontCare, dontCare, dontCare, false, false, false, false);
    element.dispatchEvent(event);
}

var input;
function testWheelEvent(parameters)
{
    var inputType = parameters['inputType'];
    var initialValue = parameters['initialValue'];
    var stepUpValue1 = parameters['stepUpValue1'];
    var stepUpValue2 = parameters['stepUpValue2'];
    description('Test for wheel operations for &lt;input type=' + inputType + '>');
    var parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input type=' + inputType + ' id=test value="' + initialValue + '"> <input id=another>';
    input = document.getElementById('test');
    input.focus();

    debug('Initial value is ' + initialValue + '. We\'ll wheel up by ' + stepUpValue1 + ':');
    dispatchWheelEvent(input, 0, 1);
    shouldBeEqualToString('input.value', stepUpValue1);

    debug('Wheel up by 100:');
    dispatchWheelEvent(input, 0, 100);
    shouldBeEqualToString('input.value', stepUpValue2);

    debug('Wheel down by 1:');
    dispatchWheelEvent(input, 0, -1);
    shouldBeEqualToString('input.value', stepUpValue1);

    debug('Wheel down by 256:');
    dispatchWheelEvent(input, 0, -256);
    shouldBeEqualToString('input.value', initialValue);

    debug('Disabled input element:');
    input.disabled = true;
    dispatchWheelEvent(input, 0, 1);
    shouldBeEqualToString('input.value', initialValue);
    input.removeAttribute('disabled');


    debug('Read-only input element:');
    input.readOnly = true;
    dispatchWheelEvent(input, 0, 1);
    shouldBeEqualToString('input.value', initialValue);
    input.readOnly = false;

    debug('No focus:');
    document.getElementById('another').focus();
    dispatchWheelEvent(input, 0, 1);
    shouldBeEqualToString('input.value', initialValue);

    parent.parentNode.removeChild(parent);
}
