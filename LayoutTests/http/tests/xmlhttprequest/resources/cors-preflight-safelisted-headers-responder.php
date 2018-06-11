<?php

if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS') {
    if (!isset($_GET['shouldPreflight'])) {
        return 404;
    }
    if (isset($_GET['explicitlyAllowHeaders'])) {
        header('Access-Control-Allow-Methods: GET, OPTIONS');
        header('Access-Control-Allow-Headers: Accept, Accept-Language, Content-Language');
    }
}
header('Access-Control-Allow-Origin: http://127.0.0.1:8000');

?>
