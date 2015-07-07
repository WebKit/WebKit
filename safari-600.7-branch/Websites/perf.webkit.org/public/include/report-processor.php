<?php

require_once('../include/json-header.php');

class ReportProcessor {
    private $db;
    private $name_to_aggregator;
    private $report_id;
    private $runs;

    function __construct($db) {
        $this->db = $db;
        $this->name_to_aggregator = array();
        $aggregator_table = $db->fetch_table('aggregators');
        if ($aggregator_table) {
            foreach ($aggregator_table as $aggregator_row) {
                $this->name_to_aggregator[$aggregator_row['aggregator_name']] = $aggregator_row;
            }
        }
    }

    private function exit_with_error($message, $details = NULL) {
        if (!$this->report_id) {
            $details['failureStored'] = FALSE;
            exit_with_error($message, $details);
        }

        $details['failureStored'] = $this->db->query_and_get_affected_rows(
            'UPDATE reports SET report_failure = $1, report_failure_details = $2 WHERE report_id = $3',
            array($message, $details ? json_encode($details) : NULL, $this->report_id)) == 1;
        exit_with_error($message, $details);
    }

    function process($report, $existing_report_id = NULL) {
        $this->report_id = $existing_report_id;
        $this->runs = NULL;

        array_key_exists('builderName', $report) or $this->exit_with_error('MissingBuilderName');
        array_key_exists('buildTime', $report) or $this->exit_with_error('MissingBuildTime');

        $builder_info = array('name' => $report['builderName']);
        if (!$existing_report_id)
            $builder_info['password_hash'] = hash('sha256', $report['builderPassword']);
        if (array_key_exists('builderPassword', $report))
            unset($report['builderPassword']);

        $matched_builder = $this->db->select_first_row('builders', 'builder', $builder_info);
        if (!$matched_builder)
            $this->exit_with_error('BuilderNotFound', array('name' => $builder_info['name']));

        $build_data = $this->construct_build_data($report, $matched_builder);
        if (!$existing_report_id)
            $this->store_report($report, $build_data);

        $this->runs = new TestRunsGenerator($this->db, $this->name_to_aggregator, $this->report_id);
        $this->recursively_ensure_tests($report['tests']);

        $this->runs->aggregate();
        $this->runs->compute_caches();

        $platform_id = $this->db->select_or_insert_row('platforms', 'platform', array('name' => $report['platform']));
        if (!$platform_id)
            $this->exit_with_error('FailedToInsertPlatform', array('name' => $report['platform']));

        $build_id = $this->resolve_build_id($build_data, array_get($report, 'revisions', array()));

        $this->runs->commit($platform_id, $build_id);
    }

    private function construct_build_data($report, $builder) {
        array_key_exists('buildNumber', $report) or $this->exit_with_error('MissingBuildNumber');
        array_key_exists('buildTime', $report) or $this->exit_with_error('MissingBuildTime');

        return array('builder' => $builder['builder_id'], 'number' => $report['buildNumber'], 'time' => $report['buildTime']);
    }

    private function store_report($report, $build_data) {
        assert(!$this->report_id);
        $this->report_id = $this->db->insert_row('reports', 'report', array('builder' => $build_data['builder'], 'build_number' => $build_data['number'],
            'content' => json_encode($report)));
        if (!$this->report_id)
            $this->exit_with_error('FailedToStoreRunReport');
    }

    private function resolve_build_id($build_data, $revisions) {
        // FIXME: This code has a race condition. See <rdar://problem/15876303>.
        $results = $this->db->query_and_fetch_all("SELECT build_id FROM builds WHERE build_builder = $1 AND build_number = $2 AND build_time <= $3 AND build_time + interval '1 day' > $3",
            array($build_data['builder'], $build_data['number'], $build_data['time']));
        if ($results)
            $build_id = $results[0]['build_id'];
        else
            $build_id = $this->db->insert_row('builds', 'build', $build_data);
        if (!$build_id)
            $this->exit_with_error('FailedToInsertBuild', $build_data);

        foreach ($revisions as $repository_name => $revision_data) {
            $repository_id = $this->db->select_or_insert_row('repositories', 'repository', array('name' => $repository_name));
            if (!$repository_id)
                $this->exit_with_error('FailedToInsertRepository', array('name' => $repository_name));

            $revision_data = array('repository' => $repository_id, 'build' => $build_id, 'value' => $revision_data['revision'],
                'time' => array_get($revision_data, 'timestamp'));
            $revision_row = $this->db->select_or_insert_row('build_revisions', 'revision', array('repository' => $repository_id, 'build' => $build_id), $revision_data, '*');
            if (!$revision_row)
                $this->exit_with_error('FailedToInsertRevision', $revision_data);
            if ($revision_row['revision_value'] != $revision_data['value'])
                $this->exit_with_error('MismatchingRevisionData', array('existing' => $revision_row, 'new' => $revision_data));
        }

        return $build_id;
    }

