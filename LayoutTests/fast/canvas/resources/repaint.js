async function runRepaintTest()
{
    if (window.testRunner) {
        document.body.offsetTop;
        await testRunner.displayAndTrackRepaints();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
