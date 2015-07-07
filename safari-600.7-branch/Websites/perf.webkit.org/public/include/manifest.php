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
        $config_table = $this->db->fetch_table('test_configurations');
        $platform_table = $this->db->fetch_table('platforms');
        $repositories_table = $this->db->fetch_table('repositories');
        $this->manifest = array(
            'tests' => $this->tests(),
            'metrics' => $this->metrics(),
            'all' => $this->platforms($config_table, $platform_table, false),
            'dashboard' => $this->platforms($config_table, $platform_table, true),
            'repositories' => $this->repositories($repositories_table),
            'builders' => $this->builders(),
            'bugTrackers' => $this->bug_trackers($repositories_table),
        );
        return $this->manifest;
    }

    function store() {
        return file_put_contents(self::MANIFEST_PATH, json_encode($this->manifest));
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
                if ($is_dashboard && !$this->db->is_true($config_row['config_is_in_dashboard']))
                    continue;

                $platform = &array_ensure_item_has_array($platform_metrics, $config_row['config_platform']);
                if (!in_array($config_row['config_metric'], $platform))
                    array_push($platform, $config_row['config_metric']);
            }
        }
        $platforms = array();
        if ($platform_table) {
            foreach ($platform_table as $platform_row) {
                if ($this->db->is_true($platform_row['platform_hidden']))
                    continue;
                $id = $platform_row['platform_id'];
                if (array_key_exists($id, $platform_metrics))
                    $platforms[$id] = array('name' => $platform_row['platform_name'], 'metrics' => $platform_metrics[$id]);
            }
        }
        return $platforms;
    }

    private function repositories($repositories_table) {
        $repositories = array();
        if (!$repositories_table)
            return $repositories;
        foreach ($repositories_table as $row)
            $repositories[$row['repository_name']] = array('url' => $row['repository_url'], 'blameUrl' => $row['repository_blame_url']);

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
        $repository_id_to_name = array();
        if ($repositories_table) {
            foreach ($repositories_table as $row)
                $repository_id_to_name[$row['repository_id']] = $row['repository_name'];
        }

        $tracker_id_to_repositories = array();
        $tracker_repositories_table = $this->db->fetch_table('tracker_repositories');
        if ($tracker_repositories_table) {
            foreach ($tracker_repositories_table as $row) {
                array_push(array_ensure_item_has_array($tracker_id_to_repositories, $row['tracrepo_tracker']),
                    $repository_id_to_name[$row['tracrepo_repository']]);
            }
        }

        $bug_trackers = array();
        $bug_trackers_table = $this->db->fetch_table('bug_trackers');
        if ($bug_trackers_table) {
            foreach ($bug_trackers_table as $row) {
                $bug_trackers[$row['tracker_name']] = array('newBugUrl' => $row['tracker_new_bug_url'],
                    'repositories' => $tracker_id_to_repositories[$row['tracker_id']]);
            }
        }

        return $bug_trackers;
    }
}

?>
