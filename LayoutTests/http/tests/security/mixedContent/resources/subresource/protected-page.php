<?php
header("Cache-Control: no-store");
header("Connection: close");
if (!isset($_SERVER["PHP_AUTH_USER"])) {
    header("WWW-authenticate: Basic realm=\"" . $_SERVER["REQUEST_URI"] . "\"");
    header("HTTP/1.0 401 Unauthorized");
}
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<html>
<body>
<p><?php if (isset($_SERVER["PHP_AUTH_USER"])) { ?>
Authenticated with username <?php echo htmlspecialchars($_SERVER["PHP_AUTH_USER"]); ?>.
<?php } else { ?>
Unauthorized.
<?php } ?></p>
<script>
if (window.testRunner)
    testRunner.notifyDone();
</script>
</body>
</html>
