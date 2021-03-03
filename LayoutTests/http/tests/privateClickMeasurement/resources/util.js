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
        testRunner.setPrivateClickMeasurementOverrideTimerForTesting(false);
        testRunner.setPrivateClickMeasurementAttributionReportURLForTesting("");
        testRunner.setPrivateClickMeasurementTokenSignatureURLForTesting("");
        testRunner.setPrivateClickMeasurementTokenPublicKeyURLForTesting("");
        testRunner.notifyDone();
    }
}
