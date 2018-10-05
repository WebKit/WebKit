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

    if (!count($path)) {
        $task_id = array_get($_GET, 'task');
        if (!$task_id)
            exit_with_error('TaskIdNotSpecified');

        $test_groups = $db->select_rows('analysis_test_groups', 'testgroup', array('task' => $task_id));
        if (!is_array($test_groups))
            exit_with_error('FailedToFetchTestGroups');
        $build_requests_fetcher->fetch_for_task($task_id);
    } elseif ($path[0] == 'ready-for-notification') {
        $test_groups = $db->query_and_fetch_all("SELECT * FROM analysis_test_groups
            WHERE EXISTS(SELECT 1 FROM build_requests
                WHERE request_group = testgroup_id
                    AND request_status IN ('pending', 'scheduled', 'running', 'canceled')) IS FALSE
                    AND testgroup_needs_notification IS TRUE AND testgroup_hidden IS FALSE");

        if (!count($test_groups)) {
            exit_with_success(array('testGroups' => array(),
                'buildRequests' => array(),
                'commitSets' => array(),
                'commits' => array(),
                'uploadedFiles' => array()));
        }
        $build_requests_fetcher->fetch_requests_for_groups($test_groups);
    } elseif ($path[0] == 'need-more-requests') {
        $test_groups = $db->select_rows('analysis_test_groups', 'testgroup', array("hidden" => FALSE, "may_need_more_requests" => TRUE));
        if (!count($test_groups)) {
            exit_with_success(array('testGroups' => array(),
                'buildRequests' => array(),
                'commitSets' => array(),
                'commits' => array(),
                'uploadedFiles' => array()));
        }
        $build_requests_fetcher->fetch_requests_for_groups($test_groups);
    } else {
        $group_id = intval($path[0]);
        $group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $group_id));
        if (!$group)
            exit_with_error('GroupNotFound', array('id' => $group_id));
        $test_groups = array($group);
        $build_requests_fetcher->fetch_for_group($group['testgroup_task'], $group_id);
    }
    if (!$build_requests_fetcher->has_results())
        exit_with_error('FailedToFetchBuildRequests');

    $test_groups = array_map("format_test_group", $test_groups);
    $group_by_id = array();
    foreach ($test_groups as &$group) {
        $group_id = $group['id'];
        $group_by_id[$group_id] = &$group;
        $first_request = $db->select_first_row('build_requests', 'request', array('group' => $group_id), 'order');
        if ($first_request)
            $group['platform'] = $first_request['request_platform'];
    }

    $build_requests = $build_requests_fetcher->results();
    foreach ($build_requests as $request) {
        $request_group = &$group_by_id[$request['testGroup']];
        array_push($request_group['buildRequests'], $request['id']);
        array_push($request_group['commitSets'], $request['commitSet']);
    }

    exit_with_success(array('testGroups' => $test_groups,
        'buildRequests' => $build_requests,
        'commitSets' => $build_requests_fetcher->commit_sets(),
        'commits' => $build_requests_fetcher->commits(),
        'uploadedFiles' => $build_requests_fetcher->uploaded_files()));
}

function format_test_group($group_row) {
    return array(
        'id' => $group_row['testgroup_id'],
        'task' => $group_row['testgroup_task'],
        'name' => $group_row['testgroup_name'],
        'author' => $group_row['testgroup_author'],
        'createdAt' => Database::to_js_time($group_row['testgroup_created_at']),
        'notificationSentAt' => Database::to_js_time($group_row['testgroup_notification_sent_at']),
        'hidden' => Database::is_true($group_row['testgroup_hidden']),
        'needsNotification' => Database::is_true($group_row['testgroup_needs_notification']),
        'mayNeedMoreRequests' => Database::is_true($group_row['testgroup_may_need_more_requests']),
        'initialRepetitionCount' => $group_row['testgroup_initial_repetition_count'],
        'buildRequests' => array(),
        'commitSets' => array(),
    );
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
