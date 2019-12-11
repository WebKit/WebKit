if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function resizeTest(sizes, testFunction)
{
    var widthDelta;
    var heightDelta;

    function performNextResize()
    {
        if (sizes.length) {
            resizeTo(sizes[0][0] + widthDelta, sizes[0][1] + heightDelta);
            return;
        }
        removeEventListener("resize", resizeHandler);
        if (window.testRunner)
            testRunner.notifyDone();
    };

    function resizeHandler()
    {
        var nextSize = sizes.shift();

        shouldEvaluateTo("innerWidth", nextSize[0]);
        shouldEvaluateTo("innerHeight", nextSize[1]);

        testFunction();

        performNextResize();
    };

    const warmupWidth = outerWidth + 13;

    function warmupDone()
    {
        if (outerWidth != warmupWidth)
            return;
        widthDelta = outerWidth - innerWidth;
        heightDelta = outerHeight - innerHeight;
        removeEventListener("resize", warmupDone);
        setTimeout(function () {
            addEventListener("resize", resizeHandler);
            performNextResize();
        }, 0);
    }

    addEventListener("resize", warmupDone);
    setTimeout(function () {
        resizeTo(warmupWidth, outerHeight);
    }, 0);
}

function standardResizeTest(testFunction)
{
    resizeTest([[800, 600], [1200, 600]], testFunction);
}
