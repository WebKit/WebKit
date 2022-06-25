function tap(x, y) {
    if (!window.eventSender)
        return;

    eventSender.addTouchPoint(x, y);
    eventSender.touchStart();
    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
}

function tapSoon(x, y) {
    setTimeout(function () {
        tap(x, y);
    }, 10);
}

function logTouch(event) {
    debug("[TouchEvent X: " + event.touches[0].pageX + " Y: " + event.touches[0].pageY + "]");
}

function debug(msg) {
    var results = document.getElementById("results");
    results.innerHTML += msg + "<br>";
}

