<?php

require('../include/json-header.php');

function main() {
    $program_start_time = microtime(true);

    $arguments = validate_arguments($_GET, array(
        'platform' => 'int?',
        'metric' => 'int?',
        'analysisTask' => 'int?'));

    $platform_id = $arguments['platform'];
    $metric_id = $arguments['metric'];
    $task_id = $arguments['analysisTask'];
    if (!(($platform_id && $metric_id && !$task_id) || ($task_id && !$platform_id && !$metric_id)))
        exit_with_error('AmbiguousRequest');

    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    if ($task_id) {
        $fetcher = new AnalysisResultsFetcher($db, $task_id);
        exit_with_success($fetcher->fetch());
    }

    $fetcher = new MeasurementSetFetcher($db);
    if (!$fetcher->fetch_config_list($platform_id, $metric_id)) {
        exit_with_error('ConfigurationNotFound',
            array('platform' => $platform_id, 'metric' => $metric_id));
    }

    if ($fetcher->at_end()) {
        header($_SERVER['SERVER_PROTOCOL'] . ' 404 Not Found');
        exit(404);
    }

    $cluster_count = 0;
    $elapsed_time = NULL;
    while (!$fetcher->at_end()) {
        $content = $fetcher->fetch_next_cluster();
        $cluster_count++;
        if ($fetcher->at_end()) {
            $cache_filename = "measurement-set-$platform_id-$metric_id.json";
            $content['clusterCount'] = $cluster_count;
            $elapsed_time = (microtime(true) - $program_start_time) * 1000;
        } else
            $cache_filename = "measurement-set-$platform_id-$metric_id-{$content['endTime']}.json";

        set_successful($content);
        $json = generate_json_data_with_elapsed_time_if_needed($cache_filename, $content, $elapsed_time);
    }

    echo $json;
}

define('DAY', 24 * 3600 * 1000);
define('YEAR', 365.24 * DAY);
define('MONTH', 30 * DAY);

class MeasurementSetFetcher {
    function __construct($db) {
        $this->db = $db;
        $this->queries = NULL;

        // Each cluster contains data points between two commit time
        // as well as a point immediately before and a point immediately after these points.
        // Clusters are fetched in chronological order.
        $start_time = config('clusterStart');
        $size = config('clusterSize');
        $this->cluster_start = mktime($start_time[3], $start_time[4], 0, $start_time[1], $start_time[2], $start_time[0]) * 1000;
        $this->next_cluster_start = $this->cluster_start;
        $this->next_cluster_results = NULL;
        $this->cluster_size = $size[0] * YEAR + $size[1] * MONTH + $size[2] * DAY;
        $this->last_modified = 0;

        $this->start_time = microtime(TRUE);
    }

    function fetch_config_list($platform_id, $metric_id) {
        $config_rows = $this->db->query_and_fetch_all('SELECT *
            FROM test_configurations WHERE config_metric = $1 AND config_platform = $2',
            array($metric_id, $platform_id));
        $this->config_rows = $config_rows;
        if (!$config_rows)
            return FALSE;

        $this->queries = array();
        $this->next_cluster_results = array();
        $min_commit_time = microtime(TRUE) * 1000;
        foreach ($config_rows as &$config_row) {
            $query = $this->execute_query($config_row['config_id']);

            $this->last_modified = max($this->last_modified, Database::to_js_time($config_row['config_runs_last_modified']));

            $measurement_row = $this->db->fetch_next_row($query);
            if ($measurement_row) {
                $commit_time = 0;
                $formatted_row = self::format_run($measurement_row, $commit_time);
                $this->next_cluster_results[$config_row['config_type']] = array($formatted_row);
                $min_commit_time = min($min_commit_time, $commit_time);
            } else
                $query = NULL;

            $this->queries[$config_row['config_type']] = $query;
        }

        while ($this->next_cluster_start + $this->cluster_size < $min_commit_time)
            $this->next_cluster_start += $this->cluster_size;

        return TRUE;
    }

    function at_end() {
        if ($this->queries === NULL)
            return FALSE;
        foreach ($this->queries as $name => &$query) {
            if ($query)
                return FALSE;
        }
        return TRUE;
    }
    
    function fetch_next_cluster() {
        assert($this->queries);

        $results_by_config = array();
        $current_cluster_start = $this->next_cluster_start;
        $this->next_cluster_start += $this->cluster_size;

        foreach ($this->queries as $name => &$query) {
            assert($this->next_cluster_start);

            $carry_over = array_get($this->next_cluster_results, $name);
            if ($carry_over)
                $results_by_config[$name] = $carry_over;
            else
                $results_by_config[$name] = array();

            if (!$query)
                continue;

            while ($row = $this->db->fetch_next_row($query)) {
                $commit_time = NULL;
                $formatted_row = self::format_run($row, $commit_time);
                array_push($results_by_config[$name], $formatted_row);
                $row_belongs_to_next_cluster = $commit_time > $this->next_cluster_start;
                if ($row_belongs_to_next_cluster)
                    break;
            }

            $reached_end = !$row;
            if ($reached_end)
                $this->queries[$name] = NULL;
            $this->next_cluster_results[$name] = array_slice($results_by_config[$name], -2);
        }

        return array(
            'clusterStart' => $this->cluster_start,
            'clusterSize' => $this->cluster_size,
            'configurations' => &$results_by_config,
            'formatMap' => self::format_map(),
            'startTime' => $current_cluster_start,
            'endTime' => $this->next_cluster_start,
            'lastModified' => $this->last_modified);
    }

