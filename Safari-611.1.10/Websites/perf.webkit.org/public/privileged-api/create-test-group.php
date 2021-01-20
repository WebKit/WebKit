<?php

require_once('../include/json-header.php');
require_once('../include/commit-sets-helpers.php');

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
    $needs_notification = array_get($data, 'needsNotification', False);
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

    $group_id = create_test_group_and_build_requests($db, $commit_sets, $task_id, $name, $author, $triggerable_id, $platform_id, $test_id, $repetition_count, $needs_notification);

    $db->commit_transaction();

    exit_with_success(array('taskId' => $task_id, 'testGroupId' => $group_id));
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
