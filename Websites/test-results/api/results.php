<?php

require_once('../include/json-shared.php');

$db = connect();

require_existence_of($_GET, array('test' => '/[A-Za-z0-9\._\- ]+/'));

$test = $db->select_first_row('tests', NULL, array('name' => $_GET['test']));
if (!$test)
    exit_with_error('TestNotFound');

$result_rows = $db->query_and_fetch_all(
'SELECT results.*, builds.*, array_agg((build_revisions.repository, build_revisions.value, build_revisions.time)) AS revisions
    FROM results, builds, build_revisions
    WHERE build_revisions.build = builds.id AND results.test = $1 AND results.build = builds.id
    GROUP BY results.id, builds.id', array($test['id']));
if (!$result_rows)
    exit_with_error('ResultsNotFound');

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

$builders = array();
foreach ($result_rows as $result) {
    array_push(array_ensure_item_has_array($builders, $result['builder']),
        array('buildTime' => strtotime($result['start_time']) * 1000,
        'revisions' => parse_revisions_array($result['revisions']),
        'builder' => $result['builder'],
        'slave' => $result['slave'],
        'buildNumber' => $result['number'],
        'actual' => $result['actual'],
        'expected' => $result['expected']));
}

exit_with_success(array('builders' => $builders));

?>
