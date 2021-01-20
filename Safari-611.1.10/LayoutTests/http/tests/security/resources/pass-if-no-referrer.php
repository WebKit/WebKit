<?php
if ($_SERVER['HTTP_REFERER'] == '') {
    http_response_code(200);
} else {
    http_response_code(500);
}
header('Content-Type: text/html');
?>
