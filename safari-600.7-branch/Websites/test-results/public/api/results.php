<?php

require_once('../include/json-shared.php');
require_once('../include/test-results.php');

$db = connect();

require_existence_of($_GET, array('test' => '/[A-Za-z0-9\._\- ]+/'));

$test = $db->select_first_row('tests', NULL, array('name' => $_GET['test']));
if (!$test)
    exit_with_error('TestNotFound');

$result_rows = $db->query_and_fetch_all(
'SELECT results.*, builds.*, array_agg((build_revisions.repository, build_revisions.value, build_revisions.time)) AS revisions
    FROM results, builds, build_revisions
    WHERE build_revisions.build = builds.id AND results.build = builds.id AND results.test = $1
    GROUP BY results.id, builds.id', array($test['id']));
if (!$result_rows)
    exit_with_error('ResultsNotFound');

$builders = array();
foreach ($result_rows as $result)
    array_push(array_ensure_item_has_array($builders, $result['builder']), format_result($result));

exit_with_success(array('builders' => $builders));

?>
