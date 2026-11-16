<?php

require_once('../include/json-header.php');

$data = ensure_privileged_api_data();

$expiration = time() + 3600; // Valid for one hour.
$_COOKIE['CSRFSalt'] = bin2hex(random_bytes(16));
$_COOKIE['CSRFExpiration'] = $expiration;

$cookie_options = array(
    'expires' => $expiration,
    'path' => '/privileged-api',
    'secure' => !empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off',
    'httponly' => true,
    'samesite' => 'Strict',
);
setcookie('CSRFSalt', $_COOKIE['CSRFSalt'], $cookie_options);
setcookie('CSRFExpiration', $expiration, $cookie_options);

exit_with_success(array('user' => remote_user_name($data), 'token' => compute_token(), 'expiration' => $expiration * 1000));

?>
