<?php

$origin = $_GET['origin'];

if ($origin != 'none')
    header("Access-Control-Allow-Origin: $origin");

if (isset($_GET['headers']))
    header("Access-Control-Allow-Headers: {$_GET['headers']}");
if (isset($_GET['methods']))
    header("Access-Control-Allow-Methods: {$_GET['methods']}");

foreach ($_SERVER as $name => $value)
{
    if (substr($name, 0, 5) == 'HTTP_')
    {
        $name = strtolower(str_replace('_', '-', substr($name, 5)));
        $headers[$name] = $value;
    } else if ($name == "CONTENT_TYPE") {
        $headers["content-type"] = $value;
    } else if ($name == "CONTENT_LENGTH") {
        $headers["content-length"] = $value;
    }
}

$headers['get_value'] = isset($_GET['get_value']) ? $_GET['get_value'] : '';

echo json_encode( $headers );
