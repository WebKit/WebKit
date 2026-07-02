// Version of text-based-repaint.js that runs the repaint test asynchronously.
if (window.testRunner)
    testRunner.waitUntilDone();

function runRepaintTest()
{
    if (window.testRunner && window.internals) {
        window.testRunner.dumpAsText(false);

        if (document.body)
            document.body.offsetTop;
        else if (document.documentElement)
            document.documentElement.offsetTop;

        window.internals.startTrackingRepaints();
        setTimeout(repaintTest, 0)
    } else {
        setTimeout(repaintTest, 100);
    }
}

function finishRepaintTest()
{
    // force a style recalc.
    var dummy = document.body.offsetTop;

    if (!window.internals)
        return;

    var repaintRects = window.internals.repaintRectsAsText();

    window.internals.stopTrackingRepaints();

    var pre = document.createElement('pre');
    document.body.appendChild(pre);
    pre.innerHTML = repaintRects;

    testRunner.notifyDone();
}
