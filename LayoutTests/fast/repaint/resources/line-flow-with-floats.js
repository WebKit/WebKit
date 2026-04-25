var frameDoc;
var loadCount = 0;

async function loaded()
{
    testRunner?.waitUntilDone();
    loadCount++;
    if (loadCount == 2) {
        document.body.offsetTop;
        await beginTest();
    }
    testRunner?.notifyDone();
}

async function beginTest()
{
    if (window.testRunner) {
        document.body.offsetTop;
        await testRunner.displayAndTrackRepaints();
        test(document.getElementById("iframe").contentDocument);
    } else setTimeout(
        function() {
            test(document.getElementById("iframe").contentDocument);
        },
        10
    );
}
