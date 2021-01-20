function testLosingRendererOnClick()
{
    if (window.testRunner)
        testRunner.dumpAsText();
    var input = document.getElementById("test");
    var x = input.offsetLeft + input.offsetWidth - 8;
    var y = input.offsetTop + input.offsetHeight - 7;
    if (window.eventSender) {
        eventSender.mouseMoveTo(x, y);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}
