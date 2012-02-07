function runRepaintTest()
{
    if (window.layoutTestController) {
        if (document.body)
            document.body.offsetTop;
        else if (document.rootElement)
            document.rootElement.offsetTop;

        layoutTestController.display();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}

function runSVGRepaintTest()
{
    // The SVG onload event is fired earlier than the HTML load event. To make display() reliable,
    // call it from a timer, to simulate the same fire time as the HTML load event, until it's fixed.
    if (window.layoutTestController) {
        layoutTestController.waitUntilDone();
        setTimeout(runRepaintTest, 0);
    } else
        runRepaintTest();
}
