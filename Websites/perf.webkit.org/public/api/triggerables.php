<?php

require('../include/json-header.php');
require('../include/manifest-generator.php');

// V2 UI compatibility (detect-changes.js).
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

    $id_to_triggerable = ManifestGenerator::fetch_triggerables($db, $query);
    exit_with_success(array('triggerables' => array_values($id_to_triggerable)));
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
