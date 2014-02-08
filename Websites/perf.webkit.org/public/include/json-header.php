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

function require_format($key, $value, $pattern) {
    if (!preg_match($pattern, $value))
        exit_with_error('Invalid' . camel_case_words_separated_by_underscore($key), array('value' => $value));
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

?>
