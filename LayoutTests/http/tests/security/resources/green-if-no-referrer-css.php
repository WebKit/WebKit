<?php
header('Content-Type: text/css');
if ($_SERVER['HTTP_REFERER'] == '') {
    echo "body { background-color: green; }";
} else {
    echo "body { background-color: red; }";
}
?>
