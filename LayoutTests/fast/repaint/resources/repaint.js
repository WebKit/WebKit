function runRepaintTest()
{
    if (window.testRunner) {
        if (document.body)
            document.body.offsetTop;
        else if (document.documentElement)
            document.documentElement.offsetTop;

        testRunner.display();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