    private function recursively_ensure_tests($tests, $parent_id = NULL, $level = 0) {
        foreach ($tests as $test_name => $test) {
            $test_id = $this->db->select_or_insert_row('tests', 'test', $parent_id ? array('name' => $test_name, 'parent' => $parent_id) : array('name' => $test_name),
                array('name' => $test_name, 'parent' => $parent_id, 'url' => array_get($test, 'url')));
            if (!$test_id)
                $this->exit_with_error('FailedToAddTest', array('name' => $test_name, 'parent' => $parent_id));

            if (array_key_exists('tests', $test))
                $this->recursively_ensure_tests($test['tests'], $test_id, $level + 1);

            foreach (array_get($test, 'metrics', array()) as $metric_name => $aggregators_or_config_types) {
                $aggregators = $this->aggregator_list_if_exists($aggregators_or_config_types);
                if ($aggregators) {
                    foreach ($aggregators as $aggregator_name)
                        $this->runs->add_aggregated_metric($parent_id, $test_id, $test_name, $metric_name, $aggregator_name, $level);
                } else {
                    $metric_id = $this->db->select_or_insert_row('test_metrics', 'metric', array('name' => $metric_name, 'test' => $test_id));
                    if (!$metric_id)
                        $this->exit_with_error('FailedToAddMetric', array('name' => $metric_name, 'test' => $test_id));

                    foreach ($aggregators_or_config_types as $config_type => $values) {
                        // Some tests submit groups of iterations; e.g. [[1, 2, 3, 4], [5, 6, 7, 8]]
                        // Convert other tests to this format to simplify the computation later.
                        if (gettype($values) !== 'array')
                            $values = array($values);
                        if (gettype($values[0]) !== 'array')
                            $values = array($values);
                        $this->runs->add_values_to_commit($metric_id, $config_type, $values);
                        $this->runs->add_values_for_aggregation($parent_id, $test_name, $metric_name, $config_type, $values);
                    }
                }
            }
        }
    }

    private function aggregator_list_if_exists($aggregators_or_config_types) {
        if (array_key_exists(0, $aggregators_or_config_types))
            return $aggregators_or_config_types;
        else if (array_get($aggregators_or_config_types, 'aggregators'))
            return $aggregators_or_config_types['aggregators'];
        return NULL;
    }
};

class TestRunsGenerator {
    private $db;
    private $name_to_aggregator;
    private $report_id;
    private $metrics_to_aggregate;
    private $parent_to_values;
    private $values_to_commit;

    function __construct($db, $name_to_aggregator, $report_id) {
        $this->db = $db;
        $this->name_to_aggregator = $name_to_aggregator or array();
        $this->report_id = $report_id;
        $this->metrics_to_aggregate = array();
        $this->parent_to_values = array();
        $this->values_to_commit = array();
    }

    private function exit_with_error($message, $details = NULL) {
        $details['failureStored'] = $this->db->query_and_get_affected_rows(
            'UPDATE reports SET report_failure = $1, report_failure_details = $2 WHERE report_id = $3',
            array($message, $details ? json_encode($details) : NULL, $this->report_id)) == 1;
        exit_with_error($message, $details);
    }

    function add_aggregated_metric($parent_id, $test_id, $test_name, $metric_name, $aggregator_name, $level) {
        array_key_exists($aggregator_name, $this->name_to_aggregator) or $this->exit_with_error('AggregatorNotFound', array('name' => $aggregator_name));

        $metric_id = $this->db->select_or_insert_row('test_metrics', 'metric', array('name' => $metric_name,
            'test' => $test_id, 'aggregator' => $this->name_to_aggregator[$aggregator_name]['aggregator_id']));
        if (!$metric_id)
            $this->exit_with_error('FailedToAddAggregatedMetric', array('name' => $metric_name, 'test' => $test_id, 'aggregator' => $aggregator_name));

        array_push($this->metrics_to_aggregate, array(
            'test_id' => $test_id,
            'parent_id' => $parent_id,
            'metric_id' => $metric_id,
            'test_name' => $test_name,
            'metric_name' => $metric_name,
            'aggregator' => $aggregator_name,
            'aggregator_definition' => $this->name_to_aggregator[$aggregator_name]['aggregator_definition'],
            'level' => $level));
    }

