<?php
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Headers: X-WebKit");
if ($_SERVER["REQUEST_METHOD"] == "OPTIONS" && isset($_SERVER["HTTP_ACCESS_CONTROL_REQUEST_METHOD"]) && $_SERVER["HTTP_ACCESS_CONTROL_REQUEST_METHOD"] == "GET") {
  exit;
}
$headerStringValue = $_SERVER["HTTP_X_WEBKIT"];
header("HTTP/1.1 301 Moved Permanently");
header("Cache-Control: private, max-age=31536000", true);
header('ETag: "WebKitTest"', true);
header("Location: http://localhost:8000/resourceLoadStatistics/resources/echo-query.php?value=" . $headerStringValue);
?>