<?php

require_once('db.php');

require_once('test-path-resolver.php');

header('Content-type: application/json');

function exit_with_error($status, $details = array()) {
    $details['status'] = $status;
    merge_additional_details($details);

    echo json_encode($details);
    exit(1);
}

function set_successful(&$details = array()) {
    $details['status'] = 'OK';
    merge_additional_details($details);
    return $details;
}

function success_json($details = array()) {
    set_successful($details);
    return json_encode($details);
}

function exit_with_success($details = array()) {
    echo success_json($details);
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
        exit_with_error('Invalid' . camel_case_words_separated_by_underscore($name), array('value' => $value));
}

function require_match_one_of_values($name, $value, $valid_values) {
    if (!in_array($value, $valid_values))
        exit_with_error('Invalid' . camel_case_words_separated_by_underscore($name), array('value' => $value));
}

function validate_arguments($array, $list_of_arguments) {
    $result = array();
    foreach ($list_of_arguments as $name => $pattern) {
        $value = array_get($array, $name, '');
        if ($pattern == 'int') {
            require_format($name, $value, '/^\d+$/');
            $value = intval($value);
        } else if ($pattern == 'int?') {
            require_format($name, $value, '/^\d*$/');
            $value = $value ? intval($value) : null;
        } else if ($pattern == 'json') {
            $value = json_decode($value, true);
            if ($value === NULL)
                exit_with_error('Invalid' . camel_case_words_separated_by_underscore($name));
        } else
            require_format($name, $value, $pattern);
        $result[$name] = $value;
    }
    return $result;
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

    if (config('maintenanceMode'))
        exit_with_error('InMaintenanceMode');

    if ($_SERVER['REQUEST_METHOD'] != 'POST')
        exit_with_error('InvalidRequestMethod');

    $data = json_decode(file_get_contents('php://input'), true);

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

function remote_user_name($data = NULL) {
    return $data && should_authenticate_as_worker($data) ? NULL : array_get($_SERVER, 'REMOTE_USER');
}

function should_authenticate_as_worker($data) {
    return (array_key_exists('workerName', $data) && array_key_exists('workerPassword', $data)) ||
        (array_key_exists('slaveName', $data) && array_key_exists('slavePassword', $data));
}

function ensure_privileged_api_data_and_token_or_worker($db) {
    $data = ensure_privileged_api_data();
    if (should_authenticate_as_worker($data))
        verify_worker($db, $data);
    else if (!verify_token(array_get($data, 'token')))
        exit_with_error('InvalidToken');
    return $data;
}

function compute_token() {
    if (!array_key_exists('CSRFSalt', $_COOKIE) || !array_key_exists('CSRFExpiration', $_COOKIE))
        return NULL;
    $user = remote_user_name(array());
    $salt = $_COOKIE['CSRFSalt'];
    $expiration = $_COOKIE['CSRFExpiration'];
    return hash('sha256', "$salt|$user|$expiration");
}

function verify_token($token) {
    $expected_token = compute_token();
    return $expected_token && $token == $expected_token && $_COOKIE['CSRFExpiration'] > time();
}

function verify_worker($db, $params) {
    array_key_exists('workerName', $params) or array_key_exists('slaveName', $params) or exit_with_error('MissingWorkerName');
    array_key_exists('workerPassword', $params) or array_key_exists('slavePassword', $params) or exit_with_error('MissingWorkerPassword');

    $worker_info = array(
        'name' => array_get($params, 'workerName', array_get($params, 'slaveName')),
        'password_hash' => hash('sha256', array_get($params, 'workerPassword', array_get($params, 'slavePassword')))
    );

    $matched_worker = $db->select_first_row('build_workers', 'worker', $worker_info);
    if (!$matched_worker)
        exit_with_error('WorkerNotFound', array('name' => $worker_info['name']));

    return $matched_worker['worker_id'];
}

function find_triggerable_for_task($db, $task_id) {
    $task_id = intval($task_id);

    $test_rows = $db->query_and_fetch_all('SELECT metric_test AS "test", task_platform as "platform"
        FROM analysis_tasks JOIN test_metrics ON task_metric = metric_id WHERE task_id = $1', array($task_id));
    if (!$test_rows)
        return NULL;
    $target_test_id = $test_rows[0]['test'];
    $platform_id = $test_rows[0]['platform'];

    $path_resolver = new TestPathResolver($db);
    $test_ids = $path_resolver->ancestors_for_test($target_test_id);

    $results = $db->query_and_fetch_all('SELECT trigconfig_id AS "config_id",
        trigconfig_triggerable AS "triggerable", trigconfig_test AS "test"
        FROM triggerable_configurations WHERE trigconfig_platform = $1 AND trigconfig_test = ANY($2)',
        array($platform_id, '{' . implode(', ', $test_ids) . '}'));
    if (!$results)
        return NULL;

    $test_to_triggerable = array();
    $test_to_trigconfig = array();
    foreach ($results as $row) {
        $test_to_triggerable[$row['test']] = $row['triggerable'];
        $test_to_trigconfig[$row['test']] = $row['config_id'];
    }

    $configrepetition_rows = $db->select_rows('triggerable_configuration_repetition_types', 'configrepetition', array());
    $trigconfig_to_repetition_types = array();
    foreach ($configrepetition_rows as $row)
        array_push(array_ensure_item_has_array($trigconfig_to_repetition_types, $row['configrepetition_config']), $row['configrepetition_type']);

    foreach ($test_ids as $test_id) {
        $triggerable = array_get($test_to_triggerable, $test_id);
        if ($triggerable) {
            $supported_repetition_types = $trigconfig_to_repetition_types[$test_to_trigconfig[$test_id]];
            return array('id' => $triggerable, 'test' => $test_id, 'platform' => $platform_id, 'supportedRepetitionTypes' => $supported_repetition_types);
        }
    }

    return NULL;
}

?>
