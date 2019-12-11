<?php
    header('Content-Type: text/javascript');

    $randomId = '';
    $charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789';
    for ($i = 0; $i < 16; $i++)
        $randomId .= $charset[rand(0, strlen($charset) - 1)];

    echo 'let randomId = "' . $randomId . '";';
?>

self.addEventListener("message", (event) => {
    self.registration.update();
});