    function add_values_for_aggregation($parent_id, $test_name, $metric_name, $config_type, $values, $aggregator = NULL) {
        $value_list = &array_ensure_item_has_array(array_ensure_item_has_array(array_ensure_item_has_array(array_ensure_item_has_array(
            $this->parent_to_values, strval($parent_id)), $metric_name), $config_type), $test_name);
        array_push($value_list, array('aggregator' => $aggregator, 'values' => $values));
    }

    function aggregate() {
        $expressions = array();
        foreach ($this->metrics_to_aggregate as $test_metric) {
            $configurations = array_get(array_get($this->parent_to_values, strval($test_metric['test_id']), array()), $test_metric['metric_name']);
            foreach ($configurations as $config_type => $test_value_list) {
                // FIXME: We should preserve the test order. For that, we need a new column in our database.
                $values_by_iteration = $this->test_value_list_to_values_by_iterations($test_value_list, $test_metric, $test_metric['aggregator']);
                $flattened_aggregated_values = array();
                for ($i = 0; $i < count($values_by_iteration['values']); ++$i)
                    array_push($flattened_aggregated_values, $this->aggregate_values($test_metric['aggregator'], $values_by_iteration['values'][$i]));

                $grouped_values = array();
                foreach ($values_by_iteration['group_sizes'] as $size) {
                    $new_group = array();
                    for ($i = 0; $i < $size; ++$i)
                        array_push($new_group, array_shift($flattened_aggregated_values));
                    array_push($grouped_values, $new_group);
                }

                $this->add_values_to_commit($test_metric['metric_id'], $config_type, $grouped_values);
                $this->add_values_for_aggregation($test_metric['parent_id'], $test_metric['test_name'], $test_metric['metric_name'],
                    $config_type, $grouped_values, $test_metric['aggregator']);
            }
        }
    }

    private function test_value_list_to_values_by_iterations($test_value_list, $test_metric, $aggregator) {
        $values_by_iterations = array();
        $group_sizes = array();
        $first_test = TRUE;
        foreach ($test_value_list as $test_name => $aggregators_and_values) {
            if (count($aggregators_and_values) == 1) // Either the subtest has exactly one aggregator or is raw value (not aggregated)
                $values = $aggregators_and_values[0]['values'];
            else {
                $values = NULL;
                // Find the values of the subtest aggregated by the same aggregator.
                foreach ($aggregators_and_values as $aggregator_and_values) {
                    if ($aggregator_and_values['aggregator'] == $aggregator) {
                        $values = $aggregator_and_values['values'];
                        break;                        
                    }
                }
                if (!$values) {
                    $this->exit_with_error('NoMatchingAggregatedValueInSubtest',
                        array('parent' => $test_metric['test_id'],
                        'metric' => $test_metric['metric_name'],
                        'childTest' => $test_name,
                        'aggregator' => $aggregator,
                        'aggregatorAndValues' => $aggregators_and_values));
                }
            }

            for ($group = 0; $group < count($values); ++$group) {
                if ($first_test) {
                    array_push($group_sizes, count($values[$group]));
                    for ($i = 0; $i < count($values[$group]); ++$i)
                        array_push($values_by_iterations, array());
                }

                if ($group_sizes[$group] != count($values[$group])) {
                    $this->exit_with_error('IterationGroupSizeIsInconsistent',
                        array('parent' => $test_metric['test_id'],
                        'metric' => $test_metric['metric_name'],
                        'childTest' => $test_name,
                        'groupSizes' => $group_sizes,
                        'group' => $group,
                        'values' => $values));
                }
            }
            $first_test = FALSE;

            if (count($values) != count($group_sizes)) {
                // FIXME: We should support bootstrapping or just computing the mean in this case.
                $this->exit_with_error('IterationGroupCountIsInconsistent', array('parent' => $test_metric['test_id'],
                    'metric' => $test_metric['metric_name'], 'childTest' => $name_and_values['name'],
                    'valuesByIterations' => $values_by_iterations, 'values' => $values));
            }

            $flattened_iteration_index = 0;
            for ($group = 0; $group < count($values); ++$group) {
                for ($i = 0; $i < count($values[$group]); ++$i) {
                    $run_iteration_value = $values[$group][$i];
                    if (!is_numeric($run_iteration_value)) {
                        $this->exit_with_error('NonNumeralIterationValueForAggregation', array('parent' => $test_metric['test_id'],
                            'metric' => $test_metric['metric_name'], 'childTest' => $name_and_values['name'],
                            'valuesByIterations' => $values_by_iterations, 'values' => $values, 'index' => $i));
                    }
                    array_push($values_by_iterations[$flattened_iteration_index], $run_iteration_value);
                    $flattened_iteration_index++;
                }
            }
        }

        if (!$values_by_iterations)
            $this->exit_with_error('NoIterationToAggregation', array('parent' => $test_metric['test_id'], 'metric' => $test_metric['metric_name']));

        return array('values' => $values_by_iterations, 'group_sizes' => $group_sizes);
    }

