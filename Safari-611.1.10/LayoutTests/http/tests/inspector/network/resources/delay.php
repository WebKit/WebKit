<?php

$delay = isset($_GET['delay']) ? intval($_GET['delay']) : 100;
$redirect = isset($_GET['redirect']) ? intval($_GET['redirect']) : 'redirect.php';

usleep($delay * 1000);

header('Location: ' . $redirect);

?>

