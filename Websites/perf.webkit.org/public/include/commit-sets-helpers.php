<?php

require_once('repository-group-finder.php');
require_once('commit-log-fetcher.php');

# FIXME: Should create a helper class for below 3 helper functions to avoid passing long argument list.
function create_test_group_and_build_requests($db, $commit_sets, $test_parameter_sets, $task_id, $name, $author,
                                              $triggerable_id, $platform_id, $test_id, $repetition_count,
                                              $repetition_type, $needs_notification)
{
    assert(in_array($repetition_type, array('alternating', 'sequential', 'paired-parallel')));

    if (is_null($test_parameter_sets)) {
        $test_parameter_sets = array();
    } elseif (count($test_parameter_sets) != count($commit_sets)) {
        exit_with_error('InconsistentCommitSetsAndTestParameterSets',
            array('revisionSets' => $commit_sets, 'TestParameterSets' => $test_parameter_sets));
    }

    list ($build_configuration_list, $test_configuration_list) = insert_commit_sets_test_parameter_sets_and_construct_configuration_list($db, $commit_sets, $test_parameter_sets);

    $group_id = $db->insert_row('analysis_test_groups', 'testgroup',
        array('task' => $task_id, 'name' => $name, 'author' => $author, 'needs_notification' => $needs_notification,
            'initial_repetition_count' => $repetition_count, 'repetition_type' => $repetition_type));

    $build_count = count($build_configuration_list);
    $order = -$build_count;
    foreach($build_configuration_list as $build_configuration)
        insert_build_request_for_configuration($db, $build_configuration, $order++, $triggerable_id, $platform_id, NULL, $group_id);

    if ($repetition_type == 'sequential') {
        foreach ($test_configuration_list as $test_configuration) {
            for ($i = 0; $i < $repetition_count; $i++)
                insert_build_request_for_configuration($db, $test_configuration, $order++, $triggerable_id, $platform_id, $test_id, $group_id);
        }
    } else {
        assert($repetition_type == 'alternating' || $repetition_type == 'paired-parallel');
        for ($i = 0; $i < $repetition_count; $i++) {
            foreach ($test_configuration_list as $test_configuration)
                insert_build_request_for_configuration($db, $test_configuration, $order++, $triggerable_id, $platform_id, $test_id, $group_id);
        }
    }
    return $group_id;
}

function insert_commit_sets_test_parameter_sets_and_construct_configuration_list($db, $commit_sets, $test_parameter_sets)
{
    $repository_group_with_builds = array();
    $test_configuration_list = array();
    $build_configuration_list = array();

    foreach (array_map(null, $commit_sets, $test_parameter_sets) as list($commit_list, $test_parameters_list)) {
        $commit_set_id = $db->insert_row('commit_sets', 'commitset', array());
        $need_to_build = FALSE;
        foreach ($commit_list['set'] as $commit_row) {
            $commit_row['set'] = $commit_set_id;
            $requires_build =  $commit_row['requires_build'];
            assert(is_bool($requires_build));
            $need_to_build = $need_to_build || $requires_build;
            $db->insert_row('commit_set_items', 'commitset', $commit_row, 'commit');
        }

        if (is_null($test_parameters_list)) {
            $test_parameter_set_id = NULL;
        } else {
            $test_parameter_set_id = $db->insert_row('test_parameter_sets', 'testparamset', array());
            foreach ($test_parameters_list['set'] as &$test_parameter) {
                $test_parameter['set'] = $test_parameter_set_id;
                $db->insert_row('test_parameter_set_items', 'testparamset', $test_parameter, 'set');
            }
            $need_to_build = $need_to_build || $test_parameters_list['need_to_build'];
        }

        $repository_group = $commit_list['repository_group'];
        if ($need_to_build)
            $repository_group_with_builds[$repository_group] = TRUE;

        $test_configuration_list[] = array('commit_set' => $commit_set_id, 'repository_group' => $repository_group,
            'test_parameter_set' => $test_parameter_set_id);
    }

    foreach ($test_configuration_list as &$config) {
        if (array_get($repository_group_with_builds, $config['repository_group']))
            $build_configuration_list[] = $config;
    }
    return array($build_configuration_list, $test_configuration_list);
}

