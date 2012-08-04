<?php
header("Content-type: text/html; charset=UTF-8");
?>
<html>
<head>
<title>Test for bug 19079</title>
</head>
<body>
<p>
This is a test for https://bugs.webkit.org/show_bug.cgi?id=19079, it send the submissions
character encoding in hidden _charset_ field.
</p>

<?php
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
$data = file_get_contents("php://input");
if ($data != "")
echo "<p>PASSED: $data </p>";
else
echo "<p>PASSED: No _charset_ field </p>";
}
if ($_SERVER['REQUEST_METHOD'] == 'GET') {
$data = htmlentities($_GET['_charset_']);
echo "<p>PASSED: $data </p>";
}
?>

<script>
if(window.testRunner)
    testRunner.notifyDone();
</script>

</body>
</html>
