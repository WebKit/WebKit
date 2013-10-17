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
        foreach ($test_results['tests'] as $name => $subtests)
            recursively_add_test_results($db, $build_id, $subtests, $name);
        $db->query_and_get_affected_rows('UPDATE builds SET (start_time, end_time, slave, fetched) = ($1, $2, $3, TRUE) WHERE id = $4',
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
        foreach ($tests as $name => $subtests)
            recursively_add_test_results($db, $build_id, $subtests, $full_name . '/' . $name);
        return;
    }

    $test_id = $db->select_or_insert_row('tests', NULL,
        array('name' => $full_name), array('name' => $full_name, 'reftest_type' => json_encode(array_get($tests, 'reftest_type'))));

    // FIXME: Either use a transaction or check the return value.
    $db->select_or_insert_row('results', NULL, array('test' => $test_id, 'build' => $build_id,
        'expected' => $tests['expected'], 'actual' => $tests['actual']));
}

?>
