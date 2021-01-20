function selectListItems(startSelectionElement, endSelectionElement, endSelectionElementOffset) {

    var start = document.getElementById(startSelectionElement);
    var end = document.getElementById(endSelectionElement);

    var selection = window.getSelection();
    selection.setBaseAndExtent(start, 0, end, endSelectionElementOffset);
}

function dragSelectionToTarget(startSelectionElement, targetElement) {

    var start = document.getElementById(startSelectionElement);
    var startx = start.offsetLeft;
    var starty = start.offsetTop + start.offsetHeight / 2;

    eventSender.mouseMoveTo(startx, starty);
    eventSender.mouseDown();
    eventSender.leapForward(200);

    var target = document.getElementById(targetElement);
    var targetx = target.parentNode.offsetLeft + target.offsetLeft + target.offsetWidth / 2;
    var targety = target.offsetTop + target.offsetHeight / 2;

    eventSender.mouseMoveTo(targetx, targety);
    eventSender.mouseUp();

    window.getSelection().collapse(null);
}
