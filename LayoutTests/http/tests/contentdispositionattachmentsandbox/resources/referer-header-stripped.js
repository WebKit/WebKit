if (window.internals)
    internals.settings.setContentDispositionAttachmentSandboxEnabled(true);

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
    if (testRunner.setShouldDownloadContentDispositionAttachments)
        testRunner.setShouldDownloadContentDispositionAttachments(false);
}

onload = async function() {
    // Due to the sandbox, it's not possible to run script in the iframe or even access its contentDocument.
    var element = document.getElementsByTagName("iframe")[0];
    var x = element.offsetLeft + 10;
    var y = element.offsetTop + 10;

    if (window.testRunner) {
        if (testRunner.isIOSFamily)
            testRunner.runUIScript("(function() { uiController.singleTapAtPoint(" + x + ", " + y + "); })()");
        else if (window.eventSender) {
            await eventSender.asyncMouseMoveTo(x, y);
            await eventSender.asyncMouseDown();
            await eventSender.asyncMouseUp();    
        }
    }
}
