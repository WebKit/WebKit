<?php
header("Content-Type: text/html; charset=UTF-8");
?>
<html>
<head>
<script>
onload = function() {
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
</head>
<body>
<h1>This tests verifies the test file included in the multipart form data:</h1>
<pre>
<?php
if (isset($_FILES['data'])) {
    print "PASS: File is present\n";
    if ($_FILES['data']['error'] == UPLOAD_ERR_OK)
        print "PASS: File upload was successful\n";
    else
        print "FAIL: File upload failed\n";

    if ($_FILES['data']['size'] > 0)
        print "PASS: File size is greater than 0\n";
    else
        print "FAIL: File size is 0\n";
} else {
    print "FAIL: File is missing";
}
?>
</pre>
</body>
</html>
