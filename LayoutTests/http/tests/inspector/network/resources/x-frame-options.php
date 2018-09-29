<?php

$option = isset($_GET['option']) ? $_GET['option'] : 'DENY';

header('X-Frame-Options: ' . $option);

echo $option;

?>
