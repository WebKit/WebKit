<?php

require('../include/json-header.php');

function main($path) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    if (count($path) > 0 && $path[0]) {
        $group_id = intval($path[0]);
        $group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $group_id));
        if (!$group)
            exit_with_error('GroupNotFound', array('id' => $group_id));
        $test_groups = array($group);
        $build_requests = fetch_build_requests_for_group($db, $group_id);
    } else {
        $task_id = array_get($_GET, 'task');
        if (!$task_id)
            exit_with_error('TaskIdNotSpecified');

        $test_groups = fetch_test_groups_for_task($db, $task_id);
        if (!is_array($test_groups))
            exit_with_error('FailedToFetchTestGroups');
        $build_requests = fetch_build_requests_for_task($db, $task_id);
    }
    if (!is_array($build_requests))
        exit_with_error('FailedToFetchBuildRequests');

    $test_groups = array_map("format_test_group", $test_groups);
    $group_by_id = array();
    foreach ($test_groups as &$group)
        $group_by_id[$group['id']] = &$group;

    $build_requests = array_map("format_build_request", $build_requests);
    foreach ($build_requests as $request)
        array_push($group_by_id[$request['testGroup']]['buildRequests'], $request['id']);

    exit_with_success(array('testGroups' => $test_groups, 'buildRequests' => $build_requests));
}

function fetch_test_groups_for_task($db, $task_id) {
    return $db->select_rows('analysis_test_groups', 'testgroup', array('task' => $task_id));
}

function fetch_build_requests_for_task($db, $task_id) {
    return $db->query_and_fetch_all('SELECT * FROM build_requests, builds
        WHERE request_build = build_id
            AND request_group IN (SELECT testgroup_id FROM analysis_test_groups WHERE testgroup_task = $1)
        ORDER BY request_group, request_order', array($task_id));
}

function fetch_build_requests_for_group($db, $test_group_id) {
    return $db->query_and_fetch_all('SELECT * FROM build_requests, builds
        WHERE request_build = build_id AND request_group = $1 ORDER BY request_order', array($test_group_id));
}

date_default_timezone_set('UTC');
function format_test_group($group_row) {
    return array(
        'id' => $group_row['testgroup_id'],
        'task' => $group_row['testgroup_task'],
        'name' => $group_row['testgroup_name'],
        'author' => $group_row['testgroup_author'],
        'createdAt' => strtotime($group_row['testgroup_created_at']) * 1000,
        'buildRequests' => array(),
    );
}

function format_build_request($request_row) {
    return array(
        'id' => $request_row['request_id'],
        'testGroup' => $request_row['request_group'],
        'order' => $request_row['request_order'],
        'rootSet' => $request_row['request_root_set'],
        'build' => $request_row['request_build'],
        'builder' => $request_row['build_builder'],
        'buildNumber' => $request_row['build_number'],
        'buildTime' => $request_row['build_time'] ? strtotime($request_row['build_time']) * 1000 : NULL,
    );
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
