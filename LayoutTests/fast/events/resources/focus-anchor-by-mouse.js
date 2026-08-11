window.onload = function() {
    if (window.eventSender) {
        var aElement = document.getElementById('anchor');
        var aRect = aElement.getBoundingClientRect();
        eventSender.mouseMoveTo(aRect.left + 2, aRect.top + 2);
        eventSender.mouseDown();
    }
};
