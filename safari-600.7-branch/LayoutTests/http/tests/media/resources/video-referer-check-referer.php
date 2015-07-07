<?php

    $refer = $_SERVER["HTTP_REFERER"];
    if (!isset($refer) || stripos($refer, "video-referer.html") === false)
        die;

    $fileName = $_GET["name"];
    $type = $_GET["type"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    @include("./serve-video.php"); 

?>
