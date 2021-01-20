<?php

$realm = 'Digest Auth Protected Area';
$nonce = uniqid();
$opaque = md5($realm);

$username = 'everybody';
$password = 'lovesbeer';

function http_digest_parse($txt)
{
    // protect against missing data
    $needed_parts = [
        'nonce' => 1,
        'nc' => 1,
        'cnonce' => 1,
        'qop' => 1,
        'username' => 1,
        'uri' => 1,
        'response' => 1
    ];
    $data = [];
    $keys = implode('|', array_keys($needed_parts));

    preg_match_all('@(' . $keys . ')=(?:([\'"])([^\2]+?)\2|([^\s,]+))@', $txt, $matches, PREG_SET_ORDER);

    foreach ($matches as $m) {
        $data[$m[1]] = $m[3] ? $m[3] : $m[4];
        unset($needed_parts[$m[1]]);
    }

    return $needed_parts ? false : $data;
}

function validate_digest($data)
{
    global $realm, $username, $password;

    if ($data['username'] !== $username)
        return false;

    $A1 = get_md5_of($data['username'], $realm, $password);
    $A2 = get_md5_of($_SERVER['REQUEST_METHOD'], $data['uri']);
    $valid_response = get_md5_of($A1, $data['nonce'], $data['nc'], $data['cnonce'], $data['qop'], $A2);

    return $data['response'] == $valid_response ? $username : null;
}

function get_md5_of(...$items)
{
    return md5(implode(':', $items));
}