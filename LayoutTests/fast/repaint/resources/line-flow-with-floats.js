var frameDoc;
var loadCount = 0;

function loaded()
{
    loadCount++;
    if (loadCount == 2) {
        document.body.offsetTop;
        beginTest();
    }
}

function beginTest()
{
    if (window.layoutTestController) {
        document.body.offsetTop;
        layoutTestController.display();
        test(document.getElementById("iframe").contentDocument);
    } else setTimeout(
        function() {
            test(document.getElementById("iframe").contentDocument);
        },
        10
    );
}
