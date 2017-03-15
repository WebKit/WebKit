<?php

require_once('../include/json-header.php');

function main() {
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
    $commit_sets_info = array_get($data, 'commitSets');

    require_format('Task', $task_id, '/^\d+$/');
    if (!$commit_sets_info)
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

    $commit_sets = ensure_commit_sets($db, $commit_sets_info);

    $db->begin_transaction();

    $commit_set_id_list = array();
    foreach ($commit_sets as $commit_list) {
        $commit_set_id = $db->insert_row('commit_sets', 'commitset', array());
        foreach ($commit_list as $commit)
            $db->insert_row('commit_set_relationships', 'commitset', array('set' => $commit_set_id, 'commit' => $commit), 'commit');
        array_push($commit_set_id_list, $commit_set_id);
    }

    $group_id = $db->insert_row('analysis_test_groups', 'testgroup',
        array('task' => $task['task_id'], 'name' => $name, 'author' => $author));

    $order = 0;
    for ($i = 0; $i < $repetition_count; $i++) {
        foreach ($commit_set_id_list as $commit_set_id) {
            $db->insert_row('build_requests', 'request', array(
                'triggerable' => $triggerable['id'],
                'platform' => $triggerable['platform'],
                'test' => $triggerable['test'],
                'group' => $group_id,
                'order' => $order,
                'commit_set' => $commit_set_id));
            $order++;
        }
    }

    $db->commit_transaction();

    exit_with_success(array('testGroupId' => $group_id));
}

function ensure_commit_sets($db, $commit_sets_info) {
    $repository_name_to_id = array();
    foreach ($db->fetch_table('repositories') as $row)
        $repository_name_to_id[$row['repository_name']] = $row['repository_id'];

    $commit_sets = array();
    foreach ($commit_sets_info as $repository_name => $revisions) {
        $repository_id = array_get($repository_name_to_id, $repository_name);
        if (!$repository_id)
            exit_with_error('RepositoryNotFound', array('name' => $repository_name));

        foreach ($revisions as $i => $revision) {
            $commit = $db->select_first_row('commits', 'commit', array('repository' => $repository_id, 'revision' => $revision));
            if (!$commit)
                exit_with_error('RevisionNotFound', array('repository' => $repository_name, 'revision' => $revision));
            array_set_default($commit_sets, $i, array());
            array_push($commit_sets[$i], $commit['commit_id']);
        }
    }

    if (count($commit_sets) < 2)
        exit_with_error('InvalidCommitSets', array('commitSets' => $commit_sets_info));

    $commit_count_per_set = count($commit_sets[0]);
    foreach ($commit_sets as $commits) {
        if ($commit_count_per_set != count($commits))
            exit_with_error('InvalidCommitSets', array('commitSets' => $commit_sets));
    }

    return $commit_sets;
}

main();

?>
