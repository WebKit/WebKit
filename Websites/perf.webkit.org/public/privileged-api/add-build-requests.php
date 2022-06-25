<?php

require_once('../include/json-header.php');
require_once('../include/commit-sets-helpers.php');

function main() {
    $db = connect();
    $data = ensure_privileged_api_data_and_token_or_worker($db);

    $arguments = validate_arguments($data, array(
        'addCount' => 'int',
        'group' => 'int',
        'commitSet' => 'int?',
    ));

    $additional_build_request_count = $arguments['addCount'];
    $test_group_id = $arguments['group'];
    $commit_set_id = array_get($arguments, 'commitSet');

    $test_group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $test_group_id));
    if (!$test_group)
        exit_with_error('InvalidTestGroup');

    if (Database::is_true($test_group['testgroup_hidden']))
        exit_with_error('CannotAddToHiddenTestGroup');

    $existing_build_requests = $db->select_rows('build_requests', 'request', array('group' => $test_group_id), 'order');

    $current_order = $existing_build_requests[count($existing_build_requests) - 1]['request_order'];
    if ($current_order < 0)
        exit_with_error('NoTestingBuildRequests');

    $repetition_type = $test_group['testgroup_repetition_type'];
    assert(in_array($repetition_type, ['alternating', 'sequential', 'paired-parallel']));
    $is_sequential_type = $repetition_type == 'sequential';
    if (!$is_sequential_type && $commit_set_id)
        exit_with_error('CommitSetNotSupportedRepetitionType');

    $existing_test_type_build_requests = array_filter($existing_build_requests, function($build_request) {
        return $build_request['request_order'] >= 0;
    });

    if ($is_sequential_type) {
        if ($commit_set_id)
            add_sequential_build_requests_for_commit_set($db, $existing_test_type_build_requests, $additional_build_request_count, $commit_set_id);
        else
            add_sequential_build_requests_for_all_commit_sets($db, $existing_build_requests, $additional_build_request_count, $test_group_id);
    } else
        add_alternating_or_paired_parallel_build_requests($db, $existing_test_type_build_requests, $additional_build_request_count, $current_order);

    exit_with_success();
}

function add_alternating_or_paired_parallel_build_requests($db, $existing_build_requests, $additional_build_request_count, $order)
{
    $commit_sets = array();
    $build_request_by_commit_set = array();
    foreach ($existing_build_requests as $build_request) {
        $requested_commit_set = $build_request['request_commit_set'];
        if (array_key_exists($requested_commit_set, $build_request_by_commit_set))
            continue;
        $build_request_by_commit_set[$requested_commit_set] = $build_request;
        array_push($commit_sets, $requested_commit_set);
    }

    $db->begin_transaction();
    for ($i = 0; $i < $additional_build_request_count; $i++) {
        foreach ($commit_sets as $commit_set) {
            $build_request = $build_request_by_commit_set[$commit_set];
            duplicate_build_request_with_new_order($db, $build_request, ++$order);
        }
    }
    $db->commit_transaction();
}

function add_sequential_build_requests_for_all_commit_sets($db, $existing_build_requests, $additional_build_request_count, $test_group_id)
{
    $order_shift_by_commit_set = array();
    $last_build_request_by_commit_set = array();
    foreach ($existing_build_requests as $current_build_request) {
        $commit_set_for_current_request = $current_build_request['request_commit_set'];
        if (!array_key_exists($commit_set_for_current_request, $last_build_request_by_commit_set))
            $order_shift_by_commit_set[$commit_set_for_current_request] = count($last_build_request_by_commit_set) * $additional_build_request_count;
        $last_build_request_by_commit_set[$commit_set_for_current_request] = $current_build_request;
    }

    $db->begin_transaction();
    foreach (array_reverse($order_shift_by_commit_set, true) as $commit_set => $shift) {
        if (!$shift)
            continue;
        $db->query('UPDATE build_requests SET request_order = request_order + $1
            WHERE request_commit_set = $2 AND request_group = $3',
            array($shift, $commit_set, $test_group_id));
    }
    foreach (array_keys($last_build_request_by_commit_set) as $commit_set) {
        $current_build_request = $last_build_request_by_commit_set[$commit_set];
        $last_existing_order = $current_build_request['request_order'];
        $order_shift = $order_shift_by_commit_set[$commit_set];
        for ($i = 1, $starting_order = $last_existing_order + $order_shift; $i <= $additional_build_request_count; $i++)
            duplicate_build_request_with_new_order($db, $current_build_request, $starting_order + $i);
    }
    $db->commit_transaction();
}

function add_sequential_build_requests_for_commit_set($db, $existing_build_requests, $additional_build_request_count, $target_commit_set_id)
{
    $found_target = false;
    foreach ($existing_build_requests as $current_build_request) {
        if ($current_build_request['request_commit_set'] == $target_commit_set_id) {
            $found_target = true;
            break;
        }
    }
    if (!$found_target)
        exit_with_error('NoCommitSetInTestGroup');

    $db->begin_transaction();
    foreach (array_reverse($existing_build_requests, true) as $current_build_request) {
        if ($current_build_request['request_commit_set'] == $target_commit_set_id) {
            for ($i = 0; $i < $additional_build_request_count; $i++)
                duplicate_build_request_with_new_order($db, $current_build_request, $current_build_request['request_order'] + $i + 1);
            break;
        } else {
            $db->query('UPDATE build_requests SET request_order = request_order + $1 WHERE request_id = $2',
                array($additional_build_request_count, $current_build_request['request_id']));
        }
    }
    $db->commit_transaction();
}

function duplicate_build_request_with_new_order($db, $build_request, $order_overwrite)
{
    $db->insert_row('build_requests', 'request', array(
        'triggerable' => $build_request['request_triggerable'],
        'repository_group' => $build_request['request_repository_group'],
        'platform' => $build_request['request_platform'],
        'test' => $build_request['request_test'],
        'group' => $build_request['request_group'],
        'order' => $order_overwrite,
        'commit_set' => $build_request['request_commit_set']));
}

main();

?>