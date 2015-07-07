var inputEventCounter = 0;
var changeEventCounter = 0;
var testInput;

function testClearButtonChangeAndInputEvents(inputType, initialValue)
{
    description('Test for event dispatching by spin buttons in a type=' + inputType + ' input.');
    if (!window.eventSender) {
        debug('No eventSender');
        return;
    }

    testInput = document.createElement('input');
    testInput.type = inputType;
    testInput.value = initialValue;
    testInput.onchange = function() { changeEventCounter++; };
    testInput.oninput = function() { inputEventCounter++; };
    document.body.appendChild(testInput);
    var anotherInput = document.createElement('input');
    document.body.appendChild(anotherInput);

    debug('Initial state');
    eventSender.mouseMoveTo(0, 0);
    shouldEvaluateTo('changeEventCounter', 0);
    shouldEvaluateTo('inputEventCounter', 0);

    debug('Click the clear button');
    // Move the cursor on to the clear button.
    var clearButton = getElementByPseudoId(internals.oldestShadowRoot(testInput), "-webkit-clear-button");
    eventSender.mouseMoveTo(clearButton.offsetLeft + clearButton.offsetWidth / 2, clearButton.offsetTop + clearButton.offsetHeight / 2);
    eventSender.mouseDown();
    eventSender.mouseUp();
    shouldBeEqualToString('testInput.value', '');
    shouldEvaluateTo('changeEventCounter', 1);
    shouldEvaluateTo('inputEventCounter', 1);

    debug('Focus on another field');
    anotherInput.focus();
    shouldEvaluateTo('changeEventCounter', 1);
    shouldEvaluateTo('inputEventCounter', 1);

    parent.innerHTML = '';
}
