async function tap(x, y) {
    if (!window.eventSender)
        return;

    eventSender.addTouchPoint(x, y);
    await eventSender.asyncTouchStart();
    eventSender.releaseTouchPoint(0);
    await eventSender.asyncTouchEnd();
}

function tapSoon(x, y) {
    setTimeout(async function () {
        await tap(x, y);
    }, 10);
}

function logTouch(event) {
    debug("[TouchEvent X: " + event.touches[0].pageX + " Y: " + event.touches[0].pageY + "]");
}

function debug(msg) {
    var results = document.getElementById("results");
    results.innerHTML += msg + "<br>";
}

