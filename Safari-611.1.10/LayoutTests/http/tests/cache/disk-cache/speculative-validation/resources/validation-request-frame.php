<?php
header('Content-Type: text/html');
header('Cache-Control: max-age=0');
header('Etag: 123456789');
$cookie = "speculativeRequestValidation=" . uniqid();
header('Set-Cookie: ' . $cookie);
?>
<!DOCTYPE html>
<body>
<script src="request-headers-script.php"></script>
<script>
<?php
echo "sentSetCookieHeader = '" . $cookie . "';";
?>
</script>
</body>
