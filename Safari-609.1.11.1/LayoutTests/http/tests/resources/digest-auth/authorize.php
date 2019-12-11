<?php

require_once "digest.php";

if (empty($_SERVER['PHP_AUTH_DIGEST'])) {
    header('HTTP/1.0 401 Unauthorized');
    header("WWW-Authenticate: Digest realm=\"{$realm}\",qop=\"auth\",nonce=\"{$nonce}\",opaque=\"{$opaque}\"");

    echo "<h1>You are not allowed to see this secret.</h1>";
} elseif (!($data = http_digest_parse($_SERVER['PHP_AUTH_DIGEST']))) {
    header('HTTP/1.0 401 Unauthorized');
    echo "<h1>Invalid credentials.</h1>";
} elseif (!($username = validate_digest($data))) {
    header('HTTP/1.0 401 Unauthorized');
    echo "<h1>Wrong credentials.</h1>";
} else {
    header('Content-Type: text/plain');
    echo 'User: ' . $username . "\n";
}
