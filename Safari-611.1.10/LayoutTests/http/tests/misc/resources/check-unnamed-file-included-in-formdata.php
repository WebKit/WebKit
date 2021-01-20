<?php
header("Content-Type: text/html; charset=UTF-8");
?>
<html>
<head>
<script>
    if (window.testRunner)
        testRunner.notifyDone();
</script>
</head>
<body>
<h1>This tests verifies that files without a name are included in the multipart form data:</h1>
<pre>
<?php
if (isset($_FILES['data'])) {
    print "Test passed.";
} else {
    print "Test failed.";
}
?>
</pre>
</body>
</html>
