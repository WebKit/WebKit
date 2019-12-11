<?php
$shouldReceiveCookies = $_GET["shouldReceiveCookies"];
if(isset($_COOKIE["firstPartyCookie"])) {
    echo "let cookieResult = '". ($shouldReceiveCookies ? "PASS " : "FAIL ") . "Did receive firstPartyCookie == " . $_COOKIE["firstPartyCookie"] . "';";
} else {
    echo "let cookieResult = '". ($shouldReceiveCookies ? "FAIL " : "PASS ") . "Did not receive cookie named firstPartyCookie';";
}
?>
postMessage(cookieResult);
