<?php
header("Content-type: text/html; charset=UTF-8");
?>
<html>
<head>
<title>Regression test for bug 20795 and 100445</title>
</head>
<body>
<p>
This is a test for 20795 and 100445, it makes sure that forms POSTed with a content-type of text/plain actually send data in text/plain
</p>
<?php

$content_type = $_SERVER["CONTENT_TYPE"];

if ($content_type == "text/plain") {
    echo "<p>SUCCESS: Content-type is text/plain.</p>";
} else {
    echo "<p>FAIL: Content-type should be text/plain, but was '$content_type'</p>";
}

$data = file_get_contents("php://input");

if($data == "f1=This is field #1 &!@$%\r\n='<>\r\nf2=This is field #2 \"\"") {
    echo "<p>SUCCESS</p>";
} else {
    echo "<p>FAILURE: $data</p>";
}
?>
<script>
if(window.testRunner)
    testRunner.notifyDone();
</script>
</body>
</html>
