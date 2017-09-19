<?php

require_once('../include/json-header.php');
require_once('../include/commit-log-fetcher.php');
require_once('../include/repository-group-finder.php');

function main()
{
    $db = connect();
    $data = ensure_privileged_api_data_and_token_or_slave($db);
    $author = remote_user_name($data);

    $arguments = validate_arguments($data, array(
        'name' => '/.+/',
        'task' => 'int?',
        'repetitionCount' => 'int?',
    ));
    $name = $arguments['name'];
    $task_id = array_get($arguments, 'task');
    $task_name = array_get($data, 'taskName');
    $repetition_count = $arguments['repetitionCount'];
    $platform_id = array_get($data, 'platform');
    $test_id = array_get($data, 'test');
    $revision_set_list = array_get($data, 'revisionSets');
    $commit_sets_info = array_get($data, 'commitSets'); // V2 UI compatibility

    if (!$task_id == !$task_name)
        exit_with_error('InvalidTask');

    if ($task_id)
        require_format('Task', $task_id, '/^\d+$/');
    if ($task_name || $platform_id || $test_id) {
        require_format('Platform', $platform_id, '/^\d+$/');
        require_format('Test', $test_id, '/^\d+$/');
    }

    if (!$revision_set_list && !$commit_sets_info)
        exit_with_error('InvalidCommitSets');

    if ($repetition_count === null)
        $repetition_count = 1;
    else if ($repetition_count < 1)
        exit_with_error('InvalidRepetitionCount', array('repetitionCount' => $repetition_count));

    $triggerable_id = NULL;
    if ($task_id) {
        $task = $db->select_first_row('analysis_tasks', 'task', array('id' => $task_id));
        if (!$task)
            exit_with_error('InvalidTask', array('task' => $task_id));

        $duplicate_test_group = $db->select_first_row('analysis_test_groups', 'testgroup', array('task' => $task_id, 'name' => $name));
        if ($duplicate_test_group)
            exit_with_error('DuplicateTestGroupName', array('task' => $task_id, 'testGroup' => $duplicate_test_group['testgroup_id']));

        $triggerable = find_triggerable_for_task($db, $task_id);
        if ($triggerable) {
            $triggerable_id = $triggerable['id'];
            if (!$platform_id && !$test_id) {
                $platform_id = $triggerable['platform'];
                $test_id = $triggerable['test'];
            } else {
                if ($triggerable['platform'] && $platform_id != $triggerable['platform'])
                    exit_with_error('InconsistentPlatform', array('groupPlatform' => $platform_id, 'taskPlatform' => $triggerable['platform']));
                if ($triggerable['test'] && $test_id != $triggerable['test'])
                    exit_with_error('InconsistentTest', array('groupTest' => $test_id, 'taskTest' => $triggerable['test']));
            }
        }
    }
    if (!$triggerable_id && $platform_id && $test_id) {
        $triggerable_configuration = $db->select_first_row('triggerable_configurations', 'trigconfig',
            array('test' => $test_id, 'platform' => $platform_id));
        if ($triggerable_configuration)
            $triggerable_id = $triggerable_configuration['trigconfig_triggerable'];
    }

    if (!$triggerable_id)
        exit_with_error('TriggerableNotFoundForTask', array('task' => $task_id, 'platform' => $platform_id, 'test' => $test_id));

    if ($revision_set_list)
        $commit_sets = commit_sets_from_revision_sets($db, $triggerable_id, $revision_set_list);
    else // V2 UI compatibility
        $commit_sets = ensure_commit_sets($db, $triggerable_id, $commit_sets_info);

    $db->begin_transaction();

    if ($task_name)
        $task_id = $db->insert_row('analysis_tasks', 'task', array('name' => $task_name, 'author' => $author));

    $configuration_list = array();
    $repository_group_with_builds = array();
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
        array_push($configuration_list, array('commit_set' => $commit_set_id, 'repository_group' => $repository_group));
    }

    $build_count = 0;
    foreach ($configuration_list as &$config_item) {
        if (array_get($repository_group_with_builds, $config_item['repository_group'])) {
            $config_item['need_to_build'] = TRUE;
            $build_count++;
        }
    }

    $group_id = $db->insert_row('analysis_test_groups', 'testgroup',
        array('task' => $task_id, 'name' => $name, 'author' => $author));

    if ($build_count) {
        $order = -$build_count;
        foreach ($configuration_list as $config) {
            if (!array_get($config, 'need_to_build'))
                continue;
            assert($order < 0);
            $db->insert_row('build_requests', 'request', array(
                'triggerable' => $triggerable_id,
                'repository_group' => $config['repository_group'],
                'platform' => $platform_id,
                'test' => NULL,
                'group' => $group_id,
                'order' => $order,
                'commit_set' => $config['commit_set']));
            $order++;
        }
    }

    $order = 0;
    for ($i = 0; $i < $repetition_count; $i++) {
        foreach ($configuration_list as $config) {
            $db->insert_row('build_requests', 'request', array(
                'triggerable' => $triggerable_id,
                'repository_group' => $config['repository_group'],
                'platform' => $platform_id,
                'test' => $test_id,
                'group' => $group_id,
                'order' => $order,
                'commit_set' => $config['commit_set']));
            $order++;
        }
    }

    $db->commit_transaction();

    exit_with_success(array('taskId' => $task_id, 'testGroupId' => $group_id));
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
        $required_owner_commits = array();
        $owner_commits_in_set = array();
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
                $required_owner_commits[$owner_commit_id] = $commit_id;
            } else
                $owner_commits_in_set[$commit_id] = TRUE;

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

        foreach($required_owner_commits as $required_owner_commit => $owned_commit) {
            if (!array_get($owner_commits_in_set, $required_owner_commit, FALSE))
                exit_with_error('CommitOwnerMustExistInCommitSet', array('owner_commit' => $required_owner_commit, 'owned_commit' => $owned_commit));
        }
        array_push($commit_set_list, array('repository_group' => $repository_group_id, 'set' => $commit_set));
    }

    foreach (array_keys($repositories_require_build) as $repository_id) {
        foreach($commit_set_items_by_repository[$repository_id] as &$commit_set_item)
            $commit_set_item['requires_build'] = TRUE;
    }
    return $commit_set_list;
}

