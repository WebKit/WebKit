<?php
    $username = $password = "aaaa";

    if ($_SERVER['PHP_AUTH_USER'] == $username && $_SERVER['PHP_AUTH_PW'] == $password){
        echo 'Test steps:<br>';
        echo '1. Set Private Browsing off. (Settings -> Privacy & Security -> Private Browsing)<br>';
        echo '2. Refresh the page.<br>';
        echo '3. There should be a dialog for you to input username and password again.<br>';
        echo 'If you can\'t see the dialog, this test fails.<br>';
        exit;
    } else {
        header('WWW-Authenticate: Basic realm="My Realm"');
        header('HTTP/1.0 401 Unauthorized');
        echo "Authorization Required.";
        exit;
    }
?>
