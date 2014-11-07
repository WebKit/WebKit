<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $author = remote_user_name();
    $name = array_get($data, 'name');
    $start_run_id = array_get($data, 'startRun');
    $end_run_id = array_get($data, 'endRun');

    if (!$name)
        exit_with_error('MissingName', array('name' => $name));
    $range = array('startRunId' => $start_run_id, 'endRunId' => $end_run_id);
    if (!$start_run_id || !$end_run_id)
        exit_with_error('MissingRange', $range);

    $db = connect();
    $start_run = ensure_row_by_id($db, 'test_runs', 'run', $start_run_id, 'InvalidStartRun', $range);
    $end_run = ensure_row_by_id($db, 'test_runs', 'run', $end_run_id, 'InvalidEndRun', $range);

    $config = ensure_config_from_runs($db, $start_run, $end_run);

    $db->begin_transaction();
    $duplicate = $db->select_first_row('analysis_tasks', 'task', array('start_run' => $start_run_id, 'end_run' => $end_run_id));
    if ($duplicate) {
        $db->rollback_transaction();
        exit_with_error('DuplicateAnalysisTask', array('duplicate' => $duplicate));
    }

    $task_id = $db->insert_row('analysis_tasks', 'task', array(
        'name' => $name,
        'author' => $author,
        'platform' => $config['config_platform'],
        'metric' => $config['config_metric'],
        'start_run' => $start_run_id,
        'end_run' => $end_run_id));
    $db->commit_transaction();

    exit_with_success(array('taskId' => $task_id));
}

function ensure_row_by_id($db, $table, $prefix, $id, $error_name, $error_params) {
    $row = $db->select_first_row($table, $prefix, array('id' => $id));
    if (!$row)
        exit_with_error($error_name, array($error_params));
    return $row;
}

function ensure_config_from_runs($db, $start_run, $end_run) {
    $range = array('startRun' => $start_run, 'endRun' => $end_run);

    if ($start_run['run_config'] != $end_run['run_config'])
        exit_with_error('RunConfigMismatch', $range);

    return ensure_row_by_id($db, 'test_configurations', 'config', $start_run['run_config'], 'ConfigNotFound', $range);
}

main();

?>
