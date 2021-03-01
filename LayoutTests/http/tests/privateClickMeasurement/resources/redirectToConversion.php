<?php
if (isset($_GET["delay_ms"])) {
    usleep($_GET["delay_ms"] * 1000);
}
header("HTTP/1.1 302 Found");
header("Cache-Control: no-cache, no-store, must-revalidate");
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET");
if (isset($_GET["conversionData"]) && isset($_GET["priority"])) {
  header("Location: /.well-known/private-click-measurement/trigger-attribution/" . $_GET["conversionData"] . "/" . $_GET["priority"]);
} else if (isset($_GET["conversionData"])) {
  header("Location: /.well-known/private-click-measurement/trigger-attribution/" . $_GET["conversionData"]);
}
?>
