<?php

require('../include/json-header.php');

function main($path) {
    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $task_id = array_get($_GET, 'task');
    $query = array();
    if ($task_id) {
        $triggerable = find_triggerable_for_task($db, $task_id);
        if (!$triggerable)
            exit_with_error('TriggerableNotFoundForTask', array('task' => $task_id));
        $query['id'] = $triggerable['id'];
    }

    $id_to_triggerable = array();
    foreach ($db->select_rows('build_triggerables', 'triggerable', $query) as $row) {
        $id = $row['triggerable_id'];
        $repositories = array();
        $id_to_triggerable[$id] = array('id' => $id, 'name' => $row['triggerable_name'], 'acceptedRepositories' => &$repositories);
    }

    foreach ($db->select_rows('triggerable_repositories', 'trigrepo', array()) as $row) {
        $triggerable = array_get($id_to_triggerable, $row['trigrepo_triggerable']);
        if ($triggerable)
            array_push($triggerable['acceptedRepositories'], $row['trigrepo_repository']);
    }

    exit_with_success(array('triggerables' => array_values($id_to_triggerable)));
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
