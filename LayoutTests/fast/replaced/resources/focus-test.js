if (window.layoutTestController)
    layoutTestController.waitUntilDone(), layoutTestController.dumpAsText();

function checkNoFocusRing(element, event)
{
    var color = getComputedStyle(element, null).getPropertyValue('outline-color');
    var style = getComputedStyle(element, null).getPropertyValue('outline-style');
    var width = getComputedStyle(element, null).getPropertyValue('outline-width');

    var noFocusRing = (width == '0px') && (style == 'none');

    document.body.insertAdjacentHTML('beforeEnd', '<BR>' + element.tagName +
        ' Event: ' +  event.type);
    document.body.insertAdjacentHTML('beforeEnd', noFocusRing ?
        ' PASS' : ' FAIL: focus style ' + [width, style, color].join(' '));

    if (window.layoutTestController)
        window.layoutTestController.notifyDone();
}

var element = document.getElementById('test');
element.onfocus = function() { setTimeout(checkNoFocusRing, 50, element, event) };

if (window.layoutTestController) {
    eventSender.mouseMoveTo(element.offsetLeft + 5, element.offsetTop + 5);
    eventSender.mouseDown();
    eventSender.mouseUp();
}
