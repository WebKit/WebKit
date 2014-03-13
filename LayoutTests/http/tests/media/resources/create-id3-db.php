<?php

    // This script is based on the work done by gadgetguru
    // <david@vuistbijl.nl> at
    // https://github.com/gadgetguru/PHP-Streaming-Audio and released
    // under the Public Domain.


    // This script creates, overwriting if needed, the "metadata.db"
    // file used by scripts such as "serve-video.php".
    //
    // For creating or updating an existing file you have to run the
    // http server used by the tests with:
    //
    // $ Tools/Scripts/run-webkit-httpd
    //
    // Then you will just have to use a browser to open an URL of the
    // type:
    //
    // http://127.0.0.1:8000/media/resources/create-id3-db.php?name=../../../../media/content/silence.wav
    //
    // With this, a new "metadaba.db" file will be create in the
    // directory containing the referred file. In this example, the
    // database will be created at:
    //
    // $ LayoutTests/media/content/metadata.db


    require_once '../../resources/portabilityLayer.php';
    require_once '../../resources/dir-helpers.php';

    // 500 on errors
    $httpStatus = "500 Internal Server Error";
    $htmlMessage = "";

    if (!array_key_exists("name", $_GET)) {
        trigger_error("You have not specified a 'name' parameter.", E_USER_WARNING);
        goto answering;
    }
    $mediaDirectory = dirname($_GET["name"]);

    if (!is_dir($mediaDirectory)) {
        trigger_error("The provided directory path doesn't exist.", E_USER_WARNING);
        goto answering;
    }
    $databaseFile = realpath($mediaDirectory) . "/metadata.db";

    if (file_exists($databaseFile)) {
        unlink($databaseFile);
        $htmlMessage = $htmlMessage . "<p/>Deleted previously existing db file at: '" . $databaseFile . "'";
    }

    // We don't have getid3 in WebKit. If not currently present, we
    // will try to download it now before making use of it.
    //
    // getid3 is available at http://getid3.sourceforge.net

    $getId3DestDir = "getid3";
    if (!file_exists($getId3DestDir)) {

        $tmpDir = tmpdir(FALSE, "php");
        if (!is_dir($tmpDir)) {
            trigger_error("Impossible to create temporal directory.", E_USER_WARNING);
            goto answering;
        }

        $id3DestPath = $tmpDir . "/getid3.zip";
        $phpVersion = explode(".", PHP_VERSION);
        if (($phpVersion[0] > 4) && ($phpVersion[1] > 0))
            $id3Url = "http://sourceforge.net/projects/getid3/files/getID3%28%29%201.x/1.9.7/getID3-1.9.7.zip/download";
        else
            $id3Url = "http://sourceforge.net/projects/getid3/files/getID3%28%29%201.x/1.7.10/getid3-1.7.10-20090426.zip/download";
        $htmlMessage = $htmlMessage . "<p/>getId3 not in the system.<p/>Downloading from: '" . $id3Url . "'";

        $ctxtOpts = array("http" =>
            array(
                "method"  => "GET",
                "timeout" => 60,
            )
        );
        $ctxt = stream_context_create($ctxtOpts);
        $src = fopen($id3Url, "rb", false, $ctxt);
        $dest = fopen($id3DestPath, "wb");
        $copied = stream_copy_to_stream($src, $dest);
        fclose($src);
        fclose($dest);
        if ($copied == 0) {
            trigger_error("Impossible to copy the latest getid3 zip release file.", E_USER_WARNING);
            goto answering;
        }
        $htmlMessage = $htmlMessage . "<p/>getId3 downloaded to: '" . $id3DestPath . "'";

        if (!exec("unzip " . $id3DestPath . " -d " . $tmpDir)) {
            trigger_error("Impossible to extract the downloaded zip file.", E_USER_WARNING);
            goto answering;
        }
        $htmlMessage = $htmlMessage . "<p/>Zip file successfully extracted.";

        $getId3SourceDir = first_dir($getId3DestDir, $tmpDir);
        if (!$getId3SourceDir) {
            trigger_error("Impossible to find the getid3 directory with the PHP code to copy.", E_USER_WARNING);
            goto answering;
        }
        $htmlMessage = $htmlMessage . "<p/>getId3 directory to copy found at: '" . $getId3SourceDir . "'";


        rcopy($getId3SourceDir, $getId3DestDir);
        $htmlMessage = $htmlMessage . "<p/>getId3 directory copied to its final destination at: '" . realpath($getId3SourceDir) . "'";

        rrmdir($tmpDir);
        $htmlMessage = $htmlMessage . "<p/>Deleted all temporal files at: '" . $tmpDir . "'";
    }


    // getid3 downloaded. Let's create the database file.
    require_once("getid3/getid3.php");
    $getID3 = new getID3;

    $id3FileNames = array_diff(scandir($mediaDirectory), array(".", ".."));

    foreach ($id3FileNames as $id3FileName) {
        $id3 = $getID3->analyze($mediaDirectory . "/" . $id3FileName);
        $playfile = array(
            "fileName" => $id3["filename"],
            "fileSize" => $id3["filesize"],
            "playTime" => $id3["playtime_seconds"],
            "audioStart" => $id3["avdataoffset"],
            "audioEnd" => $id3["avdataend"],
            "audioLength" => $id3["avdataend"] - $id3["avdataoffset"],
            "artist" => $id3["tags"]["id3v2"]["artist"][0],
            "title" => $id3["tags"]["id3v2"]["title"][0],
            "bitRate" => $id3["audio"]["bitrate"],
            "mimeType" => $id3["mime_type"],
            "fileFormat" => $id3["fileformat"],
        );
        if (empty($playfile["artist"]) || empty($playfile["title"]))
            list($playfile["artist"], $playfile["title"]) = explode(" - ", substr($playfile["fileName"], 0 , -4));
        $playFiles[] = $playfile;
    }

    if (!file_put_contents($databaseFile, serialize($playFiles))) {
        trigger_error("Impossible to write the getid3 db.", E_USER_WARNING);
        goto answering;
    }
    $htmlMessage = $htmlMessage . "<p/>Written new database file at: '" . $databaseFile . "'";


    // Database created, everything OK.
    $httpStatus = "200 OK";


answering:

    header("Status: " . $httpStatus);
    header("HTTP/1.1 " . $httpStatus);
    header("Connection: close");
    header("Content-Type: text/html");

    $htmlMessage = "<html><body><h1>" . $httpStatus . "</h1>" . $htmlMessage;

    if ($httpStatus == "500 Internal Server Error") {
        if (function_exists("error_get_last")) {
            $errorArray = error_get_last();
            if ($errorArray) {
                $htmlMessage = sprintf("%s<p/>Error type: %d<p/>Error message: %s<p/>".
                    "Error file: %s<p/>Error line: %d",
                    $htmlMessage, $errorArray["type"], $errorArray["message"],
                    $errorArray["file"], $errorArray["line"]);
            }
        }
    }
    $htmlMessage = $htmlMessage . "</body><html>";

    print($htmlMessage);
    flush();
    exit;
?>
