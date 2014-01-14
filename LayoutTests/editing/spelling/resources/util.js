function log(msg)
{
    document.getElementById("console").innerHTML += (msg + "\n");
}

function initSpellTest(testElementId, testText, testFunction)
{
    if (!window.internals || !window.testRunner) {
        log("FAIL Incomplete test environment");
        return;
    }
    testFunctionCallback = testFunction;
    jsTestIsAsync = true;

    internals.setAutomaticSpellingCorrectionEnabled(false);

    internals.settings.setAsynchronousSpellCheckingEnabled(true);
    internals.settings.setSmartInsertDeleteEnabled(true);
    internals.settings.setSelectTrailingWhitespaceEnabled(false);
    internals.settings.setUnifiedTextCheckerEnabled(true);

    var destination = document.getElementById(testElementId);
    destination.focus();
    document.execCommand("InsertText", false, testText);
    shouldBecomeDifferent('internals.markerCountForNode(destination.childNodes[0], "spelling")', '0', function() {
        testFunctionCallback(destination.childNodes[0]);
    });
}
