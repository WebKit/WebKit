<?php
ini_set('memory_limit', '1024M');

ini_set('memory_limit', '1024M');

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

function add_build($db, $builder_id, $build_number, $slave_id) {
    return $db->select_or_insert_row('builds', NULL, array('builder' => $builder_id, 'number' => $build_number, 'slave' => $slave_id));
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

function store_test_results($db, $test_results, $build_id, $start_time, $end_time) {
    $db->begin_transaction();

    try {
        recursively_add_test_results($db, $build_id, $test_results['tests'], '');

        $db->query_and_get_affected_rows(
            'UPDATE builds SET (start_time, end_time, is_processed) = (least($1, start_time), greatest($2, end_time), FALSE) WHERE id = $3',
            array($start_time->format('Y-m-d H:i:s.u'), $end_time->format('Y-m-d H:i:s.u'), $build_id));
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
        require_format('test_modifiers', $modifiers, '/^[A-Za-z0-9 \.\/\+]+$/');
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

class ResultsJSONWriter {
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

    public function add_results_for_test($current_test, $current_results) {
        if (!count($current_results))
            return;
        // FIXME: Why do we need to check the count?

        $prefix = $this->emitted_results ? ",\n" : "";
        fwrite($this->fp, "$prefix\"$current_test\":");
        fwrite($this->fp, json_encode($current_results, true));

        $this->emitted_results = TRUE;
    }
}

class ResultsJSONGenerator {
    private $db;
    private $builder_id;

    public function __construct($db, $builder_id)
    {
        $this->db = $db;
        $this->builder_id = $builder_id;
    }

    public function generate($failure_type)
    {
        $start_time = microtime(true);

        if (!$this->builder_id)
            return FALSE;

        switch ($failure_type) {
        case 'flaky':
            $test_rows = $this->db->query_and_fetch_all("SELECT DISTINCT(results.test) FROM results,
                (SELECT builds.id FROM builds WHERE builds.builder = $1 GROUP BY builds.id LIMIT 500) as builds
                WHERE results.build = builds.id AND results.is_flaky is TRUE",
                array($this->builder_id));
            break;
        case 'wrongexpectations':
            // FIXME: three replace here shouldn't be necessary. Do it in webkitpy or report.php at latest.
            $test_rows = $this->db->query_and_fetch_all("SELECT results.test FROM results WHERE results.build = $1
                AND NOT string_to_array(expected, ' ') >=
                    string_to_array(replace(replace(replace(actual, 'TEXT', 'FAIL'), 'AUDIO', 'FAIL'), 'IMAGE+TEXT', 'FAIL'), ' ')",
                array($this->latest_build()));
            break;
        default:
            return FALSE;
        }

        if (!$test_rows)
            return TRUE;

        $comma_separated_test_ids = '';
        foreach ($test_rows as $row) {
            if ($comma_separated_test_ids)
                $comma_separated_test_ids .= ', ';
            $comma_separated_test_ids .= intval($row['test']);
        }

        $all_results = $this->db->query(
        "SELECT results.*, builds.* FROM results
            JOIN (SELECT builds.*, array_agg((build_revisions.repository, build_revisions.value, build_revisions.time)) AS revisions
                    FROM builds, build_revisions
                    WHERE build_revisions.build = builds.id AND builds.builder = $1
                    GROUP BY builds.id LIMIT 500) as builds ON results.build = builds.id
            WHERE results.test in ($comma_separated_test_ids)
            ORDER BY results.test DESC", array($this->builder_id));
        if (!$all_results)
            return FALSE;

        $json_fp = $this->open_json_for_failure_type($failure_type);
        try {
            return $this->write_jsons($all_results, new ResultsJSONWriter($json_fp), $start_time);
        } catch (Exception $exception) {
            fclose($json_fp);
            throw $exception;
        }
        return FALSE;
    }

    private function latest_build() {
        $results = $this->db->query_and_fetch_all('SELECT builds.id, max(build_revisions.time) AS latest_revision_time
            FROM builds, build_revisions
            WHERE build_revisions.build = builds.id AND builds.builder = $1
            GROUP BY builds.id
            ORDER BY latest_revision_time DESC LIMIT 1', array($this->builder_id));
        if (!$results)
            return NULL;
        return $results[0]['id'];
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

    private function write_jsons($all_results, $writer, $start_time) {
        $writer->start($this->builder_id);
        $current_test = NULL;
        $current_results = array();
        while ($result = $this->db->fetch_next_row($all_results)) {
            if ($result['test'] != $current_test) {
                if ($current_test)
                    $writer->add_results_for_test($current_test, $current_results);
                $current_results = array();
                $current_test = $result['test'];
            }
            array_push($current_results, format_result($result));
        }
        $writer->end(microtime(true) - $start_time);
        return TRUE;
    }
}

function update_flakiness_for_build($db, $preceeding_build, $current_build, $succeeding_build) {
    return $db->query_and_get_affected_rows("UPDATE results
        SET is_flaky = preceeding_results.actual = succeeding_results.actual AND preceeding_results.actual != results.actual
        FROM results preceeding_results, results succeeding_results
        WHERE preceeding_results.build = $1 AND results.build = $2 AND succeeding_results.build = $3
            AND preceeding_results.test = results.test AND succeeding_results.test = results.test
            AND (results.is_flaky IS NULL OR results.is_flaky !=
                    (preceeding_results.actual = succeeding_results.actual AND preceeding_results.actual != results.actual))",
            array($preceeding_build['id'], $current_build['id'], $succeeding_build['id']));
}

function update_flakiness_after_inserting_build($db, $build_id) {
    // FIXME: In theory, it's possible for new builds to be inserted between the time this select query is ran and quries are executed by update_flakiness_for_build.
    $ordered_builds = $db->query_and_fetch_all("SELECT builds.id, max(build_revisions.time) AS latest_revision_time
        FROM builds, build_revisions
        WHERE build_revisions.build = builds.id AND builds.builder = (SELECT builds.builder FROM builds WHERE id = $1)
        GROUP BY builds.id ORDER BY latest_revision_time, builds.start_time DESC", array($build_id));

    $current_build = NULL;
    for ($i = 0; $i < count($ordered_builds); $i++) {
        if ($ordered_builds[$i]['id'] == $build_id) {
            $current_build = $i;
            break;
        }
    }
    if ($current_build === NULL)
        return NULL;

    $affected_rows = 0;
    if ($current_build >= 2)
        $affected_rows += update_flakiness_for_build($db, $ordered_builds[$current_build - 2], $ordered_builds[$current_build - 1], $ordered_builds[$current_build]);

    if ($current_build >= 1 && $current_build + 1 < count($ordered_builds))
        $affected_rows += update_flakiness_for_build($db, $ordered_builds[$current_build - 1], $ordered_builds[$current_build], $ordered_builds[$current_build + 1]);

    if ($current_build + 2 < count($ordered_builds))
        $affected_rows += update_flakiness_for_build($db, $ordered_builds[$current_build], $ordered_builds[$current_build + 1], $ordered_builds[$current_build + 2]);

    return $affected_rows;
}

?>
