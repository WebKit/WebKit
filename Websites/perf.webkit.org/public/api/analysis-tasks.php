<?php

require('../include/json-header.php');

function main($path) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    $task_id = count($path) > 0 && $path[0] ? $path[0] : array_get($_GET, 'id');

    if ($task_id) {
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
    $bugs = fetch_and_push_bugs_to_tasks($db, $tasks);

    exit_with_success(array('analysisTasks' => $tasks, 'bugs' => $bugs));
}

function fetch_and_push_bugs_to_tasks($db, &$tasks) {
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

    $task_build_counts = $db->query_and_fetch_all('SELECT
        testgroup_task AS "task",
        count(testgroup_id) as "total",
        sum(case when request_status = \'failed\' or request_status = \'completed\' then 1 else 0 end) as "finished"
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

    $run_ids = array();
    $task_by_run = array();
    foreach ($tasks as &$task) {
        if ($task['startRun']) {
            array_push($run_ids, $task['startRun']);
            $task_by_run[$task['startRun']] = &$task;
        }
        if ($task['endRun']) {
            array_push($run_ids, $task['endRun']);
            $task_by_run[$task['endRun']] = &$task;
        }
    }

    // FIXME: This query is quite expensive. We may need to store this directly in analysis_tasks table instead.
    $build_revision_times = $db->query_and_fetch_all('SELECT run_id, build_time, max(commit_time) AS revision_time
            FROM builds
                LEFT OUTER JOIN build_commits ON commit_build = build_id
                LEFT OUTER JOIN commits ON build_commit = commit_id, test_runs
            WHERE run_build = build_id AND run_id = ANY($1) GROUP BY build_id, run_id',
        array('{' . implode(', ', $run_ids) . '}'));
    foreach ($build_revision_times as &$row) {
        $time = $row['revision_time'] or $row['build_time'];
        $id = $row['run_id'];
        if ($task_by_run[$id]['startRun'] == $id)
            $task_by_run[$id]['startRunTime'] = Database::to_js_time($time);
        if ($task_by_run[$id]['endRun'] == $id)
            $task_by_run[$id]['endRunTime'] = Database::to_js_time($time);
    }

    return $bugs;
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
        'endRun' => $task_row['task_end_run'],
        'category' => $task_row['task_result'] ? 'bisecting' : 'unconfirmed',
        'result' => $task_row['task_result'],
        'needed' => $task_row['task_needed'] ? Database::is_true($task_row['task_needed']) : null,
        'bugs' => array(),
    );
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
