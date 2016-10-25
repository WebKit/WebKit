if (window.testRunner) {
    testRunner.setCanOpenWindows(true);
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

if (document.location.hash === "#new-window") {
    var console = window.open("", "originalWindow").document.getElementById("console");
    if (window.opener)
        console.innerText = "FAIL: window.opener is non-null";
    else
        console.innerText = "PASS: window.opener is null";
    testRunner.notifyDone();
} else {
    window.name = "originalWindow";
    document.getElementById("link").click();
}
