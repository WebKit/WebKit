<?php
header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
header("Access-Control-Allow-Headers: X-WebKit");
if(isset($_GET["value"])) {
  echo $_GET["value"];
} else {
  echo "No query parameter named 'value.'";
}
?>