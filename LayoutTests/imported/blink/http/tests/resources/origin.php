<?php

if ($_SERVER['HTTP_ORIGIN']) {
    header('Location: square100.png');
}
else {
    header('Location: square200.png');
}
header('HTTP/1.1 302 Redirect');
