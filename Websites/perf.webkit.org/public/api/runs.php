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
        SELECT test_runs.*, builds.*, array_agg((commit_repository, commit_revision, commit_time)) AS revisions
            FROM builds LEFT OUTER JOIN build_commits ON commit_build = build_id
                LEFT OUTER JOIN commits ON build_commit = commit_id,
                (SELECT test_runs.*, array_agg((bug_tracker, bug_number)) AS bugs
                    FROM test_runs LEFT OUTER JOIN bugs ON bug_run = run_id WHERE run_config = $1 GROUP BY run_id) as test_runs
                WHERE run_build = build_id
                GROUP BY run_id, run_config, run_build, run_mean_cache, run_iteration_count_cache,
                    run_sum_cache, run_square_sum_cache, bugs, build_id', array($config['config_id']));

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

function parse_bugs_array($postgres_array) {
    // e.g. {"(1 /* Bugzilla */, 12345)","(2 /* Radar */, 67890)"}
    $outer_array = json_decode('[' . trim($postgres_array, '{}') . ']');
    $bugs = array();
    foreach ($outer_array as $item) {
        $raw_data = explode(',', trim($item, '()'));
        if (!$raw_data[0])
            continue;
        $bugs[trim($raw_data[0], '"')] = trim($raw_data[1], '"');
    }
    return $bugs;
}

function format_run($run) {
    return array(
        'id' => intval($run['run_id']),
        'mean' => floatval($run['run_mean_cache']),
        'iterationCount' => intval($run['run_iteration_count_cache']),
        'sum' => floatval($run['run_sum_cache']),
        'squareSum' => floatval($run['run_square_sum_cache']),
        'revisions' => parse_revisions_array($run['revisions']),
        'bugs' => parse_bugs_array($run['bugs']),
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
