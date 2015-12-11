<?php
header("Content-Disposition: attachment; filename=test.html");
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<style>
a {
    display: block;
    width: 100vw;
    height: 100vh;
}

</style>
<a href="second-wo-referer.php">Link to second-wo-referer.php</a>