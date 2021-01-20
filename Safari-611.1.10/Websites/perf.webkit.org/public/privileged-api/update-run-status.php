<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $run_id = array_get($data, 'run');
    if (!$run_id)
        exit_with_error('MissingRunId');

    $db = connect();
    $run = $db->select_first_row('test_runs', 'run', array('id' => $run_id));
    if (!$run)
        exit_with_error('InvalidRun', array('run' => $run_id));

    $marked_outlier = array_get($data, 'markedOutlier');

    $db->begin_transaction();
    $db->update_row('test_runs', 'run', array('id' => $run_id), array(
        'id' => $run_id,
        'marked_outlier' => $marked_outlier ? 't' : 'f'));
    $db->commit_transaction();

    exit_with_success();
}

main();

?>
