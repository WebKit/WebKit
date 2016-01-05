<?php

require_once('../include/json-header.php');

function main() {
    $db = connect();
    $data = ensure_privileged_api_data_and_token_or_slave($db);

    $author = remote_user_name($data);
    $name = array_get($data, 'name');
    $start_run_id = array_get($data, 'startRun');
    $end_run_id = array_get($data, 'endRun');

    $segmentation_name = array_get($data, 'segmentationStrategy');
    $test_range_name = array_get($data, 'testRangeStrategy');

    if (!$name)
        exit_with_error('MissingName', array('name' => $name));
    $range = array('startRunId' => $start_run_id, 'endRunId' => $end_run_id);
    if (!$start_run_id || !$end_run_id)
        exit_with_error('MissingRange', $range);

    $start_run = ensure_row_by_id($db, 'test_runs', 'run', $start_run_id, 'InvalidStartRun', $range);
    $end_run = ensure_row_by_id($db, 'test_runs', 'run', $end_run_id, 'InvalidEndRun', $range);

    $config = ensure_config_from_runs($db, $start_run, $end_run);

    $start_run_time = time_for_run($db, $start_run_id);
    $end_run_time = time_for_run($db, $end_run_id);
    if (!$start_run_time || !$end_run_time)
        exit_with_error('InvalidTimeRange', array('startTime' => $start_run_time, 'endTime' => $end_run_time));

    $db->begin_transaction();

    $segmentation_id = NULL;
    if ($segmentation_name) {
        $segmentation_id = $db->select_or_insert_row('analysis_strategies', 'strategy', array('name' => $segmentation_name));
        if (!$segmentation_id) {
            $db->rollback_transaction();
            exit_with_error('CannotFindOrInsertSegmentationStrategy', array('segmentationStrategy' => $segmentation_name));
        }
    }

    $test_range_id = NULL;
    if ($test_range_name) {
        $test_range_id = $db->select_or_insert_row('analysis_strategies', 'strategy', array('name' => $test_range_name));
        if (!$test_range_id) {
            $db->rollback_transaction();
            exit_with_error('CannotFindOrInsertTestRangeStrategy', array('testRangeStrategy' => $test_range_name));
        }
    }

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
        'start_run_time' => $start_run_time,
        'end_run' => $end_run_id,
        'end_run_time' => $end_run_time,
        'segmentation' => $segmentation_id,
        'test_range' => $test_range_id));
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

function time_for_run($db, $run_id) {
    $result = $db->query_and_fetch_all('SELECT max(commit_time) as time
        FROM test_runs JOIN builds ON run_build = build_id
            JOIN build_commits ON commit_build = build_id
            JOIN commits ON build_commit = commit_id
        WHERE run_id = $1', array($run_id));
    return $result ? $result[0]['time'] : null;
}

main();

?>
