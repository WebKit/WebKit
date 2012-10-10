var inputEventCounter = 0;
var changeEventCounter = 0;
var testInput;

function testSpinButtonChangeAndInputEvents(inputType, initialValue, expectedValue, maximumValue)
{
    description('Test for event dispatching by spin buttons in a type=' + inputType + ' input.');
    if (!window.eventSender) {
        debug('No eventSender');
        return;
    }

    var parent = document.createElement('div');
    document.body.appendChild(parent);
    parent.innerHTML = '<input id=test><input id=another>';
    testInput = document.getElementById('test');
    var anotherInput = document.getElementById('another');

    testInput.type = inputType;
    if (maximumValue != undefined)
        testInput.setAttribute("max", maximumValue);
    testInput.setAttribute("value", initialValue);
    testInput.onchange = function() { changeEventCounter++; };
    testInput.oninput = function() { inputEventCounter++; };

    debug('Initial state');
    eventSender.mouseMoveTo(0, 0);
    shouldEvaluateTo('changeEventCounter', 0);
    shouldEvaluateTo('inputEventCounter', 0);

    debug('Click the upper button');
    // Move the cursor on the upper button.
    var spinButton = getElementByPseudoId(internals.oldestShadowRoot(testInput), "-webkit-inner-spin-button");
    eventSender.mouseMoveTo(testInput.offsetLeft + spinButton.offsetLeft, testInput.offsetTop + testInput.offsetHeight / 4);
    eventSender.mouseDown();
    eventSender.mouseUp();
    shouldBeEqualToString('testInput.value', expectedValue);
    shouldEvaluateTo('changeEventCounter', 1);
    shouldEvaluateTo('inputEventCounter', 1);

    if (testInput.hasAttribute("max")) {
        debug('Click again, but the value is not changed.');
        eventSender.mouseDown();
        eventSender.mouseUp();
        shouldBeEqualToString('testInput.value', expectedValue);
        shouldEvaluateTo('changeEventCounter', 1);
        shouldEvaluateTo('inputEventCounter', 1);
    }

    debug('Focus on another field');
    anotherInput.focus();
    shouldEvaluateTo('changeEventCounter', 1);
    shouldEvaluateTo('inputEventCounter', 1);

    parent.innerHTML = '';
}
