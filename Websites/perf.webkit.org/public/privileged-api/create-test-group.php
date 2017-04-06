<?php

require_once('../include/json-header.php');
require_once('../include/repository-group-finder.php');

function main()
{
    $db = connect();
    $data = ensure_privileged_api_data_and_token_or_slave($db);
    $author = remote_user_name($data);

    $arguments = validate_arguments($data, array(
        'name' => '/.+/',
        'task' => 'int',
        'repetitionCount' => 'int?',
    ));
    $name = $arguments['name'];
    $task_id = $arguments['task'];
    $repetition_count = $arguments['repetitionCount'];
    $revision_set_list = array_get($data, 'revisionSets');
    $commit_sets_info = array_get($data, 'commitSets');

    require_format('Task', $task_id, '/^\d+$/');
    if (!$revision_set_list && !$commit_sets_info)
        exit_with_error('InvalidCommitSets');

    if ($repetition_count === null)
        $repetition_count = 1;
    else if ($repetition_count < 1)
        exit_with_error('InvalidRepetitionCount', array('repetitionCount' => $repetition_count));

    $task = $db->select_first_row('analysis_tasks', 'task', array('id' => $task_id));
    if (!$task)
        exit_with_error('InvalidTask', array('task' => $task_id));
    $triggerable = find_triggerable_for_task($db, $task_id);
    if (!$triggerable)
        exit_with_error('TriggerableNotFoundForTask', array('task' => $task_id));

    if ($revision_set_list)
        $commit_sets = commit_sets_from_revision_sets($db, $triggerable['id'], $revision_set_list);
    else // V2 UI compatibility
        $commit_sets = ensure_commit_sets($db, $triggerable['id'], $commit_sets_info);

    $db->begin_transaction();

    $configuration_list = array();
    foreach ($commit_sets as $commit_list) {
        $commit_set_id = $db->insert_row('commit_sets', 'commitset', array());
        foreach ($commit_list['set'] as $commit)
            $db->insert_row('commit_set_relationships', 'commitset', array('set' => $commit_set_id, 'commit' => $commit), 'commit');
        array_push($configuration_list, array('commit_set' => $commit_set_id, 'repository_group' => $commit_list['repository_group']));
    }

    $group_id = $db->insert_row('analysis_test_groups', 'testgroup',
        array('task' => $task['task_id'], 'name' => $name, 'author' => $author));

    $order = 0;
    for ($i = 0; $i < $repetition_count; $i++) {
        foreach ($configuration_list as $config) {
            $db->insert_row('build_requests', 'request', array(
                'triggerable' => $triggerable['id'],
                'repository_group' => $config['repository_group'],
                'platform' => $triggerable['platform'],
                'test' => $triggerable['test'],
                'group' => $group_id,
                'order' => $order,
                'commit_set' => $config['commit_set'],));
            $order++;
        }
    }

    $db->commit_transaction();

    exit_with_success(array('testGroupId' => $group_id));
}

function commit_sets_from_revision_sets($db, $triggerable_id, $revision_set_list)
{
    if (count($revision_set_list) < 2)
        exit_with_error('InvalidRevisionSets', array('revisionSets' => $revision_set_list));

    $finder = new RepositoryGroupFinder($db, $triggerable_id);
    $commit_set_list = array();
    foreach ($revision_set_list as $revision_set) {
        if (!count($revision_set))
            exit_with_error('InvalidRevisionSets', array('revisionSets' => $revision_set_list));

        $commit_set = array();
        $repository_list = array();

        foreach ($revision_set as $repository_id => $revision) {
            if (!is_numeric($repository_id))
                exit_with_error('InvalidRepository', array('repository' => $repository_id));
            $commit = $db->select_first_row('commits', 'commit',
                array('repository' => intval($repository_id), 'revision' => $revision));
            if (!$commit)
                exit_with_error('RevisionNotFound', array('repository' => $repository_id, 'revision' => $revision));
            array_push($commit_set, $commit['commit_id']);
            array_push($repository_list, $repository_id);
        }

        $repository_group_id = $finder->find_by_repositories($repository_list);
        if (!$repository_group_id)
            exit_with_error('NoMatchingRepositoryGroup', array('repositoris' => $repository_list));

        array_push($commit_set_list, array('repository_group' => $repository_group_id, 'set' => $commit_set));
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
            array_push($commit_sets[$i]['set'], $commit['commit_id']);
        }
    }

    $finder = new RepositoryGroupFinder($db, $triggerable_id);
    $repository_group_id = $finder->find_by_repositories($repository_list);
    if (!$repository_group_id)
        exit_with_error('NoMatchingRepositoryGroup', array('repositoris' => $repository_list));

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
