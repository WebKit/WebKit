
function moveMouseToMiddleOfElement(element)
{
    eventSender.mouseMoveTo(element.offsetLeft + element.offsetWidth / 2, element.offsetTop + element.offsetHeight / 2);
}

var inputs = document.getElementsByTagName('input');
for (var i = 0; i < inputs.length; i++)
{
    inputs[i].style.fontSize = '1em';
    inputs[i].style.width = '7ex';
}

if (window.testRunner) {
    if (!window.eventSender)
        document.writeln('This test requires eventSender');
    else {
        moveMouseToMiddleOfElement(document.getElementById('from'));
        eventSender.mouseDown();

        eventSender.leapForward(200);

        moveMouseToMiddleOfElement(document.getElementById('to'));
        eventSender.mouseUp();
    }
}
