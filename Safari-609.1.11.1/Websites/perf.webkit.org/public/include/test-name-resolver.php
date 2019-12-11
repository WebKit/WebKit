<?php

class TestNameResolver {

    function __construct($db) {
        $this->db = $db;
        $this->full_name_to_test = array();
        $this->test_id_to_child_metrics = array();
        $this->test_to_metrics = array();
        $this->tests_sorted_by_full_name = array();
        $this->id_to_metric = array();
        $this->id_to_aggregator = array();

        $test_table = $db->fetch_table('tests');
        if (!$test_table)
            return;

        $test_id_to_name = array();
        $test_id_to_parent = array();
        foreach ($test_table as $test_row) {
            $test_id = $test_row['test_id'];
            $test_id_to_name[$test_id] = $test_row['test_name'];
            $test_id_to_parent[$test_id] = $test_row['test_parent'];
        }

        $this->full_name_to_test = $this->compute_full_name($test_table, $test_id_to_name, $test_id_to_parent);
        $this->test_to_metrics = $this->map_metrics_to_tests($db->fetch_table('test_metrics'), $test_id_to_parent);
        $this->tests_sorted_by_full_name = $this->sort_tests_by_full_name($this->full_name_to_test);

        if ($aggregator_table = $db->fetch_table('aggregators')) {
            foreach ($aggregator_table as $aggregator)
                $this->id_to_aggregator[$aggregator['aggregator_id']] = $aggregator;
        }
    }

    private function compute_full_name($test_table, $test_id_to_name, $test_id_to_parent) {
        $full_name_to_test = array();
        foreach ($test_table as $test_row) {
            $test_path = array();
            $test_id = $test_row['test_id'];
            do {
                array_push($test_path, $test_id_to_name[$test_id]);
                $test_id = $test_id_to_parent[$test_id];
            } while ($test_id);
            $test_row['full_name'] = join('/', array_reverse($test_path));
            $full_name_to_test[$test_row['full_name']] = $test_row;
        }
        return $full_name_to_test;
    }

    private function map_metrics_to_tests($metrics_table, $test_id_to_parent) {
        $test_to_metrics = array();
        if (!$metrics_table)
            return $test_to_metrics;

        foreach ($metrics_table as $metric_row) {
            $this->id_to_metric[$metric_row['metric_id']] = $metric_row;

            $test_id = $metric_row['metric_test'];
            array_set_default($test_to_metrics, $test_id, array());
            array_push($test_to_metrics[$test_id], $metric_row);

            $parent_id = $test_id_to_parent[$test_id];
            if ($parent_id) {
                array_set_default($this->test_id_to_child_metrics, $parent_id, array());
                $parent_metrics = &$this->test_id_to_child_metrics[$parent_id];
                if (!in_array($metric_row['metric_name'], $parent_metrics))
                    array_push($parent_metrics, $metric_row);
            }
        }
        return $test_to_metrics;
    }

    private function sort_tests_by_full_name($full_name_to_test) {
        $tests_sorted_by_full_name = array();
        $full_names = array_keys($full_name_to_test);
        asort($full_names);
        foreach ($full_names as $name)
            array_push($tests_sorted_by_full_name, $full_name_to_test[$name]);
        return $tests_sorted_by_full_name;
    }

    function tests() {
        return $this->tests_sorted_by_full_name;
    }

    function test_id_for_full_name($full_name) {
        return array_get($this->full_name_to_test, $full_name);
    }

    function full_name_for_test($test_id) {
        foreach ($this->full_name_to_test as $full_name => $test) {
            if ($test['test_id'] == $test_id)
                return $full_name;
        }
        return NULL;
    }

    function full_name_for_metric($metric_id) {
        $metric_row = array_get($this->id_to_metric, $metric_id);
        if (!$metric_row)
            return NULL;
        $full_name = $this->full_name_for_test($metric_row['metric_test']) . ':' . $metric_row['metric_name'];
        if ($aggregator_id = $metric_row['metric_aggregator'])
            $full_name .= ':' . $this->id_to_aggregator[$aggregator_id]['aggregator_name'];
        return $full_name;
    }

    function metrics_for_test_id($test_id) {
        return array_get($this->test_to_metrics, $test_id, array());
    }

    function child_metrics_for_test_id($test_id) {
        return array_get($this->test_id_to_child_metrics, $test_id, array());
    }

    function configurations_for_metric_and_platform($metric_id, $platform_id) {
        return $this->db->query_and_fetch_all('SELECT * FROM test_configurations WHERE config_metric = $1 AND config_platform = $2
            ORDER BY config_id', array($metric_id, $platform_id));
    }
}

?>
