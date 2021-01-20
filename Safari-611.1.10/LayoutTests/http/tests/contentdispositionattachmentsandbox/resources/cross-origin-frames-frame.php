<?php
header("Content-Disposition: attachment; filename=test.html");
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<iframe src="data:text/html,FAIL"></iframe>
