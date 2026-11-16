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

function forceLayoutAndRepaint()
{
    let paragraph = document.querySelector("p");
    paragraph.style.width = (paragraph.offsetWidth - 10) + "px";
    paragraph.style.backgroundColor = paragraph.style.backgroundColor === "green" ? "blue" : "green";
    return paragraph.offsetWidth;
}

function dispatchTestEvent()
{
    document.body.addEventListener("test-event", eventHandler, {once: true});
    document.body.dispatchEvent(new Event("test-event"));
}
