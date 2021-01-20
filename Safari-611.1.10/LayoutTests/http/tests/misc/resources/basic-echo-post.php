<?php
header("Cache-Control: no-store");
header("Connection: close");
if (!isset($_SERVER['PHP_AUTH_USER']) 
    || !isset($_SERVER['PHP_AUTH_PW'])) {
    header("WWW-Authenticate: Basic realm=\"TestRealm\"");
    header('HTTP/1.0 401 Unauthorized');
    echo "Please send a username and password";
    exit;
}

$myfile = fopen($_FILES["data"]["tmp_name"], "r") or die("Unable to open file!");
echo "Uploaded blob data: ";
echo fread($myfile,$_FILES["data"]["size"]);
fclose($myfile);
?>
