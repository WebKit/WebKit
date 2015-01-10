<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $author = remote_user_name();

    $task_id = array_get($data, 'task');
    $name = array_get($data, 'name');
    $root_sets = array_get($data, 'rootSets');
    $repetition_count = intval(array_get($data, 'repetitionCount', 1));

    if (!$name)
        exit_with_error('MissingName');
    if (!$root_sets)
        exit_with_error('MissingRootSets');
    if ($repetition_count < 1)
        exit_with_error('InvalidRepetitionCount', array('repetitionCount' => $repetition_count));

    $db = connect();
    $task = $db->select_first_row('analysis_tasks', 'task', array('id' => $task_id));
    if (!$task)
        exit_with_error('InvalidTask', array('task' => $task_id));
    $triggerable = find_triggerable_for_task($db, $task_id);
    if (!$triggerable)
        exit_with_error('TriggerableNotFoundForTask', array('task' => $task_id));

    $commit_sets = commit_sets_from_root_sets($db, $root_sets);

    $db->begin_transaction();

    $root_set_id_list = array();
    foreach ($commit_sets as $commit_list) {
        $root_set_id = $db->insert_row('root_sets', 'rootset', array());
        foreach ($commit_list as $commit)
            $db->insert_row('roots', 'root', array('set' => $root_set_id, 'commit' => $commit), 'commit');
        array_push($root_set_id_list, $root_set_id);
    }

    $group_id = $db->insert_row('analysis_test_groups', 'testgroup',
        array('task' => $task['task_id'], 'name' => $name, 'author' => $author));

    $order = 0;
    for ($i = 0; $i < $repetition_count; $i++) {
        foreach ($root_set_id_list as $root_set_id) {
            $db->insert_row('build_requests', 'request', array(
                'triggerable' => $triggerable['id'],
                'platform' => $triggerable['platform'],
                'test' => $triggerable['test'],
                'group' => $group_id,
                'order' => $order,
                'root_set' => $root_set_id));
            $order++;
        }
    }

    $db->commit_transaction();

    exit_with_success(array('testGroupId' => $group_id));
}

function commit_sets_from_root_sets($db, $root_sets) {
    $repository_name_to_id = array();
    foreach ($db->fetch_table('repositories') as $row)
        $repository_name_to_id[$row['repository_name']] = $row['repository_id'];

    $commit_sets = array();
    foreach ($root_sets as $repository_name => $revisions) {
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

    $commit_count_per_set = count($commit_sets[0]);
    foreach ($commit_sets as $commits) {
        if ($commit_count_per_set != count($commits))
            exit_with_error('InvalidRootSets', array('rootSets' => $root_sets));
    }

    return $commit_sets;
}

main();

?>
