<?php

require_once('../include/json-shared.php');

$db = connect();
$tests = $db->fetch_table('tests');

function fetch_table_and_create_map_by_id($table_name) {
    global $db;

    $rows = $db->fetch_table($table_name);
    if (!$rows)
        return array();

    $results = array();
    foreach ($rows as $row) {
        $results[$row['id']] = $row;
    }
    return $results;
}

exit_with_success(array('tests' => $tests,
    'builders' => fetch_table_and_create_map_by_id('builders'),
    'slaves' => fetch_table_and_create_map_by_id('slaves'),
    'repositories' => fetch_table_and_create_map_by_id('repositories')));

?>
