<?php
header("Vary: Origin");
header("Cache-Control: max-age=31536000");
if (isset($_SERVER['HTTP_ORIGIN'])) {
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    echo "Cross origin ";
} else {
    echo "Same origin ";
}
?>
response