if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.setCanOpenWindows();
}

window.open("javascript:alert('FAIL')");