function ensure_commit_sets($db, $triggerable_id, $commit_sets_info) {
    $repository_name_to_id = array();
    foreach ($db->select_rows('repositories', 'repository', array('owner' => NULL)) as $row)
        $repository_name_to_id[$row['repository_name']] = $row['repository_id'];

    $commit_sets = array();
    $repository_list = array();
    foreach ($commit_sets_info as $repository_name => $revisions) {
        $repository_id = array_get($repository_name_to_id, $repository_name);
        if (!$repository_id)
            exit_with_error('RepositoryNotFound', array('name' => $repository_name));
        array_push($repository_list, $repository_id);

        foreach ($revisions as $i => $revision) {
            $commit = $db->select_first_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $revision));
            if (!$commit)
                exit_with_error('RevisionNotFound', array('repository' => $repository_name, 'revision' => $revision));
            array_set_default($commit_sets, $i, array('set' => array()));
            array_push($commit_sets[$i]['set'], array('commit' => $commit['commit_id'], 'patch_file' => NULL, 'requires_build' => FALSE, 'commit_owner' => NULL));
        }
    }

    $finder = new RepositoryGroupFinder($db, $triggerable_id);
    $repository_group_id = $finder->find_by_repositories($repository_list);
    if (!$repository_group_id)
        exit_with_error('NoMatchingRepositoryGroup', array('repositories' => $repository_list));

    if (count($commit_sets) < 2)
        exit_with_error('InvalidCommitSets', array('commitSets' => $commit_sets_info));

    $commit_count_per_set = count($commit_sets[0]['set']);
    foreach ($commit_sets as &$commits) {
        $commits['repository_group'] = $repository_group_id;
        if ($commit_count_per_set != count($commits['set']))
            exit_with_error('InvalidCommitSets', array('commitSets' => $commit_sets));
    }

    return $commit_sets;
}

main();

?>
