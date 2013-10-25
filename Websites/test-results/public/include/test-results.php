<?php

require_once('db.php');

function float_to_time($time_in_float) {
    $time = new DateTime();
    $time->setTimestamp(floatval($time_in_float));
    return $time;
}

function add_builder($db, $master, $builder_name) {
    if (!in_array($master, config('masters')))
        return NULL;

    return $db->select_or_insert_row('builders', NULL, array('master' => $master, 'name' => $builder_name));
}

function add_build($db, $builder_id, $build_number) {
    return $db->select_or_insert_row('builds', NULL, array('builder' => $builder_id, 'number' => $build_number));
}

function add_slave($db, $name) {
    return $db->select_or_insert_row('slaves', NULL, array('name' => $name));
}

function fetch_and_parse_test_results_json($url, $jsonp = FALSE) {
    $json_contents = file_get_contents($url);
    if (!$json_contents)
        return NULL;

    if ($jsonp)
        $json_contents = preg_replace('/^\w+\(|\);$/', '', $json_contents);

    return json_decode($json_contents, true);
}

function store_test_results($db, $test_results, $build_id, $start_time, $end_time, $slave_id) {
    $db->begin_transaction();

    try {
        recursively_add_test_results($db, $build_id, $test_results['tests'], '');

        $db->query_and_get_affected_rows(
            'UPDATE builds SET (start_time, end_time, slave) = (least($1, start_time), greatest($2, end_time), $3) WHERE id = $4',
            array($start_time->format('Y-m-d H:i:s.u'), $end_time->format('Y-m-d H:i:s.u'), $slave_id, $build_id));
        $db->commit_transaction();
    } catch (Exception $e) {
        $db->rollback_transaction();
        return FALSE;
    }

    return TRUE;
}

function recursively_add_test_results($db, $build_id, $tests, $full_name) {
    if (!array_key_exists('expected', $tests) and !array_key_exists('actual', $tests)) {
        $prefix = $full_name ? $full_name . '/' : '';
        foreach ($tests as $name => $subtests) {
            require_format('test_name', $name, '/^[A-Za-z0-9 +_\-\.]+$/');
            recursively_add_test_results($db, $build_id, $subtests, $prefix . $name);
        }
        return;
    }

    require_format('expected_result', $tests['expected'], '/^[A-Za-z \+]+$/');
    require_format('actual_result', $tests['actual'], '/^[A-Za-z \+]+$/');
    require_format('test_time', $tests['time'], '/^\d*$/');
    $modifiers = array_get($tests, 'modifiers');
    if ($modifiers)
        require_format('test_modifiers', $modifiers, '/^[A-Za-z0-9 \.\/]+$/');
    else
        $modifiers = NULL;
    $category = 'LayoutTest'; // FIXME: Support other test categories.

    $test_id = $db->select_or_insert_row('tests', NULL,
        array('name' => $full_name),
        array('name' => $full_name, 'reftest_type' => json_encode(array_get($tests, 'reftest_type')), 'category' => $category));

    $db->insert_row('results', NULL, array('test' => $test_id, 'build' => $build_id,
        'expected' => $tests['expected'], 'actual' => $tests['actual'],
        'time' => $tests['time'], 'modifiers' => $tests['modifiers']));
}

date_default_timezone_set('UTC');
function parse_revisions_array($postgres_array) {
    // e.g. {"(WebKit,131456,\"2012-10-16 14:53:00\")","(Safari,162004,)"}
    $outer_array = json_decode('[' . trim($postgres_array, '{}') . ']');
    $revisions = array();
    foreach ($outer_array as $item) {
        $name_and_revision = explode(',', trim($item, '()'));
        $time = strtotime(trim($name_and_revision[2], '"')) * 1000;
        $revisions[trim($name_and_revision[0], '"')] = array(trim($name_and_revision[1], '"'), $time);
    }
    return $revisions;
}

function format_result($result) {
    return array('buildTime' => strtotime($result['start_time']) * 1000,
        'revisions' => parse_revisions_array($result['revisions']),
        'slave' => $result['slave'],
        'buildNumber' => $result['number'],
        'actual' => $result['actual'],
        'expected' => $result['expected'],
        'time' => $result['time'],
        'modifiers' => $result['modifiers']);
}

abstract class ResultsJSONWriter {
    private $fp;
    private $emitted_results;

    public function __construct($fp) {
        $this->fp = $fp;
        $this->emitted_results = FALSE;
    }

    public function start($builder_id) {
        fwrite($this->fp, "{\"status\": \"OK\", \"builders\": {\"$builder_id\":{");
    }

    public function end($total_time) {
        fwrite($this->fp, "}}, \"totalGenerationTime\": $total_time}");
    }

    public function add_results_for_test_if_matches($current_test, $current_results) {
        if (!count($current_results) || $this->pass_for_failure_type($current_results))
            return;
        // FIXME: Why do we need to check the count?

        $prefix = $this->emitted_results ? ",\n" : "";
        fwrite($this->fp, "$prefix\"$current_test\":");
        fwrite($this->fp, json_encode($current_results, true));

        $this->emitted_results = TRUE;
    }

