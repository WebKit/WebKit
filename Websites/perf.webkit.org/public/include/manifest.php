<?php

class ManifestGenerator {
    private $db;
    private $manifest;

    // FIXME: Compute this value from config.json
    const MANIFEST_PATH = '../data/manifest.json';

    function __construct($db) {
        $this->db = $db;
    }

    function generate() {
        $start_time = microtime(true);

        $config_table = $this->db->fetch_table('test_configurations');
        $platform_table = $this->db->fetch_table('platforms');
        $repositories_table = $this->db->fetch_table('repositories');

        $repositories_with_commit = $this->db->query_and_fetch_all(
            'SELECT DISTINCT(commit_repository) FROM commits WHERE commit_reported IS TRUE');
        if (!$repositories_with_commit)
            $repositories_with_commit = array();

        foreach ($repositories_with_commit as &$row)
            $row = $row['commit_repository'];

        $this->manifest = array(
            'siteTitle' => config('siteTitle', 'Performance Dashboard'),
            'tests' => (object)$this->tests(),
            'metrics' => (object)$this->metrics(),
            'all' => (object)$this->platforms($config_table, $platform_table, false),
            'dashboard' => (object)$this->platforms($config_table, $platform_table, true),
            'repositories' => (object)$this->repositories($repositories_table, $repositories_with_commit),
            'builders' => (object)$this->builders(),
            'bugTrackers' => (object)$this->bug_trackers($repositories_table),
            'dashboards' => (object)config('dashboards'),
        );

        $this->manifest['elapsedTime'] = (microtime(true) - $start_time) * 1000;

        return TRUE;
    }

    function manifest() { return $this->manifest; }

    function store() {
        return generate_data_file('manifest.json', json_encode($this->manifest));
    }

    private function tests() {
        $tests = array();
        $tests_table = $this->db->fetch_table('tests');
        if (!$tests_table)
            return $tests;
        foreach ($tests_table as $test_row) {
            $tests[$test_row['test_id']] = array(
                'name' => $test_row['test_name'],
                'url' => $test_row['test_url'],
                'parentId' => $test_row['test_parent'],
            );
        }
        return $tests;
    }

    private function metrics() {
        $metrics = array();
        $metrics_table = $this->db->query_and_fetch_all('SELECT * FROM test_metrics LEFT JOIN aggregators ON metric_aggregator = aggregator_id');
        if (!$metrics_table)
            return $metrics;
        foreach ($metrics_table as $row) {
            $metrics[$row['metric_id']] = array(
                'name' => $row['metric_name'],
                'test' => $row['metric_test'],
                'aggregator' => $row['aggregator_name']);
        }
        return $metrics;
    }

    private function platforms($config_table, $platform_table, $is_dashboard) {
        $platform_metrics = array();
        if ($config_table) {
            foreach ($config_table as $config_row) {
                if ($is_dashboard && !Database::is_true($config_row['config_is_in_dashboard']))
                    continue;

                $new_last_modified = array_get($config_row, 'config_runs_last_modified', 0);
                if ($new_last_modified)
                    $new_last_modified = strtotime($config_row['config_runs_last_modified']) * 1000;

                $platform = &array_ensure_item_has_array($platform_metrics, $config_row['config_platform']);
                $metrics = &array_ensure_item_has_array($platform, 'metrics');
                $last_modified = &array_ensure_item_has_array($platform, 'last_modified');

                $metric_id = $config_row['config_metric'];
                $index = array_search($metric_id, $metrics);
                if ($index === FALSE) {
                    array_push($metrics, $metric_id);
                    array_push($last_modified, $new_last_modified);
                } else
                    $last_modified[$index] = max($last_modified[$index], $new_last_modified);
            }
        }
        $configurations = array();
        
        $platforms = array();
        if ($platform_table) {
            foreach ($platform_table as $platform_row) {
                if (Database::is_true($platform_row['platform_hidden']))
                    continue;
                $id = $platform_row['platform_id'];
                if (array_key_exists($id, $platform_metrics)) {
                    $platforms[$id] = array(
                        'name' => $platform_row['platform_name'],
                        'metrics' => $platform_metrics[$id]['metrics'],
                        'lastModified' => $platform_metrics[$id]['last_modified']);
                }
            }
        }
        return $platforms;
    }

    private function repositories($repositories_table, $repositories_with_commit) {
        $repositories = array();
        if (!$repositories_table)
            return $repositories;
        foreach ($repositories_table as $row) {
            $repositories[$row['repository_id']] = array(
                'name' => $row['repository_name'],
                'url' => $row['repository_url'],
                'blameUrl' => $row['repository_blame_url'],
                'hasReportedCommits' => in_array($row['repository_id'], $repositories_with_commit));
        }

        return $repositories;
    }

    private function builders() {
        $builders_table = $this->db->fetch_table('builders');
        if (!$builders_table)
            return array();
        $builders = array();
        foreach ($builders_table as $row)
            $builders[$row['builder_id']] = array('name' => $row['builder_name'], 'buildUrl' => $row['builder_build_url']);

        return $builders;
    }

    private function bug_trackers($repositories_table) {
        $tracker_id_to_repositories = array();
        $tracker_repositories_table = $this->db->fetch_table('tracker_repositories');
        if ($tracker_repositories_table) {
            foreach ($tracker_repositories_table as $row) {
                array_push(array_ensure_item_has_array($tracker_id_to_repositories, $row['tracrepo_tracker']),
                    $row['tracrepo_repository']);
            }
        }

        $bug_trackers = array();
        $bug_trackers_table = $this->db->fetch_table('bug_trackers');
        if ($bug_trackers_table) {
            foreach ($bug_trackers_table as $row) {
                $bug_trackers[$row['tracker_id']] = array(
                    'name' => $row['tracker_name'],
                    'bugUrl' => $row['tracker_bug_url'],
                    'newBugUrl' => $row['tracker_new_bug_url'],
                    'repositories' => array_get($tracker_id_to_repositories, $row['tracker_id']));
            }
        }

        return $bug_trackers;
    }
}

?>
