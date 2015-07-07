<html>
<body>
<script>
if (window.testRunner)
    testRunner.dumpAsText();

var accept = "<?php echo $_SERVER["HTTP_ACCEPT"]; ?>";
document.write("Accept: " + accept + "<br><br>");
if (accept.match(/application\/xhtml\+xml/)) {
    document.write("PASS: The browser asks for XHTML.");
} else {
    document.write("FAIL: The browser doesn't ask for XHTML");
}
</script>

</body>
</html>
