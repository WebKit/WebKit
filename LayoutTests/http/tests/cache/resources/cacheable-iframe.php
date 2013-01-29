<?php
require_once '../../resources/portabilityLayer.php';

$max_age = 12 * 31 * 24 * 60 * 60; //one year
$expires = gmdate(DATE_RFC1123, time() + $max_age);

header('Cache-Control: public, max-age=' . $max_age);
header('Expires: ' . $expires);
header('Content-Type: text/html');
exit();
?>
