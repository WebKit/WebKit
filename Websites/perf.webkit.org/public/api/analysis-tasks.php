<?php

require_once('../include/json-header.php');
require_once('../include/commit-log-fetcher.php');

function main($path) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    $build_request_id = array_get($_GET, 'buildRequest');
    $task_id = count($path) > 0 && $path[0] ? $path[0] : array_get($_GET, 'id');

    if ($build_request_id) {
        $tasks = $db->query_and_fetch_all('SELECT analysis_tasks.* FROM build_requests, analysis_test_groups, analysis_tasks
            WHERE request_id = $1 AND request_group = testgroup_id AND testgroup_task = task_id', array(intval($build_request_id)));
        if (!$tasks)
            exit_with_error('TaskNotFound', array('buildRequest' => $build_request_id));
    } else if ($task_id) {
        $task_id = intval($task_id);
        $task = $db->select_first_row('analysis_tasks', 'task', array('id' => $task_id));
        if (!$task)
            exit_with_error('TaskNotFound', array('id' => $task_id));
        $tasks = array($task);
    } else {
        $metric_id = array_get($_GET, 'metric');
        $platform_id = array_get($_GET, 'platform');
        if (!!$metric_id != !!$platform_id)
            exit_with_error('InvalidArguments', array('metricId' => $metric_id, 'platformId' => $platform_id));

        if ($metric_id)
            $tasks = $db->select_rows('analysis_tasks', 'task', array('platform' => $platform_id, 'metric' => $metric_id));
        else {
            // FIXME: Limit the number of tasks we fetch.
            $tasks = array_reverse($db->fetch_table('analysis_tasks', 'task_created_at'));
        }

        if (!is_array($tasks))
            exit_with_error('FailedToFetchTasks');
    }

    $tasks = array_map("format_task", $tasks);
    exit_with_success(fetch_associated_data_for_tasks($db, $tasks));
}

function fetch_associated_data_for_tasks($db, &$tasks) {
    $task_ids = array();
    $task_by_id = array();
    foreach ($tasks as &$task) {
        array_push($task_ids, $task['id']);
        $task_by_id[$task['id']] = &$task;
    }

    $bugs = $db->query_and_fetch_all('SELECT bug_id AS "id", bug_task AS "task", bug_tracker AS "bugTracker", bug_number AS "number"
        FROM bugs WHERE bug_task = ANY ($1)', array('{' . implode(', ', $task_ids) . '}'));
    if (!is_array($bugs))
        exit_with_error('FailedToFetchBugs');

    foreach ($bugs as $bug) {
        $associated_task = &$task_by_id[$bug['task']];
        array_push($associated_task['bugs'], $bug['id']);
    }

    $commit_log_fetcher = new CommitLogFetcher($db);
    $commits = $commit_log_fetcher->fetch_for_tasks($task_ids, $task_by_id);
    if (!is_array($commits))
        exit_with_error('FailedToFetchCommits');

    $task_build_counts = $db->query_and_fetch_all('SELECT
        testgroup_task AS "task",
        count(testgroup_id) as "total",
        sum(case when request_status = \'failed\' or request_status = \'completed\' or request_status = \'canceled\' then 1 else 0 end) as "finished"
        FROM analysis_test_groups, build_requests
        WHERE request_group = testgroup_id AND testgroup_task = ANY($1) GROUP BY testgroup_task',
        array('{' . implode(', ', $task_ids) . '}'));
    if (!is_array($task_build_counts))
        exit_with_error('FailedToFetchTestGroups');

    foreach ($task_build_counts as $build_count) {
        $task = &$task_by_id[$build_count['task']];
        $task['buildRequestCount'] = $build_count['total'];
        $task['finishedBuildRequestCount'] = $build_count['finished'];
    }

    return array('analysisTasks' => $tasks, 'bugs' => $bugs, 'commits' => $commits);
}

function format_task($task_row) {
    return array(
        'id' => $task_row['task_id'],
        'name' => $task_row['task_name'],
        'author' => $task_row['task_author'],
        'segmentationStrategy' => $task_row['task_segmentation'],
        'testRangeStragegy' => $task_row['task_test_range'],
        'createdAt' => strtotime($task_row['task_created_at']) * 1000,
        'platform' => $task_row['task_platform'],
        'metric' => $task_row['task_metric'],
        'startRun' => $task_row['task_start_run'],
        'startRunTime' => Database::to_js_time($task_row['task_start_run_time']),
        'endRun' => $task_row['task_end_run'],
        'endRunTime' => Database::to_js_time($task_row['task_end_run_time']),
        'result' => $task_row['task_result'],
        'needed' => $task_row['task_needed'] ? Database::is_true($task_row['task_needed']) : null,
        'bugs' => array(),
        'causes' => array(),
        'fixes' => array(),
    );
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
