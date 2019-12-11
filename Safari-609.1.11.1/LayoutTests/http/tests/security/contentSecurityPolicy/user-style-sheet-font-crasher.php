<?php
    header("Content-Security-Policy: font-src http://webkit.org; report-uri http://webkit.org/report;");
?>
<html>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.addUserStyleSheet("@font-face { font-family: ExampleFont; src: url(example_font.woff); }", true);
}
</script>
</head>
<body>
The iframe below triggers a violation report creating the initial empty document. It should not crash the web process.<br>
<iframe src="http://127.0.0.1:8000/resources/notify-done.html"></iframe>
</body>
</html>
