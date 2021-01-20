<?php
$headers = apache_request_headers();

header("Content-Type: text/plain");
header("Cache-Control: no-store");

foreach ($headers as $header => $value) {
    $request_headers[strtolower($header)] = $value;
}
echo json_encode( $request_headers );
