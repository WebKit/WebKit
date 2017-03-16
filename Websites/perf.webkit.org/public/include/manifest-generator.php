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

        $platform_table = $this->db->fetch_table('platforms');
        $repositories_table = $this->db->fetch_table('repositories');

        $repositories_with_commit = $this->db->query_and_fetch_all(
            'SELECT DISTINCT(commit_repository) FROM commits WHERE commit_reported IS TRUE');
        if (!$repositories_with_commit)
            $repositories_with_commit = array();

        foreach ($repositories_with_commit as &$row)
            $row = $row['commit_repository'];

        $tests = (object)$this->tests();
        $metrics = (object)$this->metrics();
        $platforms = (object)$this->platforms($platform_table, false);
        $dashboard = (object)$this->platforms($platform_table, true);
        $repositories = (object)$this->repositories($repositories_table, $repositories_with_commit);

        $this->manifest = array(
            'siteTitle' => config('siteTitle', 'Performance Dashboard'),
            'tests' => &$tests,
            'metrics' => &$metrics,
            'all' => &$platforms,
            'dashboard' => &$dashboard,
            'repositories' => &$repositories,
            'builders' => (object)$this->builders(),
            'bugTrackers' => (object)$this->bug_trackers($repositories_table),
            'triggerables'=> (object)$this->triggerables(),
            'dashboards' => (object)config('dashboards'),
            'summaryPages' => config('summaryPages'),
            'fileUploadSizeLimit' => config('uploadFileLimitInMB', 0) * 1024 * 1024,
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

    private function platforms($platform_table, $is_dashboard) {
        $metrics = $this->db->query_and_fetch_all('SELECT config_metric AS metric_id, config_platform AS platform_id,
            extract(epoch from max(config_runs_last_modified) at time zone \'utc\') * 1000 AS last_modified, bool_or(config_is_in_dashboard) AS in_dashboard
            FROM test_configurations GROUP BY config_metric, config_platform ORDER BY config_platform');

        $platform_metrics = array();

        if ($metrics) {
            $current_platform_entry = null;
            foreach ($metrics as $metric_row) {
                if ($is_dashboard && !Database::is_true($metric_row['in_dashboard']))
                    continue;

                $platform_id = $metric_row['platform_id'];
                if (!$current_platform_entry || $current_platform_entry['id'] != $platform_id) {
                    $current_platform_entry = &array_ensure_item_has_array($platform_metrics, $platform_id);
                    $current_platform_entry['id'] = $platform_id;
                    array_ensure_item_has_array($current_platform_entry, 'metrics');
                    array_ensure_item_has_array($current_platform_entry, 'last_modified');
                }

                array_push($current_platform_entry['metrics'], $metric_row['metric_id']);
                array_push($current_platform_entry['last_modified'], intval($metric_row['last_modified']));
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
                'owner'=> $row['repository_owner'],
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

    private function triggerables() {

        $triggerables = $this->db->fetch_table('build_triggerables');
        if (!$triggerables)
            return array();

        $id_to_triggerable = array();
        foreach ($triggerables as $row) {
            $id = $row['triggerable_id'];
            $id_to_triggerable[$id] = array(
                'name' => $row['triggerable_name'],
                'acceptedRepositories' => array(),
                'configurations' => array());
        }

        $repository_map = $this->db->fetch_table('triggerable_repositories');
        if ($repository_map) {
            foreach ($repository_map as $row) {
                $triggerable = &$id_to_triggerable[$row['trigrepo_triggerable']];
                array_push($triggerable['acceptedRepositories'], $row['trigrepo_repository']);
            }
        }

        $configuration_map = $this->db->fetch_table('triggerable_configurations');
        if ($configuration_map) {
            foreach ($configuration_map as $row) {
                $triggerable = &$id_to_triggerable[$row['trigconfig_triggerable']];
                array_push($triggerable['configurations'], array($row['trigconfig_test'], $row['trigconfig_platform']));
            }
        }

        return $id_to_triggerable;
    }
}

?>