    function execute_query($config_id) {
        return $this->db->query('
            SELECT test_runs.*, build_id, build_number, build_builder, build_time,
            array_agg((commit_id, commit_repository, commit_revision, commit_order, extract(epoch from commit_time at time zone \'utc\') * 1000)) AS revisions,
            extract(epoch from max(commit_time at time zone \'utc\')) * 1000 AS revision_time
                FROM builds
                    LEFT OUTER JOIN build_commits ON commit_build = build_id
                    LEFT OUTER JOIN commits ON build_commit = commit_id, test_runs
                WHERE run_build = build_id AND run_config = $1 AND NOT EXISTS (SELECT * FROM build_requests WHERE request_build = build_id)
                GROUP BY build_id, build_builder, build_number, build_time, build_latest_revision, build_slave,
                    run_id, run_config, run_build, run_iteration_count_cache, run_mean_cache, run_sum_cache, run_square_sum_cache, run_marked_outlier
                ORDER BY revision_time, build_time', array($config_id));
    }

    static function format_map()
    {
        return array('id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions',
            'commitTime', 'build', 'buildTime', 'buildNumber', 'builder');
    }

    private static function format_run(&$run, &$commit_time) {
        $commit_time = intval($run['revision_time']);
        $build_time = Database::to_js_time($run['build_time']);
        if (!$commit_time)
            $commit_time = $build_time;
        return array(
            intval($run['run_id']),
            floatval($run['run_mean_cache']),
            intval($run['run_iteration_count_cache']),
            floatval($run['run_sum_cache']),
            floatval($run['run_square_sum_cache']),
            Database::is_true($run['run_marked_outlier']),
            self::parse_revisions_array($run['revisions']),
            $commit_time,
            intval($run['build_id']),
            $build_time,
            $run['build_number'],
            intval($run['build_builder']));
    }

    private static function parse_revisions_array(&$postgres_array) {
        // e.g. {"(<commit-id>,<repository-id>,<revision>,<order>,\"2012-10-16 14:53:00\")","(<commit-id>,<repository-id>,<revision>,<order>,)",
        // "(<commit-id>,<repository-id>,<revision>,,)", "(<commit-id>,<repository-id>,<revision>,,\"2012-10-16 14:53:00\")"}
        $outer_array = json_decode('[' . trim($postgres_array, '{}') . ']');
        $revisions = array();
        foreach ($outer_array as $item) {
            $name_and_revision = explode(',', trim($item, '()'));
            if (!$name_and_revision[0])
                continue;
            $commit_id = intval(trim($name_and_revision[0], '"'));
            $repository_id = intval(trim($name_and_revision[1], '"'));
            $revision = trim($name_and_revision[2], '"');
            $trimmed_order = trim($name_and_revision[3], '"');
            $order = strlen($trimmed_order) ? intval($trimmed_order) : NULL;
            $time = intval(trim($name_and_revision[4], '"'));
            array_push($revisions, array($commit_id, $repository_id, $revision, $order, $time));
        }
        return $revisions;
    }
}

class AnalysisResultsFetcher {

    function __construct($db, $task_id) {
        $this->db = $db;
        $this->task_id = $task_id;
        $this->build_to_commits = array();
    }

    function fetch()
    {
        $start_time = microtime(TRUE);

        // Fetch commmits separately from test_runs since number of builds is much smaller than number of runs here.
        $this->fetch_commits();

        $query = $this->db->query('SELECT test_runs.*, builds.*, test_configurations.*
            FROM builds,
                test_runs JOIN test_configurations ON run_config = config_id,
                build_requests JOIN analysis_test_groups ON request_group = testgroup_id
            WHERE run_build = build_id AND build_id = request_build
                AND testgroup_task = $1 AND run_config = config_id', array($this->task_id));

        $results = array();
        while ($row = $this->db->fetch_next_row($query))
            array_push($results, $this->format_measurement($row));

        return array(
            'formatMap' => self::format_map(),
            'measurements' => $results,
            'elapsedTime' => (microtime(TRUE) - $start_time) * 1000);
    }

    function fetch_commits()
    {
        $query = $this->db->query('SELECT commit_id, commit_build, commit_repository, commit_revision, commit_order, commit_time
            FROM commits, build_commits, build_requests, analysis_test_groups
            WHERE commit_id = build_commit AND commit_build = request_build
                AND request_group = testgroup_id AND testgroup_task = $1', array($this->task_id));
        while ($row = $this->db->fetch_next_row($query)) {
            $commit_time = Database::to_js_time($row['commit_time']);
            array_push(array_ensure_item_has_array($this->build_to_commits, $row['commit_build']),
                array($row['commit_id'], $row['commit_repository'], $row['commit_revision'], $row['commit_order'], $commit_time));
        }
    }

    function format_measurement($row)
    {
        $build_id = $row['build_id'];
        return array(
            intval($row['run_id']),
            floatval($row['run_mean_cache']),
            intval($row['run_iteration_count_cache']),
            floatval($row['run_sum_cache']),
            floatval($row['run_square_sum_cache']),
            $this->build_to_commits[$build_id],
            intval($build_id),
            Database::to_js_time($row['build_time']),
            $row['build_number'],
            intval($row['build_builder']),
            intval($row['config_metric']),
            $row['config_type']);
    }

    static function format_map()
    {
        return array('id', 'mean', 'iterationCount', 'sum', 'squareSum', 'revisions',
            'build', 'buildTime', 'buildNumber', 'builder', 'metric', 'configType');
    }
}

main();

?>
