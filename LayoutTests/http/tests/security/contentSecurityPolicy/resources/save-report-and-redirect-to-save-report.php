<?php
require_once "report-file-path.php";

$DO_NOT_CLEAR_COOKIES = true; // Used by save-report.php
require_once "save-report.php";

header("HTTP/1.1 307");
header("Location: save-report.php" . (isset($_SERVER["QUERY_STRING"]) ? "?" . $_SERVER["QUERY_STRING"] : ""));
?>
