<?php

    $ua = $_SERVER["HTTP_USER_AGENT"];

	header("Cache-Control: no-store");
	header("Connection: close");
	if (!isset($_SERVER['PHP_AUTH_USER'])) {
	    header("WWW-authenticate: Basic realm=\"" . $_SERVER['REQUEST_URI'] . "\"");
	    header('HTTP/1.0 401 Unauthorized');
	    exit;
	}

    $fileName = $_GET["name"];
    $type = $_GET["type"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    @include("./serve-video.php"); 
?>
