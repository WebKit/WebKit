//----------------------------------------------------------------------
// JavaScript Library to utilize eventSender conveniently

function mouseMoveToElem(element) {
    var x = element.offsetLeft + element.offsetWidth / 2;
    var y = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(x, y);
}