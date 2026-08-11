<?php

require_once('../include/json-header.php');

function main() {
    $data = ensure_privileged_api_data_and_token();

    $analysis_task_id = array_get($data, 'task');
    if (!$analysis_task_id)
        exit_with_error('AnalysisTaskNotSpecified');

    $values = array();

    if (array_key_exists('name', $data))
        $values['name'] = $data['name'];

    if (array_key_exists('result', $data)) {
        require_match_one_of_values('Result', $data['result'], array(null, 'progression', 'regression', 'unchanged', 'inconclusive'));
        $values['result'] = $data['result'];
    }

    if (array_key_exists('needed', $data)) {
        $needed = $data['needed'];
        if ($needed === null)
            $values['needed'] = null;
        else if (in_array($needed, array(0, false)))
            $values['needed'] = Database::to_database_boolean(false);
        else if (in_array($needed, array(1, true)))
            $values['needed'] = Database::to_database_boolean(true);
        else
            exit_with_error('InvalidValueForFeedback', array('value' => $data['needed']));
    }

    if (!$values)
        exit_with_error('NothingToUpdate');

    $db = connect();
    $db->begin_transaction();

    if (!$db->update_row('analysis_tasks', 'task', array('id' => $analysis_task_id), $values)) {
        $db->rollback_transaction();
        exit_with_error('FailedToUpdateTask', array('id' => $analysis_task_id, 'values' => $values));
    }

    $db->commit_transaction();

    exit_with_success();
}

main();

?>
