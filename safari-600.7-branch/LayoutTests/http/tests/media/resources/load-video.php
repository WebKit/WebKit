<?php

    $fileName = $_GET["name"];
    $type = $_GET["type"];
    if (array_key_exists("ranges", $_GET))
        $ranges = $_GET["ranges"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    if (isset($ranges))
        $_GET["ranges"] = $ranges;
    @include("./serve-video.php");

?>