    private function aggregate_values($aggregator, $values) {
        switch ($aggregator) {
        case 'Arithmetic':
            return array_sum($values) / count($values);
        case 'Geometric':
            return pow(array_product($values), 1 / count($values));
        case 'Harmonic':
            return count($values) / array_sum(array_map(function ($x) { return 1 / $x; }, $values));
        case 'Total':
            return array_sum($values);
        case 'SquareSum':
            return array_sum(array_map(function ($x) { return $x * $x; }, $values));
        default:
            $this->exit_with_error('UnknownAggregator', array('aggregator' => $aggregator));
        }
        return NULL;
    }

    function compute_caches() {
        $expressions = array();
        $size = count($this->values_to_commit);
        for ($i = 0; $i < $size; ++$i) {
            $flattened_value = array();
            foreach ($this->values_to_commit[$i]['values'] as $group) {
                for ($j = 0; $j < count($group); ++$j) {
                    $iteration_value = $group[$j];
                    if (gettype($iteration_value) === 'array') { // [relative time, value]
                        if (count($iteration_value) != 2) {
                            // FIXME: Also report test and metric.
                            $this->exit_with_error('InvalidIterationValueFormat', array('values' => $this->values_to_commit[$i]['values']));
                        }
                        $iteration_value = $iteration_value[1];
                    }
                    array_push($flattened_value, $iteration_value);                    
                }
            }
            $this->values_to_commit[$i]['mean'] = $this->aggregate_values('Arithmetic', $flattened_value);
            $this->values_to_commit[$i]['sum'] = $this->aggregate_values('Total', $flattened_value);
            $this->values_to_commit[$i]['square_sum'] = $this->aggregate_values('SquareSum', $flattened_value);
        }
    }

    function add_values_to_commit($metric_id, $config_type, $values) {
        array_push($this->values_to_commit, array('metric_id' => $metric_id, 'config_type' => $config_type, 'values' => $values));
    }

    function commit($platform_id, $build_id) {
        $this->db->begin_transaction() or $this->exit_with_error('FailedToBeginTransaction');

        foreach ($this->values_to_commit as $item) {
            $config_data = array('metric' => $item['metric_id'], 'type' => $item['config_type'], 'platform' => $platform_id);
            $config_id = $this->db->select_or_insert_row('test_configurations', 'config', $config_data);
            if (!$config_id)
                $this->rollback_with_error('FailedToObtainConfiguration', $config_data);

            $values = $item['values'];
            $total_count = 0;
            for ($group = 0; $group < count($values); ++$group)
                $total_count += count($values[$group]);
            $run_data = array('config' => $config_id, 'build' => $build_id, 'iteration_count_cache' => $total_count,
                'mean_cache' => $item['mean'], 'sum_cache' => $item['sum'], 'square_sum_cache' => $item['square_sum']);
            $run_id = $this->db->insert_row('test_runs', 'run', $run_data);
            if (!$run_id)
                $this->rollback_with_error('FailedToInsertRun', array('metric' => $item['metric_id'], 'param' => $run_data));

            $flattened_order = 0;
            for ($group = 0; $group < count($values); ++$group) {
                for ($i = 0; $i < count($values[$group]); ++$i) {
                    $iteration_value = $values[$group][$i];
                    $relative_time = NULL;
                    if (gettype($iteration_value) === 'array') {
                        assert(count($iteration_value) == 2); // compute_caches checks this condition.
                        $relative_time = $iteration_value[0];
                        $iteration_value = $iteration_value[1];
                    }
                    $param = array('run' => $run_id, 'order' => $flattened_order, 'value' => $iteration_value,
                        'group' => count($values) == 1 ? NULL : $group, 'relative_time' => $relative_time);
                    $this->db->insert_row('run_iterations', 'iteration', $param, NULL)
                        or $this->rollback_with_error('FailedToInsertIteration', array('config' => $config_id, 'build' => $build_id, 'param' => $param));
                    $flattened_order++;
                }
            }
        }

        $this->db->query_and_get_affected_rows("UPDATE reports SET report_committed_at = CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
            WHERE report_id = $1", array($this->report_id));

        $this->db->commit_transaction() or $this->exit_with_error('FailedToCommitTransaction');
    }

    private function rollback_with_error($message, $details) {
        $this->db->rollback_transaction();
        $this->exit_with_error($message, $details);
    }
};

?>
