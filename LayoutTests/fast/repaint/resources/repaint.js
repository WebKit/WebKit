function runRepaintTest()
{
    if (window.layoutTestController) {
        document.body.offsetTop;
        layoutTestController.display();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
