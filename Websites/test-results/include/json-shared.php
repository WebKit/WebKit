<?php

require_once('../include/db.php');

header('Content-type: application/json');
$maxage = config('jsonCacheMaxAge');
header('Expires: ' . gmdate('D, d M Y H:i:s', time() + $maxage) . ' GMT');
header("Cache-Control: maxage=$maxage");

function exit_with_error($status, $details = array()) {
    $details['status'] = $status;
    echo json_encode($details);
    exit(1);
}

function exit_with_success($details = array()) {
    $details['status'] = 'OK';
    echo json_encode($details);
    exit(0);
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
