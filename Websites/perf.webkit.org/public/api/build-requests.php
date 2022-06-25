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
        verify_worker($db, $report);
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
        $status_description = array_get($info, 'statusDescription');
        $build_request_for_root_reuse_id = array_get($info, 'buildRequestForRootReuse');
        $request_row = $db->select_first_row('build_requests', 'request', array('id' => $id));

        $fields_to_update = array();
        if (array_key_exists('url', $info))
            $fields_to_update['url'] = $url;
        if (array_key_exists('statusDescription', $info))
            $fields_to_update['status_description'] = $status_description;

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
            $fields_to_update['status'] = 'failed';
            $db->update_row('build_requests', 'request', array('id' => $id), $fields_to_update);
        } else {
            if (!in_array($status, array('pending', 'scheduled', 'running', 'failed', 'completed', 'canceled'))) {
                $db->rollback_transaction();
                exit_with_error('UnknownBuildRequestStatus', array('buildRequest' => $id, 'status' => $status));
            }
            $fields_to_update['status'] = $status;
            $db->update_row('build_requests', 'request', array('id' => $id), $fields_to_update);
            if ($build_request_for_root_reuse_id) {
                $build_request_for_root_reuse_id = intval($build_request_for_root_reuse_id);
                $build_request_for_root_reuse = $db->select_first_row('build_requests', 'request', array('id' => $build_request_for_root_reuse_id));
                if (!$build_request_for_root_reuse) {
                    $db->rollback_transaction();
                    exit_with_error('FailedToFindbuildRequestForRootReuse', array('buildRequest' => $build_request_for_root_reuse_id));
                }
                if ($build_request_for_root_reuse['request_status'] != 'completed') {
                    $db->rollback_transaction();
                    exit_with_error('CanOnlyReuseCompletedBuildRequest', array('buildRequest' => $build_request_for_root_reuse_id, 'status' => $build_request_for_root_reuse['request_status']));
                }
                $error = reuse_roots_in_commit_set($db, $build_request_for_root_reuse['request_commit_set'], $request_row['request_commit_set']);
                if ($error) {
                    $db->rollback_transaction();
                    exit_with_error($error['status'], $error['details']);
                }
            }
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

function reuse_roots_in_commit_set($db, $source_id, $destination_id) {
    $commit_set_items_source = $db->query_and_fetch_all('SELECT * FROM commit_set_items WHERE commitset_set = $1 AND commitset_commit IS NOT NULL', array($source_id));
    $commit_set_items_destination = $db->query_and_fetch_all('SELECT * FROM commit_set_items WHERE commitset_set = $1 AND commitset_commit IS NOT NULL', array($destination_id));
    if (count($commit_set_items_source) != count($commit_set_items_destination)) {
        return array('status' => 'CannotReuseRootWithNonMatchingCommitSets', 'details' => array('sourceCommitSet' => $source_id,
            'destinationCommitSet' => $destination_id));
    }

    $root_file_ids = array();
    foreach ($commit_set_items_destination as &$destination_item) {
        $source_item = NULL;
        foreach ($commit_set_items_source as &$item) {
            if ($destination_item['commitset_commit'] == $item['commitset_commit']
                && $destination_item['commitset_patch_file'] == $item['commitset_patch_file']
                && $destination_item['commitset_commit_owner'] == $item['commitset_commit_owner']
                && $destination_item['commitset_requires_build'] == $item['commitset_requires_build']) {
                $source_item = $item;
                break;
            }
        }
        if (!$source_item)
            return array('status' => 'NoMatchingCommitSetItem', 'details' => array('commitSet' => $source_id));

        $root_file_id = $source_item['commitset_root_file'];
        if (!$root_file_id) {
            if (!$db->is_true($source_item['commitset_requires_build']))
                continue;
            return array('status' => 'MissingRootFileFromSourceCommitSet', 'details' => array('commitSet' => $destination_id, 'rootFile' => $root_file_id));
        }

        $root_file_row = $db->select_first_row('uploaded_files', 'file', array('id' => $root_file_id));
        if ($root_file_row['file_deleted_at'])
            return array('status' => 'CannotReuseDeletedRoot', 'details' => array('commitSet' => $destination_id, 'rootFile' => $root_file_id));

        $db->update_row('commit_set_items', 'commitset', array('set' => $destination_id, 'commit' => $destination_item['commitset_commit']),
            array('root_file' => $root_file_id), 'set');

        array_push($root_file_ids, $root_file_id);
    }

    // There could be a race that those root files get purged after updating commit_set_items.
    // Add another round of check to ensure they are not deleted by the end of this function to
    // mitigate the race condition.
    foreach ($root_file_ids as &$root_file_id) {
        $root_file_row = $db->select_first_row('uploaded_files', 'file', array('id' => $root_file_id));
        if ($root_file_row['file_deleted_at'])
            return array('status' => 'CannotReuseDeletedRoot', 'details' => array('commitSet' => $destination_id, 'rootFile' => $root_file_id));
    }

    return NULL;
}

main(array_get($_GET, 'id'),
    array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array(),
    file_get_contents("php://input"));

?>