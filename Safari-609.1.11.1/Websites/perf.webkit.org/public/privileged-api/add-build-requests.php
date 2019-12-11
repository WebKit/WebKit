<?php

require_once('../include/json-header.php');
require_once('../include/commit-sets-helpers.php');

function main() {
    $db = connect();
    $data = ensure_privileged_api_data_and_token_or_slave($db);

    $additional_build_request_count = array_get($data, 'addCount');
    if (!$additional_build_request_count)
        exit_with_error('RequestCountNotSpecified');

    $test_group_id = array_get($data, 'group');
    if (!$test_group_id)
        exit_with_error('TestGroupNotSpecified');

    $test_group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $test_group_id));
    if (!$test_group)
        exit_with_error('InvalidTestGroup');

    if (Database::is_true($test_group['testgroup_hidden']))
        exit_with_error('CannotAddToHiddenTestGroup');

    $existing_build_requests = $db->select_rows('build_requests', 'request', array('group' => $test_group_id), 'order');

    $current_order = $existing_build_requests[count($existing_build_requests) - 1]['request_order'];
    if ($current_order < 0)
        exit_with_error('NoTestingBuildRequests');

    $commit_sets = array();
    $build_request_by_commit_set = array();
    foreach ($existing_build_requests as $build_request) {
        if ($build_request['request_order'] < 0)
            continue;
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
            $db->insert_row('build_requests', 'request', array(
                'triggerable' => $build_request['request_triggerable'],
                'repository_group' => $build_request['request_repository_group'],
                'platform' => $build_request['request_platform'],
                'test' => $build_request['request_test'],
                'group' => $build_request['request_group'],
                'order' => ++$current_order,
                'commit_set' => $build_request['request_commit_set']));
        }
    }
    $db->commit_transaction();

    exit_with_success();
}

main();

?>