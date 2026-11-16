<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $analysis_task_id = array_get($data, 'task');
    $bug_tracker_id = array_get($data, 'bugTracker');
    $bug_number = array_get($data, 'number');
    $bug_id = array_get($data, 'bugToDelete');
    $should_delete = array_get($data, 'shouldDelete');

    $db = connect();
    $db->begin_transaction();

    if ($bug_id) { // V2 compatibility
        require_format('BugToDelete', $bug_id, '/^\d+$/');
        $count = $db->query_and_get_affected_rows("DELETE FROM bugs WHERE bug_id = $1", array($bug_id));
        if ($count != 1) {
            $db->rollback_transaction();
            exit_with_error('UnexpectedNumberOfAffectedRows', array('affectedRows' => $count));
        }
    } else {
        require_format('AnalysisTask', $analysis_task_id, '/^\d+$/');
        require_format('BugTracker', $bug_tracker_id, '/^\d+$/');
        require_format('BugNumber', $bug_number, '/^\d+$/');
        if ($should_delete) { // V3
            $count = $db->query_and_get_affected_rows("DELETE FROM bugs WHERE bug_task = $1 AND bug_tracker = $2 AND bug_number = $3",
                array($analysis_task_id, $bug_tracker_id, $bug_number));
            if ($count < 1) { // FIXME: We should forbid duplicate bugs (same bug number on same tracker for same task)
                $db->rollback_transaction();
                exit_with_error('UnexpectedNumberOfAffectedRows', array('affectedRows' => $count));
            }
        } else {
            $bug_id = $db->insert_row('bugs', 'bug', array('task' => $analysis_task_id, 'tracker' => $bug_tracker_id, 'number' => $bug_number));
        }
    }
    $db->commit_transaction();

    exit_with_success(array('bugId' => $bug_id));
}

main();

?>
