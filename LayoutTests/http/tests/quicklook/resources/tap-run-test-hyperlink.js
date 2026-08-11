function runTest(frame, offsetInFrame, runAsync)
{
    if (!window.testRunner)
        return;

    offsetInFrame = offsetInFrame || 5;

    // FIXME: For some reason the (x, y) coordinates of the hyperlink "Run test" is offset 2 pixels
    // in legacy WebKit (why?).
    var fudgeFactor = UIHelper.isWebKit2() ? 0 : 2;
    var promise = UIHelper.activateAt(frame.offsetLeft + offsetInFrame + fudgeFactor, frame.offsetTop + offsetInFrame + fudgeFactor);
    if (runAsync)
        return promise;

    UIHelper.wait(promise);
}
