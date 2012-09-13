window.jsTestIsAsync = true;

var iframe;
var testInput;

function getSpinButton(input)
{
    if (!window.internals)
        return null;
    var editElement = window.internals.oldestShadowRoot(input);
    return editElement.firstChild.lastChild;
}

function mouseClick()
{
    if (!window.eventSender)
        return;
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function mouseMoveTo(x, y)
{
    if (!window.eventSender)
        return;
    eventSender.mouseMoveTo(x, y);
}

function runIFrameLoaded(config)
{
    testInput = iframe.contentDocument.getElementById('test');
    var spinButton = getSpinButton(testInput);
    if (spinButton) {
        mouseMoveTo(
            iframe.offsetLeft + spinButton.offsetLeft + spinButton.offsetWidth / 2,
            iframe.offsetTop + spinButton.offsetTop + spinButton.offsetHeight / 4);
    }
    mouseClick();
    shouldBeEqualToString('testInput.value', config['expectedValue']);
    iframe.parentNode.removeChild(iframe);
    finishJSTest();
}

function testClickSpinButtonInIFrame(config)
{
    description('Checks mouse click on spin button in iframe.');
    if (!window.eventSender)
        debug('Please run in DumpRenderTree');

    iframe = document.createElement('iframe');
    iframe.addEventListener('load', function () { runIFrameLoaded(config) });
    iframe.srcdoc = '<input id=test type=' + config['inputType'] + ' value="' + config['initialValue'] + '">';
    document.body.appendChild(iframe);
}
