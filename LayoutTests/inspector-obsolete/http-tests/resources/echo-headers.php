<?php

$headers = Array('HTTP_X_WEBINSPECTOR_EXTENSION', 'HTTP_USER_AGENT');

foreach ($headers as $header) {
    echo $header . ": " . $_SERVER[$header] . "\n";
}

?>
