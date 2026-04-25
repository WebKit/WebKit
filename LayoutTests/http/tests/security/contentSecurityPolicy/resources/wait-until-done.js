if (window.testRunner)
    testRunner.waitUntilDone();

function alertAndDone(message)
{
    alert(message);
    if (window.testRunner)
        testRunner.notifyDone();
}
