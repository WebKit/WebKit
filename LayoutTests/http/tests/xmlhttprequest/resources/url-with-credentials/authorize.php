<?php

if (!isset($_SERVER['PHP_AUTH_USER'])) {
    header('HTTP/1.0 401 Unauthorized');
    header('WWW-Authenticate: Basic realm="WebKit Test Area"');
} else {
    include "authenticate.php";
}
