if (window.layoutTestController) {
    layoutTestController.dumpAsText();
    layoutTestController.waitUntilDone();
}

window.onload = function () {
    window.location = "/security/contentSecurityPolicy/resources/echo-report.php";
}
