<?php

require('../include/json-header.php');

function fetch_runs_for_config_and_test_group($db, $config, $test_group_id) {
    return $db->query_and_fetch_all('
        SELECT test_runs.*, builds.*, array_agg((commit_repository, commit_revision, commit_time)) AS revisions
            FROM builds
                LEFT OUTER JOIN build_commits ON commit_build = build_id
                LEFT OUTER JOIN commits ON build_commit = commit_id,
                test_runs, build_requests, analysis_test_groups
            WHERE run_build = build_id AND run_config = $1 AND request_build = build_id AND request_group = $2
            GROUP BY build_id, run_id', array($config['config_id'], $test_group_id));
}

function fetch_runs_for_config($db, $config) {
    return $db->query_and_fetch_all('
        SELECT test_runs.*, builds.*, array_agg((commit_repository, commit_revision, commit_time)) AS revisions
            FROM builds
                LEFT OUTER JOIN build_commits ON commit_build = build_id
                LEFT OUTER JOIN commits ON build_commit = commit_id, test_runs
            WHERE run_build = build_id AND run_config = $1 AND NOT EXISTS (SELECT * FROM build_requests WHERE request_build = build_id)
            GROUP BY build_id, run_id', array($config['config_id']));
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

    $platform_id = intval($parts[0]);
    $metric_id = intval($parts[1]);
    $config_rows = $db->query_and_fetch_all('SELECT config_id, config_type, config_platform, config_metric
        FROM test_configurations WHERE config_metric = $1 AND config_platform = $2', array($metric_id, $platform_id));
    if (!$config_rows)
        exit_with_error('ConfigurationNotFound');

    $test_group_id = array_get($_GET, 'testGroup');
    if ($test_group_id)
        $test_group_id = intval($test_group_id);
    else {
        // FIXME: We should support revalication as well as caching results in the server side.
        $maxage = config('jsonCacheMaxAge');
        header('Expires: ' . gmdate('D, d M Y H:i:s', time() + $maxage) . ' GMT');
        header("Cache-Control: maxage=$maxage");
    }

    $generator = new RunsGenerator();

    foreach ($config_rows as $config) {
        if ($test_group_id)
            $raw_runs = fetch_runs_for_config_and_test_group($db, $config, $test_group_id);
        else
            $raw_runs = fetch_runs_for_config($db, $config);
        $generator->add_runs($config['config_type'], $raw_runs);
    }

    exit_with_success($generator->results());
}

class RunsGenerator {
    function __construct() {
        $this->results = array();
    }

    function &results() { return $this->results; }

    function add_runs($name, $raw_runs) {
        $formatted_runs = array();
        if ($raw_runs) {
            foreach ($raw_runs as $run)
                array_push($formatted_runs, self::format_run($run));
        }
        $this->results[$name] = $formatted_runs;
        return $formatted_runs;
    }

    private static function format_run($run) {
        return array(
            'id' => intval($run['run_id']),
            'mean' => floatval($run['run_mean_cache']),
            'iterationCount' => intval($run['run_iteration_count_cache']),
            'sum' => floatval($run['run_sum_cache']),
            'squareSum' => floatval($run['run_square_sum_cache']),
            'revisions' => self::parse_revisions_array($run['revisions']),
            'build' => $run['build_id'],
            'buildTime' => strtotime($run['build_time']) * 1000,
            'buildNumber' => intval($run['build_number']),
            'builder' => $run['build_builder']);
    }

    private static function parse_revisions_array($postgres_array) {
        // e.g. {"(WebKit,131456,\"2012-10-16 14:53:00\")","(Chromium,162004,)"}
        $outer_array = json_decode('[' . trim($postgres_array, '{}') . ']');
        $revisions = array();
        foreach ($outer_array as $item) {
            $name_and_revision = explode(',', trim($item, '()'));
            if (!$name_and_revision[0])
                continue;
            $time = strtotime(trim($name_and_revision[2], '"')) * 1000;
            $revisions[trim($name_and_revision[0], '"')] = array(trim($name_and_revision[1], '"'), $time);
        }
        return $revisions;
    }
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
