<?php
    $method = $_SERVER['REQUEST_METHOD'];
    header("REQMETHOD: $method");
    if ($method != "HEAD") {
      echo $method;
    }
?>
