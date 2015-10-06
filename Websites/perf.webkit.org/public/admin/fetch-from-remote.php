<?php

require('../include/db.php');

function main($path)
{
    $remote_server = config('remoteServer');

    $remote_url = $remote_server['url'] . $path;
    $cache_path = config_path('cacheDirectory', hash("sha256", $remote_url));

    $content = @file_get_contents($cache_path);
    if ($content === FALSE) {
        $content = fetch_remote($remote_server, $remote_url);
        if ($content === FALSE) {
            header('HTTP/1.0 404 Not Found');
            echo 'NotFound';
            exit(1);
        }
        file_put_contents($cache_path, $content);
    }

    header('Content-Type: application/json');
    header('Content-Length: ' . strlen($content));
    echo $content;
}

function fetch_remote($remote_server, $remote_url)
{
    $auth = array_get($remote_server, 'basicAuth');

    $header = '';
    if ($auth)
        $header = 'Authorization: Basic ' . base64_encode($auth['username'] . ':' . $auth['password']);

    $context = stream_context_create(array('http' => array('method' => 'GET', 'header' => $header)));

    return @file_get_contents($remote_url, false, $context);
}

main(array_get($_SERVER, 'PATH_INFO', ''));

?>
