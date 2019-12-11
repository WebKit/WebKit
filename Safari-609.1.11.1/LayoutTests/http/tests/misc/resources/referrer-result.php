<html>
<body>
<script>

var referer = "<?php echo $_SERVER["HTTP_REFERER"]; ?>";
if (referer.match(/referrer.html/)) {
    document.write("PASS: Referer header exists and contains the string 'referrer.html'.");
} else {
    document.write("FAIL: Referer header does not contain the string 'referrer.html', value is '" + referer + "'");
}

if (window.testRunner)
    testRunner.notifyDone();
</script>

</body>
</html>
