<?php
header("Content-type: text/html; charset=UTF-8");
?>
<html>
<head>
<title>Regression test for bug 20795</title>
</head>
<body>
<p>
This is a test for 20795, it makes sure that forms POSTed with a content-type of text/plain actually send data in text/plain
</p>
<?php
$data = file_get_contents("php://input");

if($data == "f1=This is field #1 &!@$%\n='<>\r\nf2=This is field #2 \"\"") {
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
