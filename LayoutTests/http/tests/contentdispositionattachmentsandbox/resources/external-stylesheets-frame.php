<?php
header("Content-Disposition: attachment; filename=test.html");
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<link rel="stylesheet" href="data:text/css,body::after { content: 'FAIL'; }">
