<?php

require_once('../include/json-header.php');

ensure_privileged_api_data();

$user = array_get($_SERVER, 'REMOTE_USER');

$expiritaion = time() + 3600; // Valid for one hour.
$_COOKIE['CSRFSalt'] = rand();
$_COOKIE['CSRFExpiration'] = $expiritaion;

setcookie('CSRFSalt', $_COOKIE['CSRFSalt']);
setcookie('CSRFExpiration', $expiritaion);

exit_with_success(array('user' => $user, 'token' => compute_token(), 'expiration' => $expiritaion * 1000));

?>
