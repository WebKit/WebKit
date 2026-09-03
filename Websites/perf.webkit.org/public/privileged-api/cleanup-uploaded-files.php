<?php
require_once('../include/db.php');
require_once('../include/uploaded-file-helpers.php');

function main() {
    set_time_limit(300);

    $db = new Database;
    if (!$db->connect()) {
        header('HTTP/1.1 500 Internal Server Error');
        exit(json_encode(array('error' => 'Database connection failed')));
    }

    $results = array();
    $results['timestamp'] = date('c');
    $results['old_files'] = prune_files_older_than_months($db);
    $results['orphaned_disk_files'] = cleanup_zombie_files($db);
    $results['orphaned_db_records'] = cleanup_ghost_records($db);
    $results['summary'] = array(
        'total_files_cleaned' =>
            $results['old_files']['deleted_count'] +
            $results['orphaned_disk_files']['files_deleted'] +
            $results['orphaned_db_records']['db_records_cleaned'],
        'total_disk_space_freed_mb' =>
            $results['old_files']['space_freed_mb'] +
            $results['orphaned_disk_files']['disk_space_freed_mb'],
        'total_quota_freed_mb' =>
            $results['old_files']['space_freed_mb'] +
            $results['orphaned_db_records']['quota_freed_mb'],
        'total_failures' =>
            $results['old_files']['failed_count'] +
            $results['orphaned_disk_files']['files_failed']
    );

    header('Content-Type: application/json');
    echo json_encode($results, JSON_PRETTY_PRINT);
}

main();
?>
