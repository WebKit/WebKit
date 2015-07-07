<?php

    $ua = $_SERVER["HTTP_USER_AGENT"];

    if (!isset($ua) || stripos($ua, "WebKit/") === false || stripos($ua, "(KHTML, like Gecko)") === false)
        die;

    $fileName = $_GET["name"];
    $type = $_GET["type"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    @include("./serve-video.php"); 

?>
