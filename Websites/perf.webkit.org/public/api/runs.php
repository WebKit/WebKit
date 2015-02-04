<?php

require('../include/json-header.php');

function fetch_runs_for_config($db, $config) {
    $raw_runs = $db->query_and_fetch_all('
        SELECT test_runs.*, builds.*, array_agg((commit_repository, commit_revision, commit_time)) AS revisions
            FROM builds
                LEFT OUTER JOIN build_commits ON commit_build = build_id
                LEFT OUTER JOIN commits ON build_commit = commit_id, test_runs
            WHERE run_build = build_id AND run_config = $1 AND NOT EXISTS (SELECT * FROM build_requests WHERE request_build = build_id)
            GROUP BY build_id, run_id', array($config['config_id']));

    $formatted_runs = array();
    if (!$raw_runs)
        return $formatted_runs;

    foreach ($raw_runs as $run)
        array_push($formatted_runs, format_run($run));

    return $formatted_runs;
}

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
        'id' => intval($run['run_id']),
        'mean' => floatval($run['run_mean_cache']),
        'iterationCount' => intval($run['run_iteration_count_cache']),
        'sum' => floatval($run['run_sum_cache']),
        'squareSum' => floatval($run['run_square_sum_cache']),
        'revisions' => parse_revisions_array($run['revisions']),
        'buildTime' => strtotime($run['build_time']) * 1000,
        'buildNumber' => intval($run['build_number']),
        'builder' => $run['build_builder']);
}

function main($path) {
    if (count($path) != 1)
        exit_with_error('InvalidRequest');

    $parts = explode('-', $path[0]);
    if (count($parts) != 2)
        exit_with_error('InvalidRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    // FIXME: We should support revalication as well as caching results in the server side.
    $maxage = config('jsonCacheMaxAge');
    header('Expires: ' . gmdate('D, d M Y H:i:s', time() + $maxage) . ' GMT');
    header("Cache-Control: maxage=$maxage");

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

    $results = array();
    foreach ($config_rows as $config) {
        if ($runs = fetch_runs_for_config($db, $config))
            $results[$config['config_type']] = $runs;
    }

    exit_with_success($results);
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
