<?php

require('../include/json-header.php');

function main($post_data) {
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
        $repository_id = $db->select_or_insert_row('repositories', 'repository', array('name' => $commit_info['repository']));
        if (!$repository_id) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertRepository', array('commit' => $commit_info));
        }

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

        $parent_revision = array_get($commit_info, 'parent');
        $parent_id = NULL;
        if ($parent_revision) {
            $parent_commit = $db->select_first_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $parent_revision));
            if (!$parent_commit) {
                $db->rollback_transaction();
                exit_with_error('FailedToFindParentCommit', array('commit' => $commit_info));
            }
            $parent_id = $parent_commit['commit_id'];
        }

        $data = array(
            'repository' => $repository_id,
            'revision' => $commit_info['revision'],
            'parent' => $parent_id,
            'order' => array_get($commit_info, 'order'),
            'time' => array_get($commit_info, 'time'),
            'committer' => $committer_id,
            'message' => array_get($commit_info, 'message'),
            'reported' => true,
        );
        $db->update_or_insert_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $data['revision']), $data);
    }
    $db->commit_transaction();

    exit_with_success();
}

main($HTTP_RAW_POST_DATA);

?>
