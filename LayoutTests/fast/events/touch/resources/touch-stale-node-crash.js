document.ontouchstart = touchStartHandler;

function touchStartHandler(e)
{
    var target = e.touches[0].target;
    document.body.removeChild(target);
    window.location = 'resources/send-touch-up.html';
}

description("If this test does not crash then you pass!");

if (window.testRunner)
    testRunner.waitUntilDone();

onload = async () => {
    await UIHelper.renderingComplete();

    if (window.eventSender) {
        eventSender.clearTouchPoints();
        eventSender.addTouchPoint(50, 150);
        await eventSender.asyncTouchStart();
    } else
        debug('This test requires DRT.');
}
