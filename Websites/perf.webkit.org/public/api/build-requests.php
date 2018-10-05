<?php

require_once('../include/json-header.php');
require_once('../include/build-requests-fetcher.php');

function main($id, $path, $post_data) {
    if (!$id && (count($path) < 1 || count($path) > 2))
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $report = $post_data ? json_decode($post_data, true) : array();
    $updates = array_get($report, 'buildRequestUpdates');
    if ($updates) {
        verify_slave($db, $report);
        update_builds($db, $updates);
    }

    $requests_fetcher = new BuildRequestsFetcher($db);

    if ($id)
        $requests_fetcher->fetch_request($id);
    else {
        $triggerable_query = array('name' => array_get($path, 0));
        $triggerable = $db->select_first_row('build_triggerables', 'triggerable', $triggerable_query);
        if (!$triggerable)
            exit_with_error('TriggerableNotFound', $triggerable_query);
        $requests_fetcher->fetch_incomplete_requests_for_triggerable($triggerable['triggerable_id']);
    }

    $resolve_id = array_get($_GET, 'useLegacyIdResolution');
    exit_with_success(array(
        'buildRequests' => $resolve_id ? $requests_fetcher->results_with_resolved_ids() : $requests_fetcher->results(),
        'commitSets' => $requests_fetcher->commit_sets(),
        'commits' => $requests_fetcher->commits(),
        'uploadedFiles' => $requests_fetcher->uploaded_files(),
    ));
}

function update_builds($db, $updates) {
    $db->begin_transaction();
    $test_groups_may_need_more_requests = array();
    foreach ($updates as $id => $info) {
        $id = intval($id);
        $status = $info['status'];
        $url = array_get($info, 'url');
        $request_row = $db->select_first_row('build_requests', 'request', array('id' => $id));
        if ($status == 'failedIfNotCompleted') {
            if (!$request_row) {
                $db->rollback_transaction();
                exit_with_error('FailedToFindBuildRequest', array('buildRequest' => $id));
            }
            if ($request_row['request_status'] == 'completed')
                continue;
            $is_build = $request_row['request_order'] < 0;
            if ($is_build) {
                $db->query_and_fetch_all('UPDATE build_requests SET request_status = \'failed\'
                    WHERE request_group = $1 AND request_order > $2',
                    array($request_row['request_group'], $request_row['request_order']));
            }
            $db->update_row('build_requests', 'request', array('id' => $id), array('status' => 'failed', 'url' => $url));
        } else {
            if (!in_array($status, array('pending', 'scheduled', 'running', 'failed', 'completed', 'canceled'))) {
                $db->rollback_transaction();
                exit_with_error('UnknownBuildRequestStatus', array('buildRequest' => $id, 'status' => $status));
            }
            $db->update_row('build_requests', 'request', array('id' => $id), array('status' => $status, 'url' => $url));
            $test_group_id = $request_row['request_group'];
            if ($status != 'failed')
                continue;
        }

        $test_group_id = $request_row['request_group'];
        if (array_key_exists($test_group_id, $test_groups_may_need_more_requests))
            continue;

        $db->update_row('analysis_test_groups', 'testgroup', array('id' => $test_group_id), array('may_need_more_requests' => TRUE));
        $test_groups_may_need_more_requests[$test_group_id] = TRUE;
    }
    $db->commit_transaction();
}

main(array_get($_GET, 'id'),
    array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array(),
    file_get_contents("php://input"));

?>
