<?php
    session_start();
    if (isset($_SESSION['triedTimes']))
        $_SESSION['triedTimes'] = $_SESSION['triedTimes'] + 1;
    else
        $_SESSION['triedTimes'] = 1;

    $username = $password = "aaaa";

    if ($_SERVER['PHP_AUTH_USER'] == $username && $_SERVER['PHP_AUTH_PW'] == $password){
        echo "Auth dialog behavior when press OK button: ";
        if ($_SESSION['triedTimes'] == 5)
            echo "<span style='color:green'>PASS</span>";
        else
            echo "<span style='color:red'>FAIL</span>";
        exit;
    } else {
        header('WWW-Authenticate: Basic realm="My Realm"');
        header('HTTP/1.0 401 Unauthorized');
        echo "Auth dialog behavior when press Cancel button: ";
        if ($_SESSION['triedTimes'] == 1)
            echo "<span style='color:green'>PASS</span>";
        else
            echo "<span style='color:red'>FAIL</span>";
        exit;
    }
?>
