if (window.testRunner) {
    testRunner.setCanOpenWindows(true);
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

if (document.location.hash === "#new-window") {
    if (window.opener)
        console.log("FAIL: window.opener is non-null");
    else
        console.log("PASS: window.opener is null");
    testRunner.notifyDone();
} else {
    window.name = "originalWindow";
    document.getElementById("link").click();
}
