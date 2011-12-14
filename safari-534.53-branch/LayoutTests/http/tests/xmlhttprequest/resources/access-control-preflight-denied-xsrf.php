<?php
require_once '../../resources/portabilityLayer.php';

$tmpFile = sys_get_temp_dir() . "/xsrf.txt";

function fail($state)
{
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    header("Access-Control-Allow-Credentials: true");
    header("Access-Control-Allow-Methods: GET");
    header("Access-Control-Max-Age: 1");
    echo "FAILED: Issued a " . $_SERVER['REQUEST_METHOD'] . " request during state '" . $state . "'\n";
    exit();
}

function setState($newState, $file)
{
    file_put_contents($file, $newState);
}

function getState($file)
{
    $state = NULL;
    if (file_exists($file))
        $state = file_get_contents($file);
    return $state ? $state : "Uninitialized";
}

$state = getState($tmpFile);

if ($_SERVER['REQUEST_METHOD'] == "GET" 
    && $_GET['state'] == "reset") {
    if (file_exists($tmpFile)) unlink($tmpFile);
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    header("Access-Control-Max-Age: 1");
    echo "Server state reset.\n";
} else if ($state == "Uninitialized") {
    if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
        if ($_GET['state'] == "method" || $_GET['state'] == "header") {
            header("Access-Control-Allow-Methods: GET");
            header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
            header("Access-Control-Max-Age: 1");
        }
        echo("FAIL: This request should not be displayed.\n");
        setState("Denied", $tmpFile);
    } else {
        fail($state);
    }
} else if ($state == "Denied") {
    if ($_SERVER['REQUEST_METHOD'] == "GET" 
        && $_GET['state'] == "complete") {
        unlink($tmpFile);
        header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
        header("Access-Control-Max-Age: 1");
        echo "PASS: Request successfully blocked.\n";
    } else {
        setState("Deny Ignored", $tmpFile);
        fail($state);
    }
} else if ($state == "Deny Ignored") {
    unlink($tmpFile);
    fail($state);
} else {
    if (file_exists($tmpFile)) unlink($tmpFile);
    fail("Unknown");
}
?>
