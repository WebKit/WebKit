<?php
  if (isset($_SERVER['HTTP_IF_NONE_MATCH'])) {
    header('HTTP/1.1 304 Not Modified');
    exit;
  }

  $type = $_GET["type"];

  $response_code = 200;
  $body = "";
  $content_type = "text/plain";

  switch ($type) {
  case "audio":
      $body = file_get_contents("silence.wav");
      $content_type = "audio/wav";
      break;

  case "css":
      $body = file_get_contents("gray_bg.css");
      $content_type = "text/css";
      break;

  case "font":
      $body = file_get_contents("Ahem.ttf");
      $content_type = "application/octet-stream";
      break;

  case "iframe":
      $body = file_get_contents("blank_page_green.htm");
      $content_type = "text/html";
      break;

  case "image":
      $body = file_get_contents("blank_image.png");
      $content_type = "image/png";
      break;

  case "script":
      $body = file_get_contents("empty_script.js");
      $content_type = "text/javascript";
      break;

  default:
      $response_code = 404;
      break;
  }

  header("HTTP/1.1 $response_code");
  header("Content-type: $content_type");
  header("Etag: 7");
  print($body);
  exit;
?>
