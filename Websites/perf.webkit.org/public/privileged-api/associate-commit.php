<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $analysis_task_id = array_get($data, 'task');
    $repository_id = array_get($data, 'repository');
    $revision = array_get($data, 'revision');
    $kind = array_get($data, 'kind');
    $commit_id_to_diassociate = array_get($data, 'commit');

    $db = connect();
    $db->begin_transaction();

    require_format('AnalysisTask', $analysis_task_id, '/^\d+$/');
    if ($commit_id_to_diassociate) {
        require_format('Commit', $commit_id_to_diassociate, '/^\d*$/');

        $count = $db->query_and_get_affected_rows("DELETE FROM task_commits WHERE taskcommit_task = $1 AND taskcommit_commit = $2",
            array($analysis_task_id, $commit_id_to_diassociate));
        if ($count != 1) {
            $db->rollback_transaction();
            exit_with_error('UnexpectedNumberOfAffectedRows', array('affectedRows' => $count));
        }
    } else {
        require_format('Repository', $repository_id, '/^\d+$/');
        require_format('Kind', $kind, '/^(cause|fix)$/');

        $commit_info = array('repository' => $repository_id, 'revision' => $revision);
        $commit_rows = $db->query_and_fetch_all('SELECT commit_id FROM commits WHERE commit_repository = $1 AND commit_revision LIKE $2 LIMIT 2',
            array($repository_id, '%' . Database::escape_for_like($revision) . '%'));
        if (count($commit_rows) > 1) {
            $db->rollback_transaction();
            exit_with_error('AmbiguousRevision', $commit_info);            
        } else if (!$commit_rows) {
            $db->rollback_transaction();
            exit_with_error('CommitNotFound', $commit_info);
        }

        $commit_id = $commit_rows[0]['commit_id'];

        $association = array('task' => $analysis_task_id, 'commit' => $commit_id, 'is_fix' => Database::to_database_boolean($kind == 'fix'));
        $commit_id = $db->update_or_insert_row('task_commits', 'taskcommit',
            array('task' => $analysis_task_id, 'commit' => $commit_id), $association, 'commit');
        if (!$commit_id) {
            $db->rollback_transaction();
            exit_with_error('FailedToAssociateCommit', $association);
        }
    }

    $db->commit_transaction();

    exit_with_success();
}

main();

?>
