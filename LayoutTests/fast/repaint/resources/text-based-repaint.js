function runRepaintTest()
{
    if (window.testRunner && window.internals) {
        window.testRunner.dumpAsText(false);

        if (document.body)
            document.body.offsetTop;
        else if (document.documentElement)
            document.documentElement.offsetTop;

        window.internals.startTrackingRepaints();

        repaintTest();

        // force a style recalc.
        var dummy = document.body.offsetTop;

        var repaintRects = window.internals.repaintRectsAsText();

        window.internals.stopTrackingRepaints();

        var pre = document.createElement('pre');
        document.body.appendChild(pre);
        pre.innerHTML = repaintRects;
    } else {
        setTimeout(repaintTest, 100);
    }
}
