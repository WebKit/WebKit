<html>
<body>
<script>
if (window.layoutTestController)
    layoutTestController.dumpAsText();

var accept = "<?php $headers = getallheaders(); echo $headers["Accept"]; ?>";
document.write("Accept: " + accept + "<br><br>");
if (accept.match(/application\/xhtml\+xml/)) {
    document.write("PASS: The browser asks for XHTML.");
} else {
    document.write("FAIL: The browser doesn't ask for XHTML");
}
</script>

</body>
</html>
