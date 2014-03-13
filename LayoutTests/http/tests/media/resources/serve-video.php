<?php

    // This script is based on the work done by gadgetguru
    // <david@vuistbijl.nl> at
    // https://github.com/gadgetguru/PHP-Streaming-Audio and released
    // under the Public Domain.

    // Set variables
    $settings = array(
        "chunkSize" => 1024 * 256,
        "databaseFile" => "metadata.db",
        "httpStatus" => "500 Internal Server Error",
        "mediaDirectory" => array_key_exists("name", $_GET) ? dirname($_GET["name"]) : "",
        "mimeType" => array_key_exists("type", $_GET) ? $_GET["type"] : "",
        "radioGenre" => "Rock",
        "radioName" => "WebKit Test Radio",
        "radioUrl" => (array_key_exists("HTTPS", $_SERVER) ? "https" : "http") . "://" . $_SERVER["HTTP_HOST"] . $_SERVER["REQUEST_URI"],
        "setContentLength" => array_key_exists("content-length", $_GET) ? $_GET["content-length"] : "yes",
        "setIcyData" => array_key_exists("icy-data", $_GET) ? $_GET["icy-data"] : "no",
        "supportRanges" => array_key_exists("ranges", $_GET) ? $_GET["ranges"] : "yes",
    );

    // 500 on errors
    if (!array_key_exists("name", $_GET)) {
        trigger_error("You have not specified a 'name' parameter.", E_USER_WARNING);
        goto answering;
    }
    $fileName = $_GET["name"];

    if (!file_exists($fileName)) {
        trigger_error("The file to stream specified at 'name' doesn't exist.", E_USER_WARNING);
        goto answering;
    }
    $settings["databaseFile"] = $settings["mediaDirectory"] . "/" . $settings["databaseFile"];

    if ($settings["setIcyData"] != "yes" && $settings["mimeType"] == "") {
        trigger_error("You have not specified a 'type' parameter.", E_USER_WARNING);
        goto answering;
    }

    if ($settings["setIcyData"] == "yes") {
        if (!file_exists($settings["databaseFile"])) {

            // If the metadata database file doesn't exist it has to
            // be create previously.
            //
            // Check the instructions about how to create it from the
            // create-id3-db.php script file in this same directory.

            trigger_error("The metadata database doesn't exist. To create one, check the script 'create-id3-db.php'.", E_USER_WARNING);
            goto answering;
        }

        $playFiles = unserialize(file_get_contents($settings["databaseFile"]));
        foreach ($playFiles as $i=>$playFile) {
            if (basename($fileName) == $playFile["fileName"]) {
                $fileInDB = true;
                break;
            }
        }

        if (!isset($fileInDB)) {
            trigger_error("The requested file is not in the database.", E_USER_WARNING);
            goto answering;
        }
    }

    // There is everything needed to send the media file
    $fileSize = filesize($fileName);
    $start = 0;
    $end = $fileSize - 1;
    if ($settings["supportRanges"] != "no" && array_key_exists("HTTP_RANGE", $_SERVER))
        $contentRange = $_SERVER["HTTP_RANGE"];
    if (isset($contentRange)) {
        $range = explode("-", substr($contentRange, strlen("bytes=")));
        $start = intval($range[0]);
        if (!empty($range[1]))
            $end = intval($range[1]);
        $settings["httpStatus"] = "206 Partial Content";
    } else
        $settings["httpStatus"] = "200 OK";


answering:

    header("Status: " . $settings["httpStatus"]);
    header("HTTP/1.1 " . $settings["httpStatus"]);
    header("Connection: close");

    if ($settings["httpStatus"] == "500 Internal Server Error") {
        header("Content-Type: text/html");
        $errorMessage = sprintf("<html><body><h1>%s</h1><p/>",
                                $settings["httpStatus"]);
        if (function_exists("error_get_last")) {
            $errorArray = error_get_last();
            if ($errorArray) {
                $errorMessage = sprintf("%sError type: %d<p/>Error message: %s<p/>".
                    "Error file: %s<p/>Error line: %d<p/>",
                    $errorMessage, $errorArray["type"], $errorArray["message"],
                    $errorArray["file"], $errorArray["line"]);
            }
        }
        $errorMessage = $errorMessage . "</body><html>";
        print($errorMessage);
        flush();
        exit;
    }

    header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");
    header("Pragma: no-cache");
    header("Etag: " . '"' . $fileSize . "-" . filemtime($fileName) . '"');
    if ($settings["setIcyData"] == "yes") {
        $bitRate = ceil($playFiles[$i]["bitRate"] / 1000);
        if ($settings["mimeType"] == "")
            $settings["mimeType"] = $playFiles[$i]["mimeType"];
        header("icy-notice1: <BR>This stream requires a shoutcast/icecast compatible player.<BR>");
        header("icy-notice2: WebKit Stream Test<BR>");
        header("icy-name: " . $settings["radioName"]);
        header("icy-genre: " . $settings["radioGenre"]);
        header("icy-url: " . $settings["radioUrl"]);
        header("icy-pub: 1");
        header("icy-br: " . $bitRate);
    }
    header("Content-Type: " . $settings["mimeType"]);
    if ($settings["supportRanges"] != "no")
        header("Accept-Ranges: bytes");
    if ($settings["setContentLength"] != "no")
        header("Content-Length: " . ($end - $start + 1));
    if (isset($contentRange))
        header("Content-Range: bytes " . $start . "-" . $end . "/" . $fileSize);

    $offset = $start;

    $fn = fopen($fileName, "rb");
    fseek($fn, $offset, 0);

    while (!feof($fn) && $offset <= $end && connection_status() == 0) {
        $readSize = min($settings["chunkSize"], ($end - $offset) + 1);
        $buffer = fread($fn, $readSize);
        print($buffer);
        flush();
        $offset += $settings["chunkSize"];
    }
    fclose($fn);

    exit;
?>
