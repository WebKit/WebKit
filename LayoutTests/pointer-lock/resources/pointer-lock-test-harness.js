// Automatically add doNextStepButton to document for manual tests.
if (!window.testRunner) {
    setTimeout(function () {
        doNextStepButton = document.body.insertBefore(document.createElement("button"), document.body.firstChild);
        doNextStepButton.onclick = doNextStep;
        doNextStepButton.innerText = "doNextStep button for manual testing. Use keyboard to select button and press (TAB, then SPACE).";
    }, 0);
}

function doNextStep()
{
    if (typeof(currentStep) == "undefined")
        currentStep = 0;

    setTimeout(function () {
        var thisStep = currentStep++;
        if (thisStep < todo.length)
            todo[thisStep]();
        else if (thisStep == todo.length)
            setTimeout(function () { finishJSTest(); }, 0); // Deferred so that excessive doNextStep calls will be observed.
        else
            testFailed("doNextStep called too many times.");
    }, 0);
}

function doNextStepWithUserGesture()
{
    if (!window.testRunner)
        return; // Wait for human to press doNextStep button.
    doNextStep();
}

function eventExpected(eventHandlerName, message, expectedCalls, targetHanderNode)
{
    targetHanderNode[eventHandlerName] = function () {
        switch (expectedCalls--) {
        case 0:
            testFailed(eventHandlerName + " received after: " + message);
            finishJSTest();
            break;
        case 1:
            doNextStep();
        default:
            testPassed(eventHandlerName + " received after: " + message);
        };
    };
};

function expectOnlyChangeEvent(message, targetDocument) {
    debug("     " + message);
    targetDocument = targetDocument !== undefined ? targetDocument : document;
    eventExpected("onwebkitpointerlockchange", message, 1, targetDocument);
    eventExpected("onwebkitpointerlockerror", message, 0, targetDocument);
};

function expectOnlyErrorEvent(message, targetDocument) {
    debug("     " + message);
    targetDocument = targetDocument !== undefined ? targetDocument : document;
    eventExpected("onwebkitpointerlockchange", message, 0, targetDocument);
    eventExpected("onwebkitpointerlockerror", message, 1, targetDocument);
};

function expectNoEvents(message, targetDocument) {
    debug("     " + message);
    targetDocument = targetDocument !== undefined ? targetDocument : document;
    eventExpected("onwebkitpointerlockchange", message, 0, targetDocument);
    eventExpected("onwebkitpointerlockerror", message, 0, targetDocument);
};

