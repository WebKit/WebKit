<?php
if (!isset($_SERVER['PHP_AUTH_USER']))
	echo "No HTTP authentication credentials<br>";
else
	echo "Authenticated as {$_SERVER['PHP_AUTH_USER']} with password {$_SERVER['PHP_AUTH_PW']}<br>";
?>
<script>
if (window.layoutTestController)
	layoutTestController.notifyDone();
</script>
