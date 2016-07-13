<?php

include('../include/admin-header.php');
include('../include/test-results.php');

$build_id = intval($_GET['build']);

$start_time = microtime(true);
$affected_rows = update_flakiness_after_inserting_build($db, $build_id);
$total_time = microtime(true) - $start_time;
if ($total_time === NULL)
    notice('Build not found');
else
    echo "Updated $affected_rows rows for build $build_id in $total_time seconds";

include('../include/admin-footer.php');

?>
