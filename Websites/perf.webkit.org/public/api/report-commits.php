<?php

require('../include/json-header.php');

function main($post_data)
{
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $report = json_decode($post_data, true);

    verify_slave($db, $report);

    $commits = array_get($report, 'commits', array());

    foreach ($commits as $commit_info) {
        if (!array_key_exists('repository', $commit_info))
            exit_with_error('MissingRepositoryName', array('commit' => $commit_info));
        if (!array_key_exists('revision', $commit_info))
            exit_with_error('MissingRevision', array('commit' => $commit_info));
        require_format('Revision', $commit_info['revision'], '/^[A-Za-z0-9 \.]+$/');
        if (array_key_exists('author', $commit_info) && !is_array($commit_info['author']))
            exit_with_error('InvalidAuthorFormat', array('commit' => $commit_info));
    }

    $db->begin_transaction();
    foreach ($commits as $commit_info) {
        $repository_id = $db->select_or_insert_row('repositories', 'repository', array('name' => $commit_info['repository'], 'owner' => NULL));
        if (!$repository_id) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertRepository', array('commit' => $commit_info));
        }
        $owner_commit_id = insert_commit($db, $commit_info, $repository_id, NULL);
        if (!array_key_exists('subCommits', $commit_info))
            continue;

        foreach($commit_info['subCommits'] as $sub_commit_repository_name => $sub_commit_info) {
            if (array_key_exists('time', $sub_commit_info)) {
                $db->rollback_transaction();
                exit_with_error('SubCommitShouldNotContainTimestamp', array('commit' => $sub_commit_info));
            }
            $sub_commit_repository_id = $db->select_or_insert_row('repositories', 'repository', array('name' => $sub_commit_repository_name, 'owner' => $repository_id));
            if (!$sub_commit_repository_id) {
                $db->rollback_transaction();
                exit_with_error('FailedToInsertRepository', array('commit' => $sub_commit_info));
            }
            insert_commit($db, $sub_commit_info, $sub_commit_repository_id, $owner_commit_id);
        }
    }
    $db->commit_transaction();

    exit_with_success();
}

function insert_commit($db, $commit_info, $repository_id, $owner_commit_id)
{
    $author = array_get($commit_info, 'author');
    $committer_id = NULL;
    if ($author) {
        $account = array_get($author, 'account');
        $committer_query = array('repository' => $repository_id, 'account' => $account);
        $committer_data = $committer_query;
        $name = array_get($author, 'name');
        if ($name)
            $committer_data['name'] = $name;
        $committer_id = $db->update_or_insert_row('committers', 'committer', $committer_query, $committer_data);
        if (!$committer_id) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertCommitter', array('committer' => $committer_data));
        }
    }

    $previous_commit_revision = array_get($commit_info, 'previousCommit');
    $previous_commit_id = NULL;
    if ($previous_commit_revision) {
        $previous_commit = $db->select_first_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $previous_commit_revision));
        if (!$previous_commit) {
            $db->rollback_transaction();
            exit_with_error('FailedToFindPreviousCommit', array('commit' => $commit_info));
        }
        $previous_commit_id = $previous_commit['commit_id'];
    }

    $data = array(
        'repository' => $repository_id,
        'revision' => $commit_info['revision'],
        'previous_commit' => $previous_commit_id,
        'order' => array_get($commit_info, 'order'),
        'time' => array_get($commit_info, 'time'),
        'committer' => $committer_id,
        'message' => array_get($commit_info, 'message'),
        'reported' => true,
    );
    $inserted_commit_id = $db->update_or_insert_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $data['revision']), $data);

    if ($owner_commit_id)
        $db->select_or_insert_row('commit_ownerships', 'commit', array('owner' => $owner_commit_id, 'owned' => $inserted_commit_id), NULL, '*');
    return $inserted_commit_id;
}

main($HTTP_RAW_POST_DATA);

?>
