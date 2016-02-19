<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $test_group_id = array_get($data, 'group');
    if (!$test_group_id)
        exit_with_error('TestGroupNotSpecified');

    $values = array();

    if (array_key_exists('name', $data))
        $values['name'] = $data['name'];

    if (array_key_exists('hidden', $data))
        $values['hidden'] = Database::to_database_boolean($data['hidden']);

    if (!$values)
        exit_with_error('NothingToUpdate');

    $db = connect();
    $db->begin_transaction();

    if (!$db->update_row('analysis_test_groups', 'testgroup', array('id' => $test_group_id), $values)) {
        $db->rollback_transaction();
        exit_with_error('FailedToUpdateTestGroup', array('id' => $test_group_id, 'values' => $values));
    }

    if (array_get($data, 'hidden')) {
        $db->query_and_get_affected_rows('UPDATE build_requests SET request_status = $1
            WHERE request_group = $2 AND request_status = $3', array('canceled', $test_group_id, 'pending'));
    }

    $db->commit_transaction();

    exit_with_success();
}

main();

?>
