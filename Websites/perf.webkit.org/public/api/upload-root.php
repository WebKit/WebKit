<?php

require_once('../include/json-header.php');
require_once('../include/uploaded-file-helpers.php');

function main()
{
    $input_file = validate_uploaded_file('rootFile');

    $db = connect();
    $slave_id = verify_slave($db, $_POST);

    $arguments = validate_arguments($_POST, array(
        'builderName' => '/.+/',
        'buildNumber' => 'int',
        'buildTime' => '/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d*Z?$/',
        'buildRequest' => 'int',
        'repositoryList' => 'json',
    ));
    $build_request_id = $arguments['buildRequest'];

    $request_row = $db->select_first_row('build_requests', 'request', array('id' => $build_request_id));
    if ($request_row['request_test'] || $request_row['request_order'] >= 0)
        exit_with_error('InvalidBuildRequestType', array('buildRequest' => $build_request_id));

    $test_group = $db->select_first_row('analysis_test_groups', 'testgroup', array('id' => $request_row['request_group']));
    assert($test_group);

    $builder_id = $db->select_or_insert_row('builders', 'builder', array('name' => $arguments['builderName']));
    $build_info = array('builder' => $builder_id, 'slave' => $slave_id, 'number' => $arguments['buildNumber'], 'time' => $arguments['buildTime']);

    $commit_set_id = $request_row['request_commit_set'];
    $commit_set_items_to_update = compute_commit_set_items_to_update($db, $commit_set_id, $arguments['repositoryList']);

    $uploaded_file = upload_file_in_transaction($db, $input_file, $test_group['testgroup_author'],
        function ($db, $file_row) use ($build_request_id, $build_info, $commit_set_id, $commit_set_items_to_update) {
            $root_file_id = $file_row['file_id'];
            // FIXME: Insert a build row and associated with the request.
            $build_id = $db->insert_row('builds', 'build', $build_info);
            if (!$build_id)
                return array('status' => 'FailedToCreateBuild', 'build' => $build_info);
            if (!$db->update_row('build_requests', 'request', array('id' => $build_request_id), array('status' => 'completed', 'build' => $build_id)))
                return array('status' => 'FailedToUpdateBuildRequest', 'buildRequest' => $build_request_id);

            foreach ($commit_set_items_to_update as $commit_id) {
                if (!$db->update_row('commit_set_items', 'commitset',
                    array('set' => $commit_set_id, 'commit' => $commit_id),
                    array('commit' => $commit_id, 'root_file' => $root_file_id), '*'))
                    return array('status' => 'FailedToUpdateCommitSet', 'commitSet' => $commit_set_id, 'commit' => $commit_id);
            }
            return NULL;
        });

    exit_with_success(array('uploadedFile' => $uploaded_file));
}

function compute_commit_set_items_to_update($db, $commit_set_id, $repository_name_list)
{
    if (!is_array($repository_name_list))
        exit_with_error('InvalidRepositoryList', array('repositoryList' => $repository_name_list));

    $commit_repository_rows_in_set = $db->query_and_fetch_all('SELECT * FROM repositories, commits, commit_set_items
        WHERE repository_id = commit_repository AND commit_id = commitset_commit
            AND commitset_set = $1', array($commit_set_id));

    $commit_by_repository_name = array();
    if ($commit_repository_rows_in_set) {
        foreach ($commit_repository_rows_in_set as $row)
            $commit_by_repository_name[$row['repository_name']] = $row['commitset_commit'];
    }

    $commit_set_items_to_update = array();
    foreach ($repository_name_list as $repository_name) {
        $commit_id = array_get($commit_by_repository_name, $repository_name);
        if (!$commit_id)
            exit_with_error('InvalidRepository', array('repositoryName' => $repository_name, 'commitSet' => $commit_set_id));
        array_push($commit_set_items_to_update, $commit_id);
    }
    if (!$commit_set_items_to_update)
        exit_with_error('InvalidRepositoryList', array('repositoryList' => $repository_list));

    return $commit_set_items_to_update;
}

main();

?>
