<?php

require_once('test-path-resolver.php');

class BuildRequestsFetcher {
    function __construct($db) {
        $this->db = $db;
        $this->rows = null;
        $this->commit_sets = array();
        $this->commits_by_id = array();
        $this->commits = array();
        $this->commit_sets_by_id = array();
    }

    function fetch_for_task($task_id) {
        $this->rows = $this->db->query_and_fetch_all('SELECT *, testgroup_task as task_id
            FROM build_requests LEFT OUTER JOIN builds ON request_build = build_id, analysis_test_groups
            WHERE request_group = testgroup_id AND testgroup_task = $1
            ORDER BY request_group, request_order', array($task_id));
    }

    function fetch_for_group($task_id, $test_group_id) {
        $this->rows = $this->db->query_and_fetch_all('SELECT *
            FROM build_requests LEFT OUTER JOIN builds ON request_build = build_id
            WHERE request_group = $1 ORDER BY request_order', array($test_group_id));
        foreach ($this->rows as &$row)
            $row['task_id'] = $task_id;
    }

    function fetch_incomplete_requests_for_triggerable($triggerable_id) {
        $this->rows = $this->db->query_and_fetch_all('SELECT *, test_groups.testgroup_task as task_id FROM build_requests,
            (SELECT testgroup_id, testgroup_task, (case when testgroup_author is not null then 0 else 1 end) as author_order, testgroup_created_at
                FROM analysis_test_groups WHERE EXISTS
                    (SELECT 1 FROM build_requests WHERE testgroup_id = request_group AND request_status
                        IN (\'pending\', \'scheduled\', \'running\'))) AS test_groups
            WHERE request_triggerable = $1 AND request_group = test_groups.testgroup_id
            ORDER BY author_order, testgroup_created_at, request_order', array($triggerable_id));
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
            $commit_set_id = $row['request_commit_set'];

            $this->fetch_commits_for_set_if_needed($commit_set_id, $resolve_ids);

            array_push($requests, array(
                'id' => $row['request_id'],
                'task' => $row['task_id'],
                'triggerable' => $row['request_triggerable'],
                'test' => $resolve_ids ? $test_path_resolver->path_for_test($test_id) : $test_id,
                'platform' => $resolve_ids ? $id_to_platform_name[$platform_id] : $platform_id,
                'testGroup' => $row['request_group'],
                'order' => $row['request_order'],
                'commitSet' => $commit_set_id,
                'status' => $row['request_status'],
                'url' => $row['request_url'],
                'build' => $row['request_build'],
                'createdAt' => $row['request_created_at'] ? strtotime($row['request_created_at']) * 1000 : NULL,
            ));
        }
        return $requests;
    }

    function commit_sets() {
        return $this->commit_sets;
    }

    function commits() {
        return $this->commits;
    }

    private function fetch_commits_for_set_if_needed($commit_set_id, $resolve_ids) {
        if (array_key_exists($commit_set_id, $this->commit_sets_by_id))
            return;

        $commit_rows = $this->db->query_and_fetch_all('SELECT *
            FROM commit_set_relationships, commits LEFT OUTER JOIN repositories ON commit_repository = repository_id
            WHERE commitset_commit = commit_id AND commitset_set = $1', array($commit_set_id));

        $commit_ids = array();
        foreach ($commit_rows as $row) {
            $repository_id = $resolve_ids ? $row['repository_name'] : $row['repository_id'];
            $revision = $row['commit_revision'];
            $commit_time = $row['commit_time'];
            array_push($commit_ids, $row['commit_id']);

            $commit_id = $row['commit_id'];
            if (array_key_exists($commit_id, $this->commits_by_id))
                continue;

            array_push($this->commits, array(
                'id' => $commit_id,
                'repository' => $repository_id,
                'revision' => $revision,
                'time' => Database::to_js_time($commit_time)));

            $this->commits_by_id[$commit_id] = TRUE;
        }

        $this->commit_sets_by_id[$commit_set_id] = TRUE;

        array_push($this->commit_sets, array('id' => $commit_set_id, 'commits' => $commit_ids));
    }
}

?>