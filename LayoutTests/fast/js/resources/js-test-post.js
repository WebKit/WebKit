function finishJSTest()
{
    shouldBeTrue("successfullyParsed");
    debug('<br /><span class="pass">TEST COMPLETE</span>');
    if (window.jsTestIsAsync && window.layoutTestController)
        layoutTestController.notifyDone();
}

if (window.jsTestIsAsync) {
    if (window.layoutTestController)
        layoutTestController.waitUntilDone();
} else
    finishJSTest();
