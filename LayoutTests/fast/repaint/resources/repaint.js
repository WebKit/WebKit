function runRepaintTest()
{
    if (window.layoutTestController) {
        if (document.body)
            document.body.offsetTop;
        else if (document.documentElement)
            document.documentElement.offsetTop;

        layoutTestController.display();
        repaintTest();
    } else {
        setTimeout(repaintTest, 100);
    }
}
