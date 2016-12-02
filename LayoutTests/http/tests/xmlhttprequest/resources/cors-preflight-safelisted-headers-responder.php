<?php
header('Access-Control-Allow-Origin: http://127.0.0.1:8000');

if ($_SERVER['REQUEST_METHOD'] == 'OPTIONS' && isset($_GET['explicitlyAllowHeaders'])) {
    header('Access-Control-Allow-Methods: GET, OPTIONS');
    header('Access-Control-Allow-Headers: Accept, Accept-Language, Content-Language');
}
?>