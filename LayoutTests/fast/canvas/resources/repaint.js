function runRepaintTest()
{
    if (window.testRunner) {
        document.body.offsetTop;
        testRunner.displayAndTrackRepaints();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
