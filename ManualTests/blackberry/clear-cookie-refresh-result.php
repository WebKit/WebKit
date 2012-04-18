<?php
    echo 'This tests the function of clear cookies by clicking "Clear Cookies" button from Settings -> Privacy & Security.<br>';
    echo 'It is for https://bugs.webkit.org/show_bug.cgi?id=84223.<br>';
    echo 'Please clear the cookies from Settings -> Privacy & Security by pressing "Clear Cookies" button, then refresh this page to see the result.<br>';
    if (isset($_COOKIE["cookieName"])) {
        echo 'result: NOT PASS';
    } else {
        echo 'result: PASS';
    }
?>
