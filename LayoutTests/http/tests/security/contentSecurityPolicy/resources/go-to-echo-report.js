if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

window.onload = function () {
    window.location = "/security/contentSecurityPolicy/resources/echo-report.php";
}
