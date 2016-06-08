<?php

require('../include/json-header.php');

function main($post_data) {
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $report = json_decode($post_data, true);
    verify_slave($db, $report);

    $triggerable_name = array_get($report, 'triggerable');
    $triggerable = $db->select_first_row('build_triggerables', 'triggerable', array('name' => $triggerable_name));
    if (!$triggerable)
        exit_with_error('TriggerableNotFound', array('triggerable' => $triggerable_name));
    $triggerable_id = $triggerable['triggerable_id'];

    $configurations = array_get($report, 'configurations');
    if (!is_array($configurations))
        exit_with_error('InvalidConfigurations', array('configurations' => $configurations));

    foreach ($configurations as $entry) {
        if (!is_array($entry) || !array_key_exists('test', $entry) || !array_key_exists('platform', $entry))
            exit_with_error('InvalidConfigurationEntry', array('configurationEntry' => $entry));
    }

    $db->begin_transaction();
    if ($db->query_and_get_affected_rows('DELETE FROM triggerable_configurations WHERE trigconfig_triggerable = $1', array($triggerable_id)) === false) {
        $db->rollback_transaction();
        exit_with_error('FailedToDeleteExistingConfigurations', array('triggerable' => $triggerable_id));
    }

    foreach ($configurations as $entry) {
        $config_info = array('test' => $entry['test'], 'platform' => $entry['platform'], 'triggerable' => $triggerable_id);
        if (!$db->insert_row('triggerable_configurations', 'trigconfig', $config_info, null)) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertConfiguration', array('entry' => $entry));
        }
    }

    $db->commit_transaction();
    exit_with_success();
}

main($HTTP_RAW_POST_DATA);

?>
