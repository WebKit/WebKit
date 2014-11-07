<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $run_id = array_get($data, 'run');
    $bug_tracker_id = array_get($data, 'tracker');
    $bug_number = array_get($data, 'bugNumber');

    if (!$run_id)
        exit_with_error('InvalidRunId', array('run' => $run_id));
    if (!$bug_tracker_id)
        exit_with_error('InvalidBugTrackerId', array('tracker' => $bug_tracker_id));

    $db = connect();
    $db->begin_transaction();

    $bug_id = NULL;
    if (!$bug_number) {
        $count = $db->query_and_get_affected_rows("DELETE FROM bugs WHERE bug_run = $1 AND bug_tracker = $2",
            array($run_id, $bug_tracker_id));
        if ($count > 1) {
            $db->rollback_transaction();
            exit_with_error('UnexpectedNumberOfAffectedRows', array('affectedRows' => $count));
        }
    } else {
        $bug_id = $db->update_or_insert_row('bugs', 'bug', array('run' => $run_id, 'tracker' => $bug_tracker_id),
            array('run' => $run_id, 'tracker' => $bug_tracker_id, 'number' => $bug_number));
    }
    $db->commit_transaction();

    exit_with_success(array('bugId' => $bug_id));
}

main();

?>
