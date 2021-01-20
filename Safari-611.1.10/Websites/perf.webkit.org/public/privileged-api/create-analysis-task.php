<?php

require_once('../include/json-header.php');
require_once('../include/commit-sets-helpers.php');

function main() {
    $db = connect();
    $data = ensure_privileged_api_data_and_token_or_slave($db);

    $author = remote_user_name($data);
    $name = array_get($data, 'name');
    $repetition_count = array_get($data, 'repetitionCount');
    $needs_notification = array_get($data, 'needsNotification', False);
    $test_group_name = array_get($data, 'testGroupName');
    $revision_set_list = array_get($data, 'revisionSets');

    $segmentation_name = array_get($data, 'segmentationStrategy');
    $test_range_name = array_get($data, 'testRangeStrategy');

    if (!$name)
        exit_with_error('MissingName', array('name' => $name));

    $range = validate_arguments($data, array('startRun' => 'int', 'endRun' => 'int'));

    $start_run = ensure_row_by_id($db, 'test_runs', 'run', $range['startRun'], 'InvalidStartRun', $range);
    $start_run_id = $start_run['run_id'];
    $end_run = ensure_row_by_id($db, 'test_runs', 'run', $range['endRun'], 'InvalidEndRun', $range);
    $end_run_id = $end_run['run_id'];

    $config = ensure_config_from_runs($db, $start_run, $end_run);

    $start_run_time = time_for_run($db, $start_run_id);
    $end_run_time = time_for_run($db, $end_run_id);
    if (!$start_run_time || !$end_run_time || $start_run_time == $end_run_time)
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

    if ($repetition_count) {
        $triggerable = find_triggerable_for_task($db, $task_id);
        if (!$triggerable || !$triggerable['id']) {
            $db->rollback_transaction();
            exit_with_error('TriggerableNotFoundForTask', array('task' => $task_id, 'platform' => $config['config_platform']));
        }
        if ($triggerable['platform'] != $config['config_platform']) {
            $db->rollback_transaction();
            exit_with_error('InconsistentPlatform', array('configPlatform' => $config['config_platform'], 'taskPlatform' => $triggerable['platform']));
        }
        $triggerable_id = $triggerable['id'];
        $test_id = $triggerable['test'];
        $commit_sets = commit_sets_from_revision_sets($db, $triggerable_id, $revision_set_list);
        create_test_group_and_build_requests($db, $commit_sets, $task_id, $test_group_name, $author, $triggerable_id, $config['config_platform'], $test_id, $repetition_count, $needs_notification);
    }

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
    $result = $db->query_and_fetch_all('SELECT max(commit_time) as time, max(build_time) as build_time
        FROM test_runs JOIN builds ON run_build = build_id
            JOIN build_commits ON commit_build = build_id
            JOIN commits ON build_commit = commit_id
        WHERE run_id = $1', array($run_id));

    $first_result = array_get($result, 0, array());
    return $first_result['time'] ? $first_result['time'] : $first_result['build_time'];
}

main();

?>
