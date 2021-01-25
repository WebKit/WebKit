<?php
header("HTTP/1.0 302 Found");
if (isset($_GET["conversionData"]) && isset($_GET["priority"])) {
  header("Location: https://127.0.0.1:8000/.well-known/private-click-measurement/trigger-attribution/" . $_GET["conversionData"] . "/" . $_GET["priority"]);
} else if (isset($_GET["conversionData"])) {
  header("Location: https://127.0.0.1:8000/.well-known/private-click-measurement/trigger-attribution/" . $_GET["conversionData"]);
}
?>
