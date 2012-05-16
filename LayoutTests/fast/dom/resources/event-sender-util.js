//----------------------------------------------------------------------
// JavaScript Library to utilize eventSender conveniently

function mouseMoveToElem(element, pos) {
    var x = element.offsetLeft;
    var y = element.offsetTop + element.offsetHeight / 2;

    if (pos == 'left')
        x += element.offsetWidth / 4;
    else if (pos == 'right')
        x += element.offsetWidth * 3 / 4;
    else
        x += element.offsetWidth / 2;

    eventSender.mouseMoveTo(x, y);
}

function dragFromTo(elementFrom, elementTo) {
    mouseMoveToElem(elementFrom, 'left');
    eventSender.mouseDown();
    mouseMoveToElem(elementTo, 'right');
    eventSender.mouseUp();
}