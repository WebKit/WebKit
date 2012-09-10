<?php
    $dnt = $_SERVER["HTTP_DNT"];
    if (!isset($dnt) || stripos($dnt, "1") === false)
        die;

    $fileName = $_GET["name"];
    $type = $_GET["type"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    @include("./serve-video.php");
?>
