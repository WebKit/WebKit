<?php

require_once('../include/json-header.php');

class ReportProcessor {
    private $db;
    private $name_to_aggregator_id;
    private $report_id;
    private $runs;

    function __construct($db) {
        $this->db = $db;
        $this->name_to_aggregator_id = array();
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

        $build_data = $this->authenticate_and_construct_build_data($report, $existing_report_id);
        if (!$existing_report_id)
            $this->store_report($report, $build_data);

        $this->ensure_aggregators();

        $this->fetch_tests();
        $this->runs = new TestRunsGenerator($this->db, $this->name_to_aggregator_id, $this->report_id);
        $this->recursively_ensure_tests($report['tests']);

        $this->runs->aggregate();
        $this->runs->compute_caches();

        $this->db->begin_transaction() or $this->exit_with_error('FailedToBeginTransaction');

        $platform_id = $this->db->select_or_insert_row('platforms', 'platform', array('name' => $report['platform']));
        if (!$platform_id)
            $this->rollback_with_error('FailedToInsertPlatform', array('name' => $report['platform']));

        // FIXME: Deprecate and unsupport "jobId".
        $build_id = $this->resolve_build_id($build_data, array_get($report, 'revisions', array()),
            array_get($report, 'jobId', array_get($report, 'buildRequest')));

        $this->db->commit_transaction() or $this->exit_with_error('FailedToCommitTransaction');

        $this->runs->commit($platform_id, $build_id);
    }

    private function authenticate_and_construct_build_data(&$report, $existing_report_id) {
        $builder_info = array('name' => $report['builderName']);
        $slave_name = array_get($report, 'slaveName', NULL);
        $slave_id = NULL;
        if (!$existing_report_id) {
            $hash = NULL;
            if ($slave_name && array_key_exists('slavePassword', $report)) {
                $universal_password = config('universalSlavePassword');
                if ($slave_name && $universal_password && $universal_password == $report['slavePassword'])
                    $slave_id = $this->db->select_or_insert_row('build_slaves', 'slave', array('name' => $slave_name));
                else {
                    $hash = hash('sha256', $report['slavePassword']);
                    $slave = $this->db->select_first_row('build_slaves', 'slave', array('name' => $slave_name, 'password_hash' => $hash));
                    if ($slave)
                        $slave_id = $slave['slave_id'];
                }
            } else if (array_key_exists('builderPassword', $report))
                $hash = hash('sha256', $report['builderPassword']);

            if (!$hash && !$slave_id)
                $this->exit_with_error('BuilderNotFound');
            if (!$slave_id)
                $builder_info['password_hash'] = $hash;
        }

        if (array_key_exists('builderPassword', $report))
            unset($report['builderPassword']);
        if (array_key_exists('slavePassword', $report))
            unset($report['slavePassword']);

        $builder_id = NULL;
        if ($slave_id)
            $builder_id = $this->db->select_or_insert_row('builders', 'builder', $builder_info);
        else {
            $builder = $this->db->select_first_row('builders', 'builder', $builder_info);
            if (!$builder)
                $this->exit_with_error('BuilderNotFound', array('name' => $builder_info['name']));
            $builder_id = $builder['builder_id'];
            if ($slave_name)
                $slave_id = $this->db->select_or_insert_row('build_slaves', 'slave', array('name' => $slave_name));
        }

        return $this->construct_build_data($report, $builder_id, $slave_id);
    }

    private function construct_build_data(&$report, $builder_id, $slave_id) {
        array_key_exists('buildNumber', $report) or $this->exit_with_error('MissingBuildNumber');
        array_key_exists('buildTime', $report) or $this->exit_with_error('MissingBuildTime');

        return array('builder' => $builder_id, 'slave' => $slave_id, 'number' => $report['buildNumber'], 'time' => $report['buildTime']);
    }

    private function store_report(&$report, $build_data) {
        assert(!$this->report_id);
        $this->report_id = $this->db->insert_row('reports', 'report', array(
            'builder' => $build_data['builder'],
            'slave' => $build_data['slave'],
            'build_number' => $build_data['number'],
            'content' => json_encode($report)));
        if (!$this->report_id)
            $this->exit_with_error('FailedToStoreRunReport');
    }

    private function ensure_aggregators() {
        foreach (TestRunsGenerator::$aggregators as $name) {
            $id = $this->db->select_or_insert_row('aggregators', 'aggregator', array('name' => $name));
            if (!$id)
                $this->exit_with_error('FailedToInsertAggregator', array('aggregator' => $name));
            $this->name_to_aggregator_id[$name] = $id;
        }
    }