function insert_build_request_for_configuration($db, $configuration, $order, $triggerable_id, $platform_id, $test_id, $group_id)
{
    $db->insert_row('build_requests', 'request', array(
        'triggerable' => $triggerable_id,
        'repository_group' => $configuration['repository_group'],
        'platform' => $platform_id,
        'test' => $test_id,
        'group' => $group_id,
        'order' => $order,
        'commit_set' => $configuration['commit_set'],
        'test_parameter_set' => $configuration['test_parameter_set']));
}

function commit_sets_from_revision_sets($db, $triggerable_id, $revision_set_list)
{
    if (count($revision_set_list) < 2)
        exit_with_error('InvalidRevisionSets', array('revisionSets' => $revision_set_list));

    $finder = new RepositoryGroupFinder($db, $triggerable_id);
    $commit_set_list = array();
    $repository_owner_list = array();
    $repositories_require_build = array();
    $commit_set_items_by_repository = array();
    foreach ($revision_set_list as $revision_set) {
        if (!count($revision_set))
            exit_with_error('InvalidRevisionSets', array('revisionSets' => $revision_set_list));

        $commit_set = array();
        $repository_list = array();
        $repository_with_patch = array();
        foreach ($revision_set as $repository_id => $data) {
            if ($repository_id == 'customRoots') {
                $file_id_list = $data;
                foreach ($file_id_list as $file_id) {
                    if (!is_numeric($file_id) || !$db->select_first_row('uploaded_files', 'file', array('id' => $file_id)))
                        exit_with_error('InvalidUploadedFile', array('file' => $file_id));
                    array_push($commit_set, array('root_file' => $file_id, 'patch_file' => NULL, 'requires_build' => FALSE, 'commit_owner' => NULL));
                }
                continue;
            }
            if (!is_numeric($repository_id))
                exit_with_error('InvalidRepository', array('repository' => $repository_id));

            if (!is_array($data))
                exit_with_error('InvalidRepositoryData', array('repository' => $repository_id, 'data' => $data));

            $revision = array_get($data, 'revision');
            if (!$revision)
                exit_with_error('InvalidRevision', array('repository' => $repository_id, 'data' => $data));
            $commit_id = CommitLogFetcher::find_commit_id_by_revision($db, $repository_id, $revision);
            if ($commit_id < 0)
                exit_with_error('AmbiguousRevision', array('repository' => $repository_id, 'revision' => $revision));
            if (!$commit_id)
                exit_with_error('RevisionNotFound', array('repository' => $repository_id, 'revision' => $revision));

            $owner_revision = array_get($data, 'ownerRevision');
            $patch_file_id = array_get($data, 'patch');
            if ($patch_file_id) {
                if (!is_numeric($patch_file_id) || !$db->select_first_row('uploaded_files', 'file', array('id' => $patch_file_id)))
                    exit_with_error('InvalidPatchFile', array('patch' => $patch_file_id));
                array_push($repository_with_patch, $repository_id);
                $repositories_require_build[$repository_id] =  TRUE;
            }

            $repository = NULL;
            $owner_commit_id = NULL;
            if ($owner_revision) {
                $repository = $db->select_first_row('repositories', 'repository', array('id' => intval($repository_id)));
                if (!$repository)
                    exit_with_error('RepositoryNotFound', array('repository' => $repository_id));
                $owner_commit = $db->select_first_row('commits', 'commit', array('repository' => $repository['repository_owner'], 'revision' => $owner_revision));
                if (!$owner_commit)
                    exit_with_error('InvalidOwnerRevision', array('repository' => $repository['repository_owner'], 'revision' => $owner_revision));
                if (!$db->select_first_row('commit_ownerships', 'commit', array('owned' => $commit_id, 'owner' => $owner_commit['commit_id'])))
                    exit_with_error('InvalidCommitOwnership', array('commitOwner' => $owner_commit['commit_id'], 'commitOwned' => $commit_id));
                $repositories_require_build[$repository_id] =  TRUE;
                $owner_commit_id = $owner_commit['commit_id'];
            }

            array_push($commit_set, array('commit' => $commit_id, 'patch_file' => $patch_file_id, 'requires_build' => FALSE, 'commit_owner' => $owner_commit_id));

            array_ensure_item_has_array($commit_set_items_by_repository, $repository_id);
            $commit_set_items_by_repository[$repository_id][] = &$commit_set[count($commit_set) - 1];

            if ($owner_commit_id)
                continue;
            array_push($repository_list, $repository_id);
        }
        $repository_group_id = $finder->find_by_repositories($repository_list);
        if (!$repository_group_id)
            exit_with_error('NoMatchingRepositoryGroup', array('repositories' => $repository_list));

        foreach ($repository_with_patch as $repository_id) {
            if (!$finder->accepts_patch($repository_group_id, $repository_id))
                exit_with_error('PatchNotAccepted', array('repository' => $repository_id, 'repositoryGroup' => $repository_group_id));
        }

        array_push($commit_set_list, array('repository_group' => $repository_group_id, 'set' => $commit_set));
    }

    foreach (array_keys($repositories_require_build) as $repository_id) {
        foreach($commit_set_items_by_repository[$repository_id] as &$commit_set_item)
            $commit_set_item['requires_build'] = TRUE;
    }
    return $commit_set_list;
}

