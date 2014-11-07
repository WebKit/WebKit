<?php

require('../include/json-header.php');

function main($path) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    if (count($path) > 0 && $path[0]) {
        $task_id = intval($path[0]);
        $task = $db->select_first_row('analysis_tasks', 'task', array('id' => $task_id));
        if (!$task)
            exit_with_error('TaskNotFound', array('id' => $task_id));
        $tasks = array($task);
    } else {
        // FIXME: Limit the number of tasks we fetch.
        $tasks = array_reverse($db->fetch_table('analysis_tasks', 'task_created_at'));
        if (!is_array($tasks))
            exit_with_error('FailedToFetchTasks');
    }

    exit_with_success(array('analysisTasks' => array_map("format_task", $tasks)));
}

date_default_timezone_set('UTC');
function format_task($task_row) {
    return array(
        'id' => $task_row['task_id'],
        'name' => $task_row['task_name'],
        'author' => $task_row['task_author'],
        'createdAt' => strtotime($task_row['task_created_at']) * 1000,
        'platform' => $task_row['task_platform'],
        'metric' => $task_row['task_metric'],
        'startRun' => $task_row['task_start_run'],
        'endRun' => $task_row['task_end_run'],
    );
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
