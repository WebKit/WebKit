<?php

require_once('../include/json-header.php');

$data = ensure_privileged_api_data();

$expiritaion = time() + 3600; // Valid for one hour.
$_COOKIE['CSRFSalt'] = rand();
$_COOKIE['CSRFExpiration'] = $expiritaion;

setcookie('CSRFSalt', $_COOKIE['CSRFSalt']);
setcookie('CSRFExpiration', $expiritaion);

exit_with_success(array('user' => remote_user_name($data), 'token' => compute_token(), 'expiration' => $expiritaion * 1000));

?>
