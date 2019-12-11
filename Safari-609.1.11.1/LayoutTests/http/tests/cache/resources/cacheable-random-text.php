<?php
    $max_age = 12 * 31 * 24 * 60 * 60; //one year
    header('Cache-Control: public, max-age=' . $max_age);
    header('Content-Type: text/html');

    $id = $GET['id'];
    if (!isset($GET['random_id'])) {
        $id = '';
        $charset = 'ABCDEFGHIKLMNOPQRSTUVWXYZ0123456789';
        for ($i = 0; $i < 16; $i++)
            $id .= $charset[rand(0, strlen($charset) - 1)];
    }
    echo '<html><body>Some random text' . $id .  '</hml></body>';
?>
