<?php

    $requiredReferer = $_GET["referer"];
    $referHeader = $_SERVER["HTTP_REFERER"];
    if (!isset($referHeader) || stripos($referHeader, $requiredReferer) === false)
        die;

    $fileName = $_GET["name"];
    $type = $_GET["type"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    @include("./serve-video.php"); 

?>
