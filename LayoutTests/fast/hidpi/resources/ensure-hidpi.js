function runTest() {
    if (!window.testRunner)
        return;

    testRunner.waitUntilDone();
    testRunner.setBackingScaleFactor(2, scaleFactorIsSet);
}

function scaleFactorIsSet() {
  testRunner.notifyDone();
}

window.addEventListener("load", runTest, false);
