<html>
<body>
<script>
if (window.layoutTestController)
    layoutTestController.dumpAsText();

var userAgent = "<?php $headers = getallheaders(); echo $headers["User-Agent"]; ?>";
if (userAgent.match(/WebKit/)) {
    document.write("PASS: User-Agent header contains the string 'WebKit'.");
} else {
    document.write("FAIL: User-Agent header does not contain the string 'WebKit', value is '" + userAgent + "'");
}
</script>

</body>
</html>
