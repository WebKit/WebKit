<?php

require_once('db.php');

header('Content-type: application/json');
$maxage = config('jsonCacheMaxAge');
header('Expires: ' . gmdate('D, d M Y H:i:s', time() + $maxage) . ' GMT');
header("Cache-Control: maxage=$maxage");

function exit_with_error($status, $details = array()) {
    $details['status'] = $status;
    merge_additional_details($details);

    echo json_encode($details);
    exit(1);
}

function echo_success($details = array()) {
    $details['status'] = 'OK';
    merge_additional_details($details);

    echo json_encode($details);
}

function exit_with_success($details = array()) {
    echo_success($details);
    exit(0);
}

$additional_exit_details = array();

function set_exit_detail($name, $value) {
    global $additional_exit_details;
    $additional_exit_details[$name] = $value;
}

function merge_additional_details(&$details) {
    global $additional_exit_details;
    foreach ($additional_exit_details as $name => $value) {
        if (!array_key_exists($name, $details))
            $details[$name] = $value;
    }
}

function connect() {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionError');
    return $db;
}

function camel_case_words_separated_by_underscore($name) {
    return implode('', array_map('ucfirst', explode('_', $name)));
}

function require_format($name, $value, $pattern) {
    if (!preg_match($pattern, $value))
        exit_with_error('Invalid' . $name, array('value' => $value));
}

function require_existence_of($array, $list_of_arguments, $prefix = '') {
    if ($prefix)
        $prefix .= '_';
    foreach ($list_of_arguments as $key => $pattern) {
        $name = camel_case_words_separated_by_underscore($prefix . $key);
        if (!array_key_exists($key, $array))
            exit_with_error($name . 'NotSpecified');
        require_format($name, $array[$key], $pattern);
    }
}

function ensure_privileged_api_data() {
    global $HTTP_RAW_POST_DATA;

    if ($_SERVER['REQUEST_METHOD'] != 'POST')
        exit_with_error('InvalidRequestMethod');

    if (!isset($HTTP_RAW_POST_DATA))
        exit_with_error('InvalidRequestContent');

    $data = json_decode($HTTP_RAW_POST_DATA, true);

    if ($data === NULL)
        exit_with_error('InvalidRequestContent');

    return $data;
}

function ensure_privileged_api_data_and_token() {
    $data = ensure_privileged_api_data();
    if (!verify_token(array_get($data, 'token')))
        exit_with_error('InvalidToken');
    return $data;
}

function remote_user_name() {
    return array_get($_SERVER, 'REMOTE_USER');
}

function compute_token() {
    if (!array_key_exists('CSRFSalt', $_COOKIE) || !array_key_exists('CSRFExpiration', $_COOKIE))
        return NULL;
    $user = remote_user_name();
    $salt = $_COOKIE['CSRFSalt'];
    $expiration = $_COOKIE['CSRFExpiration'];
    return hash('sha256', "$salt|$user|$expiration");
}

function verify_token($token) {
    $expected_token = compute_token();
    return $expected_token && $token == $expected_token && $_COOKIE['CSRFExpiration'] > time();
}

?>
