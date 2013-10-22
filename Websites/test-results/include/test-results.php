<?php

require_once('db.php');

function float_to_time($time_in_float) {
    $time = new DateTime();
    $time->setTimestamp(floatval($time_in_float));
    return $time;
}

function add_build($db, $master, $builder_name, $build_number) {
    if (!in_array($master, config('masters')))
        return NULL;

    $builder_id = $db->select_or_insert_row('builders', NULL, array('master' => $master, 'name' => $builder_name));
    if (!$builder_id)
        return NULL;

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

    require_format('expected_result', $tests['expected'], '/^[A-Za-z ]+$/');
    require_format('actual_result', $tests['actual'], '/^[A-Za-z ]+$/');
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

function format_result_rows($result_rows) {
    $builders = array();
    foreach ($result_rows as $result) {
        array_push(array_ensure_item_has_array(array_ensure_item_has_array($builders, $result['builder']), $result['test']),
            format_result($result));
    }
    return array('builders' => $builders);
}

?>
