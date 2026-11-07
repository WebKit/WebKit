function timerFired()
{
    return 42;
}

function fireTimer()
{
    setTimeout(timerFired);
}

function eventHandler()
{
    return 42;
}

function dispatchTestEvent()
{
    document.body.addEventListener("test-event", eventHandler, {once: true});
    document.body.dispatchEvent(new Event("test-event"));
}
