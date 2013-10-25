<?php

require_once('../include/json-shared.php');
require_once('../include/test-results.php');

$db = connect();

require_existence_of($_POST, array(
    'master' => '/[A-Za-z0-9\.]+/',
    'builder_name' => '/^[A-Za-z0-9 \(\)\-_]+$/',
    'build_number' => '/^[0-9]+?$/',
    'build_slave' => '/^[A-Za-z0-9\-_]+$/',
    'revisions' => '/^.+?$/',
    'start_time' => '/^[0-9]+(\.[0-9]+)?$/',
    'end_time' => '/^[0-9]+(\.[0-9]+)?$/'));
$master = $_POST['master'];
$builder_name = $_POST['builder_name'];
$build_number = intval($_POST['build_number']);

if (!array_key_exists('file', $_FILES) or !array_key_exists('tmp_name', $_FILES['file']) or count($_FILES['file']['tmp_name']) <= 0)
    exit_with_error('ResultsJSONNotIncluded');

$revisions = json_decode(str_replace('\\', '', $_POST['revisions']), TRUE);
foreach ($revisions as $repository_name => $revision_data) {
    require_format('repository_name', $repository_name, '/^\w+$/');
    require_existence_of($revision_data, array(
        'revision' => '/^[a-z0-9]+$/',
        'timestamp' => '/^[a-z0-9\-\.:TZ]+$/',
    ), 'revision');
}

$test_results = fetch_and_parse_test_results_json($_FILES['file']['tmp_name']);
if (!$test_results)
    exit_with_error('InvalidResultsJSON');

$start_time = float_to_time($_POST['start_time']);
$end_time = float_to_time($_POST['end_time']);

$builder_id = add_builder($db, $master, $builder_name);
if (!$builder_id)
    exit_with_error('FailedToInsertBuilder', array('master' => $master, 'builderName' => $builder_name));

$build_id = add_build($db, $builder_id, $build_number);
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

$slave_id = add_slave($db, $_POST['build_slave']);
if (!store_test_results($db, $test_results, $build_id, $start_time, $end_time, $slave_id))
    exit_with_error('FailedToStoreResults', array('buildId' => $build_id));

echo_success();

@ob_end_flush();
flush();
if (function_exists('fastcgi_finish_request'))
    fastcgi_finish_request();

$generator = new ResultsJSONGenerator($db, $builder_id);
$generator->generate();

?>
