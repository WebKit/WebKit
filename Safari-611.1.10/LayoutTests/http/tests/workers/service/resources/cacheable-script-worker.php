<?php
    $max_age = 12 * 31 * 24 * 60 * 60; //one year
    header('Cache-Control: public, max-age=' . $max_age);
    header('Content-Type: text/javascript');

    $randomId = '';
    $charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789';
    for ($i = 0; $i < 16; $i++)
        $randomId .= $charset[rand(0, strlen($charset) - 1)];

    echo 'let randomId = "' . $randomId . '";';
?>

self.addEventListener("message", function(e) {
   e.source.postMessage(randomId);
});
