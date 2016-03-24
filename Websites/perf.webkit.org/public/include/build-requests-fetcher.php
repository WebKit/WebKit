<?php

require_once('test-path-resolver.php');

class BuildRequestsFetcher {
    function __construct($db) {
        $this->db = $db;
        $this->rows = null;
        $this->root_sets = array();
        $this->roots_by_id = array();
        $this->roots = array();
        $this->root_sets_by_id = array();
    }

    function fetch_for_task($task_id) {
        $this->rows = $this->db->query_and_fetch_all('SELECT *
            FROM build_requests LEFT OUTER JOIN builds ON request_build = build_id, analysis_test_groups
            WHERE request_group = testgroup_id AND testgroup_task = $1
            ORDER BY request_group, request_order', array($task_id));
    }

    function fetch_for_group($test_group_id) {
        $this->rows = $this->db->query_and_fetch_all('SELECT *
            FROM build_requests LEFT OUTER JOIN builds ON request_build = build_id
            WHERE request_group = $1 ORDER BY request_order', array($test_group_id));
    }

    function fetch_incomplete_requests_for_triggerable($triggerable_id) {
        $this->rows = $this->db->query_and_fetch_all('SELECT * FROM build_requests
            WHERE request_triggerable = $1 AND request_status != \'completed\'
            ORDER BY request_created_at, request_group, request_order', array($triggerable_id));
    }

    function fetch_request($request_id) {
        $this->rows = $this->db->select_rows('build_requests', 'request', array('id' => $request_id));
    }

    function has_results() { return is_array($this->rows); }
    function results() { return $this->results_internal(false); }
    function results_with_resolved_ids() { return $this->results_internal(true); }

    private function results_internal($resolve_ids) {
        if (!$this->rows)
            return array();

        $id_to_platform_name = array();
        if ($resolve_ids) {
            foreach ($this->db->select_rows('platforms', 'platform', array()) as $platform)
                $id_to_platform_name[$platform['platform_id']] = $platform['platform_name'];
        }
        $test_path_resolver = new TestPathResolver($this->db);

        $requests = array();
        foreach ($this->rows as $row) {
            $test_id = $row['request_test'];
            $platform_id = $row['request_platform'];
            $root_set_id = $row['request_root_set'];

            $this->fetch_roots_for_set_if_needed($root_set_id, $resolve_ids);

            array_push($requests, array(
                'id' => $row['request_id'],
                'triggerable' => $row['request_triggerable'],
                'test' => $resolve_ids ? $test_path_resolver->path_for_test($test_id) : $test_id,
                'platform' => $resolve_ids ? $id_to_platform_name[$platform_id] : $platform_id,
                'testGroup' => $row['request_group'],
                'order' => $row['request_order'],
                'rootSet' => $root_set_id,
                'status' => $row['request_status'],
                'url' => $row['request_url'],
                'build' => $row['request_build'],
                'createdAt' => $row['request_created_at'] ? strtotime($row['request_created_at']) * 1000 : NULL,
            ));
        }
        return $requests;
    }

    function root_sets() {
        return $this->root_sets;
    }

    function roots() {
        return $this->roots;
    }

    private function fetch_roots_for_set_if_needed($root_set_id, $resolve_ids) {
        if (array_key_exists($root_set_id, $this->root_sets_by_id))
            return;

        $root_rows = $this->db->query_and_fetch_all('SELECT *
            FROM roots, commits LEFT OUTER JOIN repositories ON commit_repository = repository_id
            WHERE root_commit = commit_id AND root_set = $1', array($root_set_id));

        $root_ids = array();
        foreach ($root_rows as $row) {
            $repository_id = $resolve_ids ? $row['repository_name'] : $row['repository_id'];
            $revision = $row['commit_revision'];
            $commit_time = $row['commit_time'];
            array_push($root_ids, $row['commit_id']);

            $root_id = $row['commit_id'];
            if (array_key_exists($root_id, $this->roots_by_id))
                continue;

            array_push($this->roots, array(
                'id' => $root_id,
                'repository' => $repository_id,
                'revision' => $revision,
                'time' => Database::to_js_time($commit_time)));

            $this->roots_by_id[$root_id] = TRUE;
        }

        $this->root_sets_by_id[$root_set_id] = TRUE;

        array_push($this->root_sets, array('id' => $root_set_id, 'roots' => $root_ids));
    }
}

?>