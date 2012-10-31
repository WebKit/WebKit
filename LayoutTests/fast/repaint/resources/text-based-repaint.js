function runRepaintTest()
{
    if (window.testRunner && window.internals) {
        window.testRunner.dumpAsText(false);

        if (document.body)
            document.body.offsetTop;
        else if (document.documentElement)
            document.documentElement.offsetTop;

        window.internals.startTrackingRepaints(document);

        repaintTest();

        // force a style recalc.
        var dummy = document.body.offsetTop;

        var repaintRects = window.internals.repaintRectsAsText(document);

        window.internals.stopTrackingRepaints(document);

        var pre = document.createElement('pre');
        document.body.appendChild(pre);
        pre.innerHTML = repaintRects;
    } else {
        setTimeout(repaintTest, 100);
    }
}
