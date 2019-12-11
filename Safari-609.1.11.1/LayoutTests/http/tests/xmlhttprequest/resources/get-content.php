<?php
  $content = $_GET["content"] ? $_GET["content"] : "";
  $type = $_GET["type"] ? $_GET["type"] : "";
  header("HTTP/1.1 200 OK");
  header("Content-Type:" . $type);

  echo $content;
  exit;
?>
