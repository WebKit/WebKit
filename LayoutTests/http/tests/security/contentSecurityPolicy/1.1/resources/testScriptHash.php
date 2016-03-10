<?php
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-cache, must-revalidate");
    header("Pragma: no-cache");
    header("Content-Type: text/html; charset=" . (empty($_GET["charset"]) ? "UTF8" : $_GET["charset"]));
    header("Content-Security-Policy: script-src 'self' " . $_GET["hashSource"]);
?>
<!DOCTYPE html>
<html>
<head>
<script src="didRunInlineScriptPrologue.js"></script>
<script><?php echo $_GET["script"]; ?></script> <!-- Will only execute if $_GET["hashSource"] represents a valid hash of this script. -->
<script src="didRunInlineScriptEpilogue.js"></script>
</head>
</html>
