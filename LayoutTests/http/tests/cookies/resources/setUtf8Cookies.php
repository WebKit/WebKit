<?php
    setcookie("php_cookie", "UTF-8 æøå 中国");
    header('Content-Type: text/html; charset=UTF-8');
    header('Location: setUtf8Cookies-result.php');
?>