    abstract protected function pass_for_failure_type(&$results);
}

class FailingResultsJSONWriter extends ResultsJSONWriter {
    public function __construct($fp) { parent::__construct($fp); }
    protected function pass_for_failure_type(&$results) {
        return $results[0]['actual'] == 'PASS';
    }
}

class FlakyResultsJSONWriter extends ResultsJSONWriter {
    public function __construct($fp) { parent::__construct($fp); }
    protected function pass_for_failure_type(&$results) {
        $last_index = count($results) - 1;
        for ($i = 1; $i < $last_index; $i++) {
            $previous_actual = $results[$i - 1]['actual'];
            $next_actual = $results[$i + 1]['actual'];
            if ($previous_actual == $next_actual && $results[$i]['actual'] != $previous_actual) {
                $results[$i]['oneOffChange'] = TRUE;
                return FALSE;
            }
        }
        return TRUE;
    }
}

class WrongExpectationsResultsJSONWriter extends ResultsJSONWriter {
    public function __construct($fp) { parent::__construct($fp); }
    protected function pass_for_failure_type(&$results) {
        $latest_expected_result = $results[0]['expected'];
        $latest_actual_result = $results[0]['actual'];

        if ($latest_expected_result == $latest_actual_result)
            return TRUE;

        $tokens = explode(' ', $latest_expected_result);
        return array_search($latest_actual_result, $tokens) !== FALSE
            || (($latest_actual_result == 'TEXT' || $latest_actual_result == 'TEXT+IMAGE') && array_search('FAIL', $tokens) !== FALSE);
    }
}

class ResultsJSONGenerator {
    private $db;
    private $builder_id;

    const MAXIMUM_NUMBER_OF_DAYS = 30;

    public function __construct($db, $builder_id)
    {
        $this->db = $db;
        $this->builder_id = $builder_id;
    }

    public function generate()
    {
        $start_time = microtime(true);

        if (!$this->builder_id)
            return FALSE;

        $number_of_days = self::MAXIMUM_NUMBER_OF_DAYS;
        $all_results = $this->db->query(
        "SELECT results.*, builds.* FROM results
            JOIN (SELECT builds.*, array_agg((build_revisions.repository, build_revisions.value, build_revisions.time)) AS revisions
                    FROM builds, build_revisions
                    WHERE build_revisions.build = builds.id AND builds.builder = $1 AND builds.start_time > now() - interval '$number_of_days days'
                    GROUP BY builds.id
                    ORDER BY max(build_revisions.time) DESC) as builds ON results.build = builds.id
            ORDER BY results.test", array($this->builder_id));
        if (!$all_results)
            return FALSE;

        $failing_json_fp = $this->open_json_for_failure_type('failing');
        try {
            $flaky_json_fp = $this->open_json_for_failure_type('flaky');
            try {
                $wrongexpectations_json_fp = $this->open_json_for_failure_type('wrongexpectations');
                try {
                    return $this->write_jsons($all_results, array(
                        new FailingResultsJSONWriter($failing_json_fp),
                        new FlakyResultsJSONWriter($flaky_json_fp),
                        new WrongExpectationsResultsJSONWriter($wrongexpectations_json_fp)), $start_time);
                } catch (Exception $exception) {
                    fclose($wrongexpectations_json_fp);
                    throw $exception;
                }
            } catch (Exception $exception) {
                fclose($flaky_json_fp);
                throw $exception;
            }
        } catch (Exception $exception) {
            fclose($failing_json_fp);
            throw $exception;
        }
        return FALSE;
    }

    private function open_json_for_failure_type($failure_type) {
        $failing_json_path = configPath('dataDirectory', $this->builder_id . "-$failure_type.json");
        if (!$failing_json_path)
            exit_with_error('FailedToDetermineResultsJSONPath', array('builderId' => $this->builder_id, 'failureType' => $failure_type));
        $fp = fopen($failing_json_path, 'w');
        if (!$fp)
            exit_with_error('FailedToOpenResultsJSON', array('builderId' => $this->builder_id, 'failureType' => $failure_type));
        return $fp;
    }

    private function write_jsons($all_results, $writers, $start_time) {
        foreach ($writers as $writer)
            $writer->start($this->builder_id);
        $current_test = NULL;
        $current_results = array();
        while ($result = $this->db->fetch_next_row($all_results)) {
            if ($result['test'] != $current_test) {
                if ($current_test) {
                    foreach ($writers as $writer)
                        $writer->add_results_for_test_if_matches($current_test, $current_results);
                }
                $current_results = array();
                $current_test = $result['test'];
            }
            array_push($current_results, format_result($result));
        }

        $total_time = microtime(true) - $start_time;
        foreach ($writers as $writer)
            $writer->end($total_time);

        return TRUE;
    }
}

?>