function test_parameter_sets_from_test_parameters($db, $platform_id, $test_id, $test_parameters_list)
{
    if (is_null($test_parameters_list))
        return NULL;

    if (count($test_parameters_list) < 2)
        exit_with_error('InvalidTestParametersList', array('test_parameters_list' => $test_parameters_list));

    $triggerable_configuration = $db->select_first_row('triggerable_configurations', 'trigconfig', array('test' => $test_id, 'platform' => $platform_id));
    if (!$triggerable_configuration)
        exit_with_error('TriggerableNotFoundForTestAndPlatform', array('test' => $test_id, 'platform' => $platform_id));

    $trigger_config_id = $triggerable_configuration['trigconfig_id'];

    $test_parameter_rows= $db->query_and_fetch_all('SELECT testparam_id, testparam_name, testparam_type,
        testparam_has_value, testparam_has_file
        FROM triggerable_configuration_test_parameters JOIN test_parameters
            ON triggerable_configuration_test_parameters.trigconfigtestparam_parameter = test_parameters.testparam_id
        WHERE trigconfigtestparam_config = $1', array($trigger_config_id));

    $parameter_by_id = array();

    foreach($test_parameter_rows as &$row) {
        $parameter_by_id[$row['testparam_id']] = array(
            'name' => $row['testparam_name'],
            'type' => $row['testparam_type'],
            'has_value' => Database::is_true($row['testparam_has_value']),
            'has_file' => Database::is_true($row['testparam_has_file']),
        );
    }

    $test_parameter_set_list = array();
    foreach ($test_parameters_list as &$test_parameters) {
        if (is_null($test_parameters) || !count($test_parameters)) {
            $test_parameter_set_list[] = NULL;
            continue;
        }

        $test_parameter_set = array('set' => array(), 'need_to_build' => FALSE);
        foreach($test_parameters as $parameter_id => $entry) {
            if (!array_key_exists($parameter_id, $parameter_by_id))
                exit_with_error('UnsupportedTestParameter', array('parameter' => $parameter_id, 'supportedParameters' => array_keys($parameter_by_id)));
            $parameter = $parameter_by_id[$parameter_id];
            $test_parameter = array('parameter' => $parameter_id);
            if (!is_array($entry))
                exit_with_error('InvalidTestParameterEntry', array('parameter' => $parameter, 'entry' => $entry));
            $has_value = $parameter['has_value'];
            if ($has_value) {
                if (!array_key_exists('value', $entry))
                    exit_with_error('MissingValueForParameter', array('parameter' => $parameter, 'entry' => $entry));
                $test_parameter['value'] = json_encode($entry['value']);
            } else if (array_get($entry, 'value')) {
                exit_with_error('ShouldNotSpecifyValueForParameter', array('parameter' => $parameter, 'entry' => $entry));
            }
            $has_file = $parameter['has_file'];
            if ($has_file) {
                $file_id = array_get($entry, 'file');
                if (!is_numeric($file_id) || !$db->select_first_row('uploaded_files', 'file', array('id' => $file_id)))
                    exit_with_error('InvalidUploadedFileForParameter', array('parameter' => $parameter, 'entry' => $entry));
                $test_parameter['file'] = $file_id;
            } else if (array_get($entry, 'file')) {
                exit_with_error('ShouldNotSpecifyFileForParameter', array('parameter' => $parameter, 'entry' => $entry));
            }
            if ($parameter['type'] == 'build')
                $test_parameter_set['need_to_build'] = TRUE;
            $test_parameter_set['set'][] = $test_parameter;
        }
        $test_parameter_set_list[] = $test_parameter_set;
    }
    return $test_parameter_set_list;
}
?>