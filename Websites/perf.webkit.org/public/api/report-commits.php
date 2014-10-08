<?php

require('../include/json-header.php');

function main($post_data) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $report = json_decode($post_data, true);

    verify_builder($db, $report);

    $commits = array_get($report, 'commits', array());

    foreach ($commits as $commit_info) {
        if (!array_key_exists('repository', $commit_info))
            exit_with_error('MissingRepositoryName', array('commit' => $commit_info));
        if (!array_key_exists('revision', $commit_info))
            exit_with_error('MissingRevision', array('commit' => $commit_info));
        if (!ctype_alnum($commit_info['revision']))
            exit_with_error('InvalidRevision', array('commit' => $commit_info));
        if (!array_key_exists('time', $commit_info))
            exit_with_error('MissingTimestamp', array('commit' => $commit_info));
        if (!array_key_exists('author', $commit_info) || !is_array($commit_info['author']))
            exit_with_error('MissingAuthorOrInvalidFormat', array('commit' => $commit_info));
    }

    $db->begin_transaction();
    foreach ($commits as $commit_info) {
        $repository_id = $db->select_or_insert_row('repositories', 'repository', array('name' => $commit_info['repository']));
        if (!$repository_id) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertRepository', array('commit' => $commit_info));
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
            'time' => $commit_info['time'],
            'author_name' => array_get($commit_info['author'], 'name'),
            'author_email' => array_get($commit_info['author'], 'email'),
            'message' => $commit_info['message'],
            'reported' => true,
        );
        $db->update_or_insert_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $data['revision']), $data);
    }
    $db->commit_transaction();

    exit_with_success();
}

function verify_builder($db, $report) {
    array_key_exists('builderName', $report) or exit_with_error('MissingBuilderName');
    array_key_exists('builderPassword', $report) or exit_with_error('MissingBuilderPassword');

    $builder_info = array(
        'name' => $report['builderName'],
        'password_hash' => hash('sha256', $report['builderPassword'])
    );

    $matched_builder = $db->select_first_row('builders', 'builder', $builder_info);
    if (!$matched_builder)
        exit_with_error('BuilderNotFound', array('name' => $builder_info['name']));
}

main($HTTP_RAW_POST_DATA);

?>
<?php

require('../include/json-header.php');

function main($post_data) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $report = json_decode($post_data, true);

    verify_builder($db, $report);

    $commits = array_get($report, 'commits', array());

    foreach ($commits as $commit_info) {
        if (!array_key_exists('repository', $commit_info))
            exit_with_error('MissingRepositoryName', array('commit' => $commit_info));
        if (!array_key_exists('revision', $commit_info))
            exit_with_error('MissingRevision', array('commit' => $commit_info));
        if (!ctype_alnum($commit_info['revision']))
            exit_with_error('InvalidRevision', array('commit' => $commit_info));
        if (!array_key_exists('time', $commit_info))
            exit_with_error('MissingTimestamp', array('commit' => $commit_info));
        if (!array_key_exists('author', $commit_info) || !is_array($commit_info['author']))
            exit_with_error('MissingAuthorOrInvalidFormat', array('commit' => $commit_info));
    }

    $db->begin_transaction();
    foreach ($commits as $commit_info) {
        $repository_id = $db->select_or_insert_row('repositories', 'repository', array('name' => $commit_info['repository']));
        if (!$repository_id) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertRepository', array('commit' => $commit_info));
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
            'time' => $commit_info['time'],
            'author_name' => array_get($commit_info['author'], 'name'),
            'author_email' => array_get($commit_info['author'], 'email'),
            'message' => $commit_info['message'],
            'reported' => true,
        );
        $db->update_or_insert_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $data['revision']), $data);
    }
    $db->commit_transaction();

    exit_with_success();
}

function verify_builder($db, $report) {
    array_key_exists('builderName', $report) or exit_with_error('MissingBuilderName');
    array_key_exists('builderPassword', $report) or exit_with_error('MissingBuilderPassword');

    $builder_info = array(
        'name' => $report['builderName'],
        'password_hash' => hash('sha256', $report['builderPassword'])
    );

    $matched_builder = $db->select_first_row('builders', 'builder', $builder_info);
    if (!$matched_builder)
        exit_with_error('BuilderNotFound', array('name' => $builder_info['name']));
}

main($HTTP_RAW_POST_DATA);

?>
