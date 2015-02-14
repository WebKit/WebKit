<?php

require_once('../include/json-header.php');
require_once('../include/build-requests-fetcher.php');

function main($path, $post_data) {
    if (count($path) < 1 || count($path) > 2)
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $triggerable_query = array('name' => array_get($path, 0));
    $triggerable = $db->select_first_row('build_triggerables', 'triggerable', $triggerable_query);
    if (!$triggerable)
        exit_with_error('TriggerableNotFound', $triggerable_query);

    $report = $post_data ? json_decode($post_data, true) : array();
    $updates = array_get($report, 'buildRequestUpdates');
    if ($updates) {
        verify_slave($db, $report);

        $db->begin_transaction();
        foreach ($updates as $id => $info) {
            $id = intval($id);
            $status = $info['status'];
            $url = array_get($info, 'url');
            if ($status == 'failedIfNotCompleted') {
                $db->query_and_get_affected_rows('UPDATE build_requests SET (request_status, request_url) = ($1, $2)
                    WHERE request_id = $3 AND request_status != $4', array('failed', $url, $id, 'completed'));
            } else {
                if (!in_array($status, array('pending', 'scheduled', 'running', 'failed', 'completed'))) {
                    $db->rollback_transaction();
                    exit_with_error('UnknownBuildRequestStatus', array('buildRequest' => $id, 'status' => $status));
                }
                $db->update_row('build_requests', 'request', array('id' => $id), array('status' => $status, 'url' => $url));
            } 
        }
        $db->commit_transaction();
    }

    $requests_fetcher = new BuildRequestsFetcher($db);
    $requests_fetcher->fetch_incomplete_requests_for_triggerable($triggerable['triggerable_id']);

    exit_with_success(array(
        'buildRequests' => $requests_fetcher->results_with_resolved_ids(),
        'rootSets' => $requests_fetcher->root_sets(),
        'updates' => $updates,
    ));
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array(),
    file_get_contents("php://input"));

?>