    private function resolve_build_id(&$build_data, $revisions, $build_request_id) {
        $results = $this->db->query_and_fetch_all("SELECT build_id, build_slave FROM builds
            WHERE build_builder = $1 AND build_number = $2 AND build_time <= $3 AND build_time + interval '1 day' > $3 LIMIT 2",
            array($build_data['builder'], $build_data['number'], $build_data['time']));
        if ($results) {
            $first_result = $results[0];
            if ($first_result['build_slave'] != $build_data['slave'])
                $this->rollback_with_error('MismatchingBuildSlave', array('storedBuild' => $results, 'reportedBuildData' => $build_data));
            $build_id = $first_result['build_id'];
        } else
            $build_id = $this->db->insert_row('builds', 'build', $build_data);

        if (!$build_id)
            $this->rollback_with_error('FailedToInsertBuild', $build_data);

        if ($build_request_id) {
            if ($this->db->update_row('build_requests', 'request', array('id' => $build_request_id, 'build' => null), array('status' => 'completed', 'build' => $build_id))
                != $build_request_id)
                $this->rollback_with_error('FailedToUpdateBuildRequest', array('buildRequest' => $build_request_id, 'build' => $build_id));
        }


        foreach ($revisions as $repository_name => $revision_data) {
            $repository_id = $this->db->select_or_insert_row('repositories', 'repository', array('name' => $repository_name, 'owner' => NULL));
            if (!$repository_id)
                $this->rollback_with_error('FailedToInsertRepository', array('name' => $repository_name));

            $commit_data = array('repository' => $repository_id, 'revision' => $revision_data['revision'], 'time' => array_get($revision_data, 'timestamp'));

            $mismatching_commit = $this->db->query_and_fetch_all('SELECT * FROM build_commits, commits
                WHERE build_commit = commit_id AND commit_build = $1 AND commit_repository = $2 AND commit_revision != $3 LIMIT 1',
                array($build_id, $repository_id, $revision_data['revision']));
            if ($mismatching_commit)
                $this->rollback_with_error('MismatchingCommitRevision', array('build' => $build_id, 'existing' => $mismatching_commit, 'new' => $commit_data));

            $commit_row = $this->db->select_or_insert_row('commits', 'commit',
                array('repository' => $repository_id, 'revision' => $revision_data['revision']), $commit_data, '*');
            if (!$commit_row)
                $this->rollback_with_error('FailedToRecordCommit', $commit_data);

            if ($commit_data['time'] && abs(Database::to_js_time($commit_row['commit_time']) - Database::to_js_time($commit_data['time'])) > 1000.0)
                $this->rollback_with_error('MismatchingCommitTime', array('existing' => $commit_row, 'new' => $commit_data));

            if (!$this->db->select_or_insert_row('build_commits', null,
                array('commit_build' => $build_id, 'build_commit' => $commit_row['commit_id']), null, '*'))
                $this->rollback_with_error('FailedToRelateCommitToBuild', array('commit' => $commit_row, 'build' => $build_id));
        }

        return $build_id;
    }

    private function rollback_with_error($message, $details) {
        $this->db->rollback_transaction();
        $this->exit_with_error($message, $details);
    }

    private function fetch_tests() {
        $test_rows = $this->db->fetch_table('tests');
        $tests = array();
        $test_by_id = array();
        foreach ($test_rows as &$test) {
            $test_by_id[$test['test_id']] = &$test;
            $test['metrics'] = array();
            $parent_id = $test['test_parent'];
            if ($parent_id == NULL)
                $parent_id = 0;
            $parent_array = &array_ensure_item_has_array($tests, $parent_id);
            $parent_array[$test['test_name']] = &$test;
        }
        $this->tests = &$tests;

        $metric_rows = $this->db->fetch_table('test_metrics');
        foreach ($metric_rows as &$metric) {
            $test = &$test_by_id[$metric['metric_test']];
            $metrics_by_name = &array_ensure_item_has_array($test['metrics'], $metric['metric_name']);
            $metrics_by_name[$metric['metric_aggregator']] = $metric['metric_id'];
        }
    }

