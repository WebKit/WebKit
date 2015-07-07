<?php

require('../include/json-header.php');

$paths = array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array();

if (count($paths) != 1)
    exit_with_error('InvalidRequest');

$parts = explode('-', $paths[0]);
if (count($parts) != 2)
    exit_with_error('InvalidRequest');

$db = new Database;
if (!$db->connect())
    exit_with_error('DatabaseConnectionFailure');

$platform_id = intval($parts[0]);
$metric_id = intval($parts[1]);
$config_rows = $db->query_and_fetch_all('SELECT config_id, config_type, config_platform, config_metric
    FROM test_configurations WHERE config_metric = $1 AND config_platform = $2', array($metric_id, $platform_id));
if (!$config_rows)
    exit_with_error('ConfigurationNotFound');

$repository_id_to_name = array();
if ($repository_table = $db->fetch_table('repositories')) {
    foreach ($repository_table as $repository)
        $repository_id_to_name[$repository['repository_id']] = $repository['repository_name'];
}

function fetch_runs_for_config($db, $config) {
    $raw_runs = $db->query_and_fetch_all('
    SELECT test_runs.*, builds.*, array_agg((revision_repository, revision_value, revision_time)) AS revisions
        FROM builds LEFT OUTER JOIN build_revisions ON revision_build = build_id, test_runs
        WHERE run_build = build_id AND run_config = $1
        GROUP BY build_id, build_builder, build_number, build_time, build_latest_revision,
            run_id, run_config, run_build, run_iteration_count_cache,
            run_mean_cache, run_sum_cache, run_square_sum_cache
        ORDER BY build_latest_revision, build_time', array($config['config_id']));

    $formatted_runs = array();
    if (!$raw_runs)
        return $formatted_runs;

    foreach ($raw_runs as $run)
        array_push($formatted_runs, format_run($run));

    return $formatted_runs;
}

date_default_timezone_set('UTC');
function parse_revisions_array($postgres_array) {
    global $repository_id_to_name;

    // e.g. {"(WebKit,131456,\"2012-10-16 14:53:00\")","(Chromium,162004,)"}
    $outer_array = json_decode('[' . trim($postgres_array, '{}') . ']');
    $revisions = array();
    foreach ($outer_array as $item) {
        $name_and_revision = explode(',', trim($item, '()'));
        if (!$name_and_revision[0])
            continue;
        $time = strtotime(trim($name_and_revision[2], '"')) * 1000;
        $revisions[$repository_id_to_name[trim($name_and_revision[0], '"')]] = array(trim($name_and_revision[1], '"'), $time);
    }
    return $revisions;
}

function format_run($run) {
    return array(
        'mean' => floatval($run['run_mean_cache']),
        'iterationCount' => intval($run['run_iteration_count_cache']),
        'sum' => floatval($run['run_sum_cache']),
        'squareSum' => floatval($run['run_square_sum_cache']),
        'revisions' => parse_revisions_array($run['revisions']),
        'buildTime' => strtotime($run['build_time']) * 1000,
        'buildNumber' => intval($run['build_number']),
        'builder' => $run['build_builder']);
}

$results = array();
foreach ($config_rows as $config) {
    if ($runs = fetch_runs_for_config($db, $config))
        $results[$config['config_type']] = $runs;
}

exit_with_success($results);

?>
