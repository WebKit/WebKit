<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $analysis_task_id = array_get($data, 'task');
    $bug_tracker_id = array_get($data, 'bugTracker');
    $bug_number = array_get($data, 'number');

    require_format('AnalysisTask', $analysis_task_id, '/^\d+$/');
    require_format('BugTracker', $bug_tracker_id, '/^\d+$/');
    require_format('BugNumber', $bug_number, '/^\d*$/');

    $db = connect();
    $db->begin_transaction();

    $bug_id = NULL;
    if (!$bug_number) {
        $count = $db->query_and_get_affected_rows("DELETE FROM bugs WHERE bug_task = $1 AND bug_tracker = $2",
            array($analysis_task_id, $bug_tracker_id));
        if ($count > 1) {
            $db->rollback_transaction();
            exit_with_error('UnexpectedNumberOfAffectedRows', array('affectedRows' => $count));
        }
    } else {
        $bug_id = $db->update_or_insert_row('bugs', 'bug', array('task' => $analysis_task_id, 'tracker' => $bug_tracker_id),
            array('task' => $analysis_task_id, 'tracker' => $bug_tracker_id, 'number' => $bug_number));
    }
    $db->commit_transaction();

    exit_with_success(array('bugId' => $bug_id));
}

main();

?>
