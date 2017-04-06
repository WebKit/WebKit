<?php

require_once('../include/json-header.php');
require_once('../include/repository-group-finder.php');

function main($post_data)
{
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
    validate_configurations($db, $configurations);

    $repository_groups = array_get($report, 'repositoryGroups', array());
    validate_repository_groups($db, $repository_groups);

    $finder = new RepositoryGroupFinder($db, $triggerable_id);
    foreach ($repository_groups as &$group)
        $group['existingGroup'] = $finder->find_by_repositories($group['repositories']);

    $db->begin_transaction();
    if ($db->query_and_get_affected_rows('DELETE FROM triggerable_configurations WHERE trigconfig_triggerable = $1', array($triggerable_id)) === false) {
        $db->rollback_transaction();
        exit_with_error('FailedToDeleteExistingConfigurations', array('triggerable' => $triggerable_id));
    }

    foreach ($configurations as &$entry) {
        $config_info = array('test' => $entry['test'], 'platform' => $entry['platform'], 'triggerable' => $triggerable_id);
        if (!$db->insert_row('triggerable_configurations', 'trigconfig', $config_info, null)) {
            $db->rollback_transaction();
            exit_with_error('FailedToInsertConfiguration', array('entry' => $entry));
        }
    }

    foreach ($repository_groups as &$group) {
        $group_id = $group['existingGroup'];
        if ($group_id) {
            $group_info = array('name' => $group['name'], 'description' => array_get($group, 'description'));
            if (!$db->update_row('triggerable_repository_groups', 'repositorygroup', array('id' => $group_id), $group_info)) {
                $db->rollback_transaction();
                exit_with_error('FailedToInsertRepositoryGroup', array('repositoryGroup' => $group));
            }
        } else {
            $group_id = $db->update_or_insert_row('triggerable_repository_groups', 'repositorygroup',
                array('triggerable' => $triggerable_id, 'name' => $group['name']),
                array('triggerable' => $triggerable_id, 'name' => $group['name'], 'description' => array_get($group, 'description')));
            if (!$group_id) {
                $db->rollback_transaction();
                exit_with_error('FailedToInsertRepositoryGroup', array('repositoryGroup' => $group));
            }
        }
        if ($db->query_and_get_affected_rows('DELETE FROM triggerable_repositories WHERE trigrepo_group = $1', array($group_id)) === FALSE) {
            $db->rollback_transaction();
            exit_with_error('FailedToDisassociateRepositories', array('repositoryGroup' => $group));
        }
        foreach ($group['repositories'] as $repository_id) {
            if (!$db->insert_row('triggerable_repositories', 'trigrepo', array('group' => $group_id, 'repository' => $repository_id), null)) {
                $db->rollback_transaction();
                exit_with_error('FailedToAssociateRepository', array('repositoryGroup' => $group, 'repository' => $repository_id));
            }
        }
    }

    $db->commit_transaction();
    exit_with_success();
}

function validate_configurations($db, $configurations)
{
    if (!is_array($configurations))
        exit_with_error('InvalidConfigurations', array('configurations' => $configurations));

    foreach ($configurations as $entry) {
        if (!is_array($entry) || !array_key_exists('test', $entry) || !array_key_exists('platform', $entry))
            exit_with_error('InvalidConfigurationEntry', array('configurationEntry' => $entry));
    }
}

function validate_repository_groups($db, $repository_groups)
{
    if (!is_array($repository_groups))
        exit_with_error('InvalidRepositoryGroups', array('repositoryGroups' => $repository_groups));

    $top_level_repositories = $db->select_rows('repositories', 'repository', array('owner' => null));
    $top_level_repository_ids = array();
    foreach ($top_level_repositories as $repository_row)
        $top_level_repository_ids[$repository_row['repository_id']] = true;

    foreach ($repository_groups as &$group) {
        if (!is_array($group) || !array_key_exists('name', $group) || !array_key_exists('repositories', $group) || !is_array($group['repositories']))
            exit_with_error('InvalidRepositoryGroup', array('repositoryGroup' => $group));
        $repository_list = $group['repositories'];
        $group_repository_list = array();
        foreach ($repository_list as $repository_id) {
            if (!array_key_exists($repository_id, $top_level_repository_ids) || array_key_exists($repository_id, $group_repository_list))
                exit_with_error('InvalidRepository', array('repositoryGroup' => $group, 'repository' => $repository_id));
            $group_repository_list[$repository_id] = true;
        }
    }
}

main($HTTP_RAW_POST_DATA);

?>
