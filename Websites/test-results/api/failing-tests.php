<?php

require_once('../include/json-shared.php');
require_once('../include/test-results.php');

$db = connect();

require_existence_of($_GET, array('builder' => '/^[A-Za-z0-9 \(\)\-_]+$/'));
$builder_name = $_GET['builder'];
$number_of_days = array_get($_GET, 'days');
if ($number_of_days) {
    require_format('number_of_days', $number_of_days, '/^[0-9]+$/');
    $number_of_days = intval($number_of_days);
} else
    $number_of_days = 3;

$builder_row = $db->select_first_row('builders', NULL, array('name' => $builder_name));
if (!$builder_row)
    exit_with_error('BuilderNotFound');
$builder_id = $builder_row['id'];

$result_rows = $db->query_and_fetch_all(
'SELECT results.*, builds.*, array_agg((build_revisions.repository, build_revisions.value, build_revisions.time)) AS revisions
    FROM results, builds, build_revisions
    WHERE build_revisions.build = builds.id AND results.build = builds.id AND builds.builder = $1
    AND results.actual != $2 AND builds.start_time > now() - interval \'' . $number_of_days . ' days\'
    GROUP BY results.id, builds.id', array($builder_id, 'PASS'));
if (!$result_rows)
    exit_with_error('ResultsNotFound');

exit_with_success(format_result_rows($result_rows));

?>
