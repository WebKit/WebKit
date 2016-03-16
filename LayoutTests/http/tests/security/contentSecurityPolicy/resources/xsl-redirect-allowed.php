<?php
header("Content-Type: application/xhtml+xml");
header("Content-Security-Policy: script-src http://127.0.0.1:8000/resources/redirect.php http://localhost:8000 'unsafe-inline'");
echo '<?xml version="1.0" encoding="UTF-8"?>' . "\n";
echo '<?xml-stylesheet type="text/xsl" href="http://127.0.0.1:8000/resources/redirect.php?code=307&amp;url=http%3A%2F%2Flocalhost%3A8000/security/contentSecurityPolicy/resources/alert-pass.xsl"?>' . "\n";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
</head>
<body>
</body>
</html>
