<?php
header("Content-Disposition: attachment; filename=test.html");
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<style>
@import url("data:text/css,body::after { content: 'FAIL'; }");
</style>