    private function recursively_ensure_tests(&$tests, $parent_id = NULL, $level = 0) {
        foreach ($tests as $test_name => $test) {
            $test_row = array_get(array_get($this->tests, $parent_id ? $parent_id : 0, array()), $test_name);
            if ($test_row)
                $test_id = intval($test_row['test_id']);
            else {
                $test_id = $this->db->select_or_insert_row('tests', 'test', $parent_id ? array('name' => $test_name, 'parent' => $parent_id) : array('name' => $test_name),
                    array('name' => $test_name, 'parent' => $parent_id, 'url' => array_get($test, 'url')));
            }
            if (!$test_id)
                $this->exit_with_error('FailedToAddTest', array('name' => $test_name, 'parent' => $parent_id));

            if (array_key_exists('tests', $test))
                $this->recursively_ensure_tests($test['tests'], $test_id, $level + 1);

            foreach (array_get($test, 'metrics', array()) as $metric_name => &$aggregators_or_config_types) {
                $aggregators = $this->aggregator_list_if_exists($aggregators_or_config_types);
                if ($aggregators) {
                    foreach ($aggregators as $aggregator_name) {
                        $aggregator_id = array_get($this->name_to_aggregator_id, $aggregator_name, NULL);
                        if ($aggregator_id == NULL)
                            $this->exit_with_error('AggregatorNotFound', array('name' => $aggregator_name));

                        $metrics = $test_row ? array_get($test_row['metrics'], $metric_name) : NULL;
                        $metric_id = $metrics ? $metrics[$aggregator_id] : NULL;
                        if (!$metric_id) {
                            $metric_id = $this->db->select_or_insert_row('test_metrics', 'metric', array('name' => $metric_name,
                                'test' => $test_id, 'aggregator' => $this->name_to_aggregator_id[$aggregator_name]));
                        }
                        if (!$metric_id)
                            $this->exit_with_error('FailedToAddAggregatedMetric', array('name' => $metric_name, 'test' => $test_id, 'aggregator' => $aggregator_name));

                        $this->runs->add_aggregated_metric($parent_id, $test_id, $test_name, $metric_id, $metric_name, $aggregator_name, $level);
                    }
                } else {
                    $metrics = $test_row ? array_get($test_row['metrics'], $metric_name) : NULL;
                    $metric_id = $metrics ? array_get($metrics, '') : NULL;
                    if (!$metric_id)
                        $metric_id = $this->db->select_or_insert_row('test_metrics', 'metric', array('name' => $metric_name, 'test' => $test_id));
                    if (!$metric_id)
                        $this->exit_with_error('FailedToAddMetric', array('name' => $metric_name, 'test' => $test_id));

                    foreach ($aggregators_or_config_types as $config_type => &$values) {
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

    private function aggregator_list_if_exists(&$aggregators_or_config_types) {
        if (array_key_exists(0, $aggregators_or_config_types))
            return $aggregators_or_config_types;
        else if (array_get($aggregators_or_config_types, 'aggregators'))
            return $aggregators_or_config_types['aggregators'];
        return NULL;
    }
};

class TestRunsGenerator {
    private $db;
    private $name_to_aggregator_id;
    private $report_id;
    private $metrics_to_aggregate;
    private $parent_to_values;
    private $values_to_commit;

    function __construct($db, $name_to_aggregator_id, $report_id) {
        $this->db = $db;
        $this->name_to_aggregator_id = $name_to_aggregator_id;
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

    function add_aggregated_metric($parent_id, $test_id, $test_name, $metric_id, $metric_name, $aggregator_name, $level) {
        array_push($this->metrics_to_aggregate, array(
            'test_id' => $test_id,
            'parent_id' => $parent_id,
            'metric_id' => $metric_id,
            'test_name' => $test_name,
            'metric_name' => $metric_name,
            'aggregator' => $aggregator_name,
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
                    'metric' => $test_metric['metric_name'], 'childTest' => $test_name,
                    'valuesByIterations' => $values_by_iterations, 'values' => $values));
            }

            $flattened_iteration_index = 0;
            for ($group = 0; $group < count($values); ++$group) {
                for ($i = 0; $i < count($values[$group]); ++$i) {
                    $run_iteration_value = $values[$group][$i];
                    if (!is_numeric($run_iteration_value)) {
                        $this->exit_with_error('NonNumeralIterationValueForAggregation', array('parent' => $test_metric['test_id'],
                            'metric' => $test_metric['metric_name'], 'childTest' => $test_name,
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

    static public $aggregators = array('Arithmetic', 'Geometric', 'Harmonic', 'Total');

    private function aggregate_values($aggregator, $values) {
        switch ($aggregator) {
        case 'Arithmetic':
            return array_sum($values) / count($values);
        case 'Geometric':
            return exp(array_sum(array_map(function ($x) { return log($x); }, $values)) / count($values));
        case 'Harmonic':
            return count($values) / array_sum(array_map(function ($x) { return 1 / $x; }, $values));
        case 'Total':
            return array_sum($values);
        case 'SquareSum': # This aggregator is only used internally to compute run_square_sum_cache in test_runs table.
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

        $this->db->query_and_get_affected_rows("UPDATE reports
            SET (report_committed_at, report_build) = (CURRENT_TIMESTAMP AT TIME ZONE 'UTC', $2)
            WHERE report_id = $1", array($this->report_id, $build_id));

        $this->db->commit_transaction() or $this->exit_with_error('FailedToCommitTransaction');
    }

    private function rollback_with_error($message, $details) {
        $this->db->rollback_transaction();
        $this->exit_with_error($message, $details);
    }
};

?>
