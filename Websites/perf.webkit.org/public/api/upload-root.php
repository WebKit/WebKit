<?php

require_once('../include/json-header.php');
require_once('../include/uploaded-file-helpers.php');

function main()
{
    $input_file = validate_uploaded_file('rootFile');

    $db = connect();
    $worker_id = verify_worker($db, $_POST);

    if (array_key_exists('buildNumber', $_POST) && array_key_exists('buildTag', $_POST) && $_POST['buildNumber'] != $_POST['buildTag'])
        exit_with_error('BuilderNumberTagMismatch', array('buildNumber' => $_POST['buildNumber'], 'buildTag' => $_POST['buildTag']));
    elseif (array_key_exists('buildNumber', $_POST) && !array_key_exists('buildTag', $_POST)) {
        require_format('buildNumber', $_POST['buildNumber'], '/^\d+$/');
        $_POST['buildTag'] = $_POST['buildNumber'];
    }

    $arguments = validate_arguments($_POST, array(
        'builderName' => '/.+/',
        'buildTag' => '/.+/',
        'buildTime' => '/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d*Z?$/',
        'buildRequest' => 'int',
        'repositoryList' => 'json',
    ));
    $build_request_id = $arguments['buildRequest'];

    $request_row = $db->select_first_row('build_requests', 'request', array('id' => $build_request_id));
    if (!$request_row || $request_row['request_test'] || $request_row['request_order'] >= 0)
        exit_with_error('InvalidBuildRequestType', array('buildRequest' => $build_request_id));

    $test_group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $request_row['request_group']));
    assert($test_group);

    $builder_id = $db->select_or_insert_row('builders', 'builder', array('name' => $arguments['builderName']));
    $build_info = array('builder' => $builder_id, 'worker' => $worker_id, 'tag' => $arguments['buildTag'], 'time' => $arguments['buildTime']);

    $commit_set_id = $request_row['request_commit_set'];
    $commit_set_items_to_update = compute_commit_set_items_to_update($db, $commit_set_id, $arguments['repositoryList']);

    $uploaded_file = upload_file_in_transaction($db, $input_file, $test_group['testgroup_author'],
        function ($db, $file_row) use ($build_request_id, $build_info, $commit_set_id, $commit_set_items_to_update) {
            $root_file_id = $file_row['file_id'];
            // FIXME: Insert a build row and associated with the request.
            $build_id = $db->insert_row('builds', 'build', $build_info);
            if (!$build_id)
                return array('status' => 'FailedToCreateBuild', 'build' => $build_info);

            foreach ($commit_set_items_to_update as $commit_id) {
                if (!$db->update_row('commit_set_items', 'commitset',
                    array('set' => $commit_set_id, 'commit' => $commit_id),
                    array('commit' => $commit_id, 'root_file' => $root_file_id), '*'))
                    return array('status' => 'FailedToUpdateCommitSet', 'commitSet' => $commit_set_id, 'commit' => $commit_id);
            }
            if (!all_commit_set_items_are_satisfied($db, $commit_set_id))
                return NULL;
            if (!$db->update_row('build_requests', 'request', array('id' => $build_request_id), array('status' => 'completed', 'build' => $build_id)))
                return array('status' => 'FailedToUpdateBuildRequest', 'buildRequest' => $build_request_id);
            return NULL;
        });

    exit_with_success(array('uploadedFile' => $uploaded_file));
}

function all_commit_set_items_are_satisfied($db, $commit_set_id)
{
    return !$db->select_first_row('commit_set_items', 'commitset', array('root_file' => NULL, 'requires_build' => TRUE, 'set' => $commit_set_id));
}

function compute_commit_set_items_to_update($db, $commit_set_id, $repository_name_list)
{
    if (!is_array($repository_name_list))
        exit_with_error('InvalidRepositoryList', array('repositoryList' => $repository_name_list));

    $commit_repository_rows_in_set = $db->query_and_fetch_all('SELECT commit_set_items.commitset_commit as commitset_commit,
        owned.repository_name as repository_name, owner.repository_name as owner_repository_name
        FROM repositories as owned LEFT OUTER JOIN repositories as owner ON owned.repository_owner = owner.repository_id, commits, commit_set_items
        WHERE owned.repository_id = commit_repository AND commit_id = commitset_commit
            AND commitset_set = $1', array($commit_set_id));

    $commit_by_repository_name = array();
    $commit_by_owner_and_owned_repository_names = array();
    if ($commit_repository_rows_in_set) {
        foreach ($commit_repository_rows_in_set as $row)
            if ($row['owner_repository_name']) {
                $owned_repositories = &array_ensure_item_has_array($commit_by_owner_and_owned_repository_names, $row['owner_repository_name']);
                $owned_repositories[$row['repository_name']] = $row['commitset_commit'];
            } else
                $commit_by_repository_name[$row['repository_name']] = $row['commitset_commit'];
    }

    $commit_set_items_to_update = array();
    foreach ($repository_name_list as $repository_name) {
        $commit_id = NULL;
        if (is_string($repository_name))
            $commit_id = array_get($commit_by_repository_name, $repository_name);
        else if (is_array($repository_name)) {
            if (!array_key_exists('ownerRepository', $repository_name) || !array_key_exists('ownedRepository', $repository_name))
                exit_with_error('InvalidKeyForRepository', array('repositoryName' => $repository_name, 'commitSet' => $commit_set_id));
            $commit_id = array_get(array_get($commit_by_owner_and_owned_repository_names, $repository_name['ownerRepository'], array()), $repository_name['ownedRepository']);
        }
        if (!$commit_id)
            exit_with_error('InvalidRepository', array('repositoryName' => $repository_name, 'commitSet' => $commit_set_id));
        array_push($commit_set_items_to_update, $commit_id);
    }
    if (!$commit_set_items_to_update)
        exit_with_error('InvalidRepositoryList', array('repositoryList' => $repository_name_list, 'commitSet' => $commit_set_id));

    return $commit_set_items_to_update;
}

main();

?>
