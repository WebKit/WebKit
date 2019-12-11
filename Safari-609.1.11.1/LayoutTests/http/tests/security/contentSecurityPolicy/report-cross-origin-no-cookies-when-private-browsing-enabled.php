<?php
    header("Content-Security-Policy: img-src 'none'; report-uri http://localhost:8080/security/contentSecurityPolicy/resources/save-report.php");
?>
<!-- webkit-test-runner [ useEphemeralSession=true ] -->
<!DOCTYPE html>
<html>
<body>
<script>
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();

    testRunner.setStatisticsShouldDowngradeReferrer(false, function () {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "http://localhost:8080/cookies/resources/setCookies.cgi", false);
        xhr.setRequestHeader("SET-COOKIE", "hello=world;path=/");
        xhr.send(null);

        // This image will generate a CSP violation report.
        let imgElement = document.createElement("img");
        imgElement.onload = imgElement.onerror = function () {
            window.location = "/security/contentSecurityPolicy/resources/echo-report.php";
        };
        imgElement.src = "/security/resources/abe.png";
        document.body.appendChild(imgElement);
    });
}
</script>
</body>
</html>
