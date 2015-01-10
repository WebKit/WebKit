<?php

require_once('../include/json-header.php');
require_once('../include/build-requests-fetcher.php');

function main($path) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    $build_requests_fetcher = new BuildRequestsFetcher($db);

    if (count($path) > 0 && $path[0]) {
        $group_id = intval($path[0]);
        $group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $group_id));
        if (!$group)
            exit_with_error('GroupNotFound', array('id' => $group_id));
        $test_groups = array($group);
        $build_requests_fetcher->fetch_for_group($group_id);
    } else {
        $task_id = array_get($_GET, 'task');
        if (!$task_id)
            exit_with_error('TaskIdNotSpecified');

        $test_groups = $db->select_rows('analysis_test_groups', 'testgroup', array('task' => $task_id));
        if (!is_array($test_groups))
            exit_with_error('FailedToFetchTestGroups');
        $build_requests_fetcher->fetch_for_task($task_id);
    }
    if (!$build_requests_fetcher->has_results())
        exit_with_error('FailedToFetchBuildRequests');

    $test_groups = array_map("format_test_group", $test_groups);
    $group_by_id = array();
    foreach ($test_groups as &$group)
        $group_by_id[$group['id']] = &$group;

    $build_requests = $build_requests_fetcher->results();
    foreach ($build_requests as $request)
        array_push($group_by_id[$request['testGroup']]['buildRequests'], $request['id']);

    exit_with_success(array('testGroups' => $test_groups, 'buildRequests' => $build_requests));
}

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

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
