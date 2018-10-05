<?php

require_once('repository-group-finder.php');
require_once('commit-log-fetcher.php');

# FIXME: Should create a helper class for below 3 helper functions to avoid passing long argument list.
function create_test_group_and_build_requests($db, $commit_sets, $task_id, $name, $author, $triggerable_id, $platform_id, $test_id, $repetition_count, $needs_notification) {

    list ($build_configuration_list, $test_configuration_list) = insert_commit_sets_and_construct_configuration_list($db, $commit_sets);

    $group_id = $db->insert_row('analysis_test_groups', 'testgroup',
        array('task' => $task_id, 'name' => $name, 'author' => $author, 'needs_notification' => $needs_notification, 'initial_repetition_count' => $repetition_count));

    $build_count = count($build_configuration_list);
    $order = -$build_count;
    foreach($build_configuration_list as $build_configuration)
        insert_build_request_for_configuration($db, $build_configuration, $order++, $triggerable_id, $platform_id, NULL, $group_id);

    for ($i = 0; $i < $repetition_count; $i++) {
        foreach($test_configuration_list as $test_configuration)
            insert_build_request_for_configuration($db, $test_configuration, $order++, $triggerable_id, $platform_id, $test_id, $group_id);
    }
    return $group_id;
}

function insert_commit_sets_and_construct_configuration_list($db, $commit_sets)
{
    $repository_group_with_builds = array();
    $test_configuration_list = array();
    $build_configuration_list = array();

    foreach ($commit_sets as $commit_list) {
        $commit_set_id = $db->insert_row('commit_sets', 'commitset', array());
        $need_to_build = FALSE;
        foreach ($commit_list['set'] as $commit_row) {
            $commit_row['set'] = $commit_set_id;
            $requires_build =  $commit_row['requires_build'];
            assert(is_bool($requires_build));
            $need_to_build = $need_to_build || $requires_build;
            $db->insert_row('commit_set_items', 'commitset', $commit_row, 'commit');
        }
        $repository_group = $commit_list['repository_group'];
        if ($need_to_build)
            $repository_group_with_builds[$repository_group] = TRUE;
        array_push($test_configuration_list, array('commit_set' => $commit_set_id, 'repository_group' => $repository_group));
    }

    foreach ($test_configuration_list as &$config) {
        if (array_get($repository_group_with_builds, $config['repository_group']))
            array_push($build_configuration_list, $config);
    }
    return array($build_configuration_list, $test_configuration_list);
}

function insert_build_request_for_configuration($db, $configuration, $order, $triggerable_id, $platform_id, $test_id, $group_id)
{
    $db->insert_row('build_requests', 'request', array(
        'triggerable' => $triggerable_id,
        'repository_group' => $configuration['repository_group'],
        'platform' => $platform_id,
        'test' => $test_id,
        'group' => $group_id,
        'order' => $order,
        'commit_set' => $configuration['commit_set']));
}

function commit_sets_from_revision_sets($db, $triggerable_id, $revision_set_list)
{
    if (count($revision_set_list) < 2)
        exit_with_error('InvalidRevisionSets', array('revisionSets' => $revision_set_list));

    $finder = new RepositoryGroupFinder($db, $triggerable_id);
    $commit_set_list = array();
    $repository_owner_list = array();
    $repositories_require_build = array();
    $commit_set_items_by_repository = array();
    foreach ($revision_set_list as $revision_set) {
        if (!count($revision_set))
            exit_with_error('InvalidRevisionSets', array('revisionSets' => $revision_set_list));

        $commit_set = array();
        $repository_list = array();
        $repository_with_patch = array();
        foreach ($revision_set as $repository_id => $data) {
            if ($repository_id == 'customRoots') {
                $file_id_list = $data;
                foreach ($file_id_list as $file_id) {
                    if (!is_numeric($file_id) || !$db->select_first_row('uploaded_files', 'file', array('id' => $file_id)))
                        exit_with_error('InvalidUploadedFile', array('file' => $file_id));
                    array_push($commit_set, array('root_file' => $file_id, 'patch_file' => NULL, 'requires_build' => FALSE, 'commit_owner' => NULL));
                }
                continue;
            }
            if (!is_numeric($repository_id))
                exit_with_error('InvalidRepository', array('repository' => $repository_id));

            if (!is_array($data))
                exit_with_error('InvalidRepositoryData', array('repository' => $repository_id, 'data' => $data));

            $revision = array_get($data, 'revision');
            if (!$revision)
                exit_with_error('InvalidRevision', array('repository' => $repository_id, 'data' => $data));
            $commit_id = CommitLogFetcher::find_commit_id_by_revision($db, $repository_id, $revision);
            if ($commit_id < 0)
                exit_with_error('AmbiguousRevision', array('repository' => $repository_id, 'revision' => $revision));
            if (!$commit_id)
                exit_with_error('RevisionNotFound', array('repository' => $repository_id, 'revision' => $revision));

            $owner_revision = array_get($data, 'ownerRevision');
            $patch_file_id = array_get($data, 'patch');
            if ($patch_file_id) {
                if (!is_numeric($patch_file_id) || !$db->select_first_row('uploaded_files', 'file', array('id' => $patch_file_id)))
                    exit_with_error('InvalidPatchFile', array('patch' => $patch_file_id));
                array_push($repository_with_patch, $repository_id);
                $repositories_require_build[$repository_id] =  TRUE;
            }

            $repository = NULL;
            $owner_commit_id = NULL;
            if ($owner_revision) {
                $repository = $db->select_first_row('repositories', 'repository', array('id' => intval($repository_id)));
                if (!$repository)
                    exit_with_error('RepositoryNotFound', array('repository' => $repository_id));
                $owner_commit = $db->select_first_row('commits', 'commit', array('repository' => $repository['repository_owner'], 'revision' => $owner_revision));
                if (!$owner_commit)
                    exit_with_error('InvalidOwnerRevision', array('repository' => $repository['repository_owner'], 'revision' => $owner_revision));
                if (!$db->select_first_row('commit_ownerships', 'commit', array('owned' => $commit_id, 'owner' => $owner_commit['commit_id'])))
                    exit_with_error('InvalidCommitOwnership', array('commitOwner' => $owner_commit['commit_id'], 'commitOwned' => $commit_id));
                $repositories_require_build[$repository_id] =  TRUE;
                $owner_commit_id = $owner_commit['commit_id'];
            }

            array_push($commit_set, array('commit' => $commit_id, 'patch_file' => $patch_file_id, 'requires_build' => FALSE, 'commit_owner' => $owner_commit_id));

            array_ensure_item_has_array($commit_set_items_by_repository, $repository_id);
            $commit_set_items_by_repository[$repository_id][] = &$commit_set[count($commit_set) - 1];

            if ($owner_commit_id)
                continue;
            array_push($repository_list, $repository_id);
        }
        $repository_group_id = $finder->find_by_repositories($repository_list);
        if (!$repository_group_id)
            exit_with_error('NoMatchingRepositoryGroup', array('repositories' => $repository_list));

        foreach ($repository_with_patch as $repository_id) {
            if (!$finder->accepts_patch($repository_group_id, $repository_id))
                exit_with_error('PatchNotAccepted', array('repository' => $repository_id, 'repositoryGroup' => $repository_group_id));
        }

        array_push($commit_set_list, array('repository_group' => $repository_group_id, 'set' => $commit_set));
    }

    foreach (array_keys($repositories_require_build) as $repository_id) {
        foreach($commit_set_items_by_repository[$repository_id] as &$commit_set_item)
            $commit_set_item['requires_build'] = TRUE;
    }
    return $commit_set_list;
}
?>