<?php
  $code = ctype_digit($_GET["code"]) ? $_GET["code"] : "302";
  $location = $_GET["location"] ? $_GET["location"] : $_SERVER["SCRIPT_NAME"] . "?followed";

  header("HTTP/1.1 $code");
  header("Location: $location");
  exit;
?>
