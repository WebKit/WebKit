<?php

require_once('../include/json-shared.php');
require_once('../include/test-results.php');

function store_results($db, $master, $builder_name, $build_number, $start_time, $end_time, $revisions, $json_path) {
    $test_results = fetch_and_parse_test_results_json($json_path);
    if (!$test_results)
        exit_with_error('InvalidResultsJSON');

    $builder_id = add_builder($db, $master, $builder_name);
    if (!$builder_id)
        exit_with_error('FailedToInsertBuilder', array('master' => $master, 'builderName' => $builder_name));

    $slave_id = add_slave($db, $_POST['build_slave']);
    $build_id = add_build($db, $builder_id, $build_number, $slave_id);
    if (!$build_id)
        exit_with_error('FailedToInsertBuild', array('builderId' => $builder_id, 'buildNumber' => $build_number));

    foreach ($revisions as $repository_name => $revision_data) {
        $repository_id = $db->select_or_insert_row('repositories', NULL, array('name' => $repository_name));
        if (!$repository_id)
            exit_with_error('FailedToInsertRepository', array('name' => $repository_name));

        $revision_data = array(
            'repository' => $repository_id,
            'build' => $build_id,
            'value' => $revision_data['revision'],
            'time' => array_get($revision_data, 'timestamp'));
        $db->select_or_insert_row('build_revisions', NULL, array('repository' => $repository_id, 'build' => $build_id), $revision_data, 'value')
            or exit_with_error('FailedToInsertRevision', array('name' => $repository_name, 'data' => $revision_data));
    }

    if (!store_test_results($db, $test_results, $build_id, $start_time, $end_time))
        exit_with_error('FailedToStoreResults', array('buildId' => $build_id));
}

function main() {
    require_existence_of($_POST, array(
        'master' => '/[A-Za-z0-9\.]+/',
        'builder_name' => '/^[A-Za-z0-9 \(\)\-_,]+$/',
        'build_number' => '/^[0-9]+?$/',
        'build_slave' => '/^[A-Za-z0-9\-_]+$/',
        'revisions' => '/^.+?$/',
        'start_time' => '/^[0-9]+(\.[0-9]+)?$/',
        'end_time' => '/^[0-9]+(\.[0-9]+)?$/'));
    $master = $_POST['master'];
    $builder_name = $_POST['builder_name'];
    $build_number = intval($_POST['build_number']);
    $start_time = float_to_time($_POST['start_time']);
    $end_time = float_to_time($_POST['end_time']);

    $revisions = json_decode(str_replace('\\', '', $_POST['revisions']), TRUE);
    foreach ($revisions as $repository_name => $revision_data) {
        require_format('repository_name', $repository_name, '/^\w+$/');
        require_existence_of($revision_data, array(
            'revision' => '/^[a-z0-9]+$/',
            'timestamp' => '/^[a-z0-9\-\.:TZ]+$/',
        ), 'revision');
    }

    if (!array_key_exists('file', $_FILES) or !array_key_exists('tmp_name', $_FILES['file']) or count($_FILES['file']['tmp_name']) <= 0)
        exit_with_error('ResultsJSONNotIncluded');
    $json_path = $_FILES['file']['tmp_name'];

    $db = connect();
    store_results($db, $master, $builder_name, $build_number, $start_time, $end_time, $revisions, $json_path);
    echo_success();
}

main();

?>
