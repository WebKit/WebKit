<html>
<body>
<script>
if (window.layoutTestController)
    layoutTestController.dumpAsText();

var userAgent = "<?php echo $_SERVER['HTTP_USER_AGENT']; ?>";
if (userAgent.match(/WebKit/)) {
    document.write("PASS: User-Agent header contains the string 'WebKit'.");
} else {
    document.write("FAIL: User-Agent header does not contain the string 'WebKit', value is '" + userAgent + "'");
}
</script>

</body>
</html>
