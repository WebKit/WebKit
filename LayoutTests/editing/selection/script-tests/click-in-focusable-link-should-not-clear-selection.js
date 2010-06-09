description('Ensures that the selection is not cleared when the user does a mousedown on a mouse focusable link.')

function getElementCenter(el)
{
    var rect = el.getBoundingClientRect();
    var x = rect.left + rect.width / 2;
    var y = rect.top + rect.height / 2;
    return {x: x, y: y};
}

function doubleClickOnElement(el)
{
    if (window.eventSender) {
        var point = getElementCenter(el);
        eventSender.mouseMoveTo(point.x, point.y);
        eventSender.mouseDown();
        eventSender.mouseUp();
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

function mouseDownOnElement(el)
{
    if (window.eventSender) {
        var point = getElementCenter(el);
        eventSender.mouseMoveTo(point.x, point.y);
        eventSender.mouseDown();
    }
}

function selectionShouldBe(s)
{
    shouldBeEqualToString('String(window.getSelection())', s);
}

var span = document.createElement('span');
span.textContent = 'Select';
document.body.appendChild(span);

var br = document.createElement('br');
document.body.appendChild(br);

var a = document.createElement('a');
a.textContent = 'link';
a.href = '#';
a.tabIndex = -1;
document.body.appendChild(a);

if (window.eventSender) {
    doubleClickOnElement(span);
    selectionShouldBe('Select');

    mouseDownOnElement(a);
    selectionShouldBe('Select');

    document.body.removeChild(a);
    document.body.removeChild(br);
    document.body.removeChild(span);
}

var successfullyParsed = true;
