<?php
header("Content-Disposition: attachment; filename=test.html");
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<head>
<?php if (isset($_GET['referrer'])) print("<meta name=\"referrer\" content=\"" . $_GET['referrer'] . "\">\n"); ?>
<style>
a {
    display: block;
    width: 100vw;
    height: 100vh;
}
</style>
</head>
<a href="echo-http-referer.php">Link to echo-http-referer.php</a>