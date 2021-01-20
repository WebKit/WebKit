<?php

require('../include/json-header.php');
require('../include/commit-updater.php');

function main($post_data)
{
    $db = new Database;
    if (!$db->connect())
        exit_with_error('DatabaseConnectionFailure');

    $report = json_decode($post_data, true);

    verify_slave($db, $report);

    $commit_info_list = array_get($report, 'commits', array());
    $should_insert = array_get($report, 'insert', true);

    $commit_modifier = new CommitUpdater($db);
    $commit_modifier->report_commits($commit_info_list, $should_insert);

    exit_with_success();
}

main(file_get_contents('php://input'));

?>
