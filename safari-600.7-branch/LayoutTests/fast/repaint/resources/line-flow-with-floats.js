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
    if (window.testRunner) {
        document.body.offsetTop;
        testRunner.display();
        test(document.getElementById("iframe").contentDocument);
    } else setTimeout(
        function() {
            test(document.getElementById("iframe").contentDocument);
        },
        10
    );
}
