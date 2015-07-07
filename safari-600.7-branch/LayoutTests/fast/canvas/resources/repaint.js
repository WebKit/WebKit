function runRepaintTest()
{
    if (window.testRunner) {
        document.body.offsetTop;
        testRunner.display();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
