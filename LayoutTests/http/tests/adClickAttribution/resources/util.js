function prepareTest() {
    if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.dumpChildFramesAsText();
        testRunner.dumpAsText();
        testRunner.setAllowsAnySSLCertificate(true);
    }
}

function tearDownAndFinish() {
    if (window.testRunner) {
        testRunner.setAllowsAnySSLCertificate(false);
        testRunner.setAdClickAttributionOverrideTimerForTesting(false);
        testRunner.setAdClickAttributionConversionURLForTesting("");
        testRunner.notifyDone();
    }
}
