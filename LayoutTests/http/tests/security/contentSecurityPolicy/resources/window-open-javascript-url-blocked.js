if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.setCanOpenWindows();
    testRunner.setCloseRemainingWindowsWhenComplete(true);
}

window.open("javascript:alert('FAIL')");
