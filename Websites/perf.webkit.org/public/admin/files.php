<?php

include('../include/admin-header.php');
include('../include/uploaded-file-helpers.php');

if ($db) {

    $files_per_user = $db->query_and_fetch_all('SELECT file_author AS "author", SUM(file_size) AS "usage", COUNT(file_id) AS "count"
        FROM uploaded_files WHERE file_deleted_at IS NULL GROUP BY file_author');

    echo <<< END
<table>
    <thead>
        <tr>
            <td>User</td>
            <td>Number of Files</td>
            <td>Disk Usage</td>
        </tr>
    </thead>
    <tbody>
END;

    function format_size($usage, $quota) {
        $megabytes = round($usage / MEGABYTES);
        $percent = round(10000 * $usage / $quota) / 100;
        return "$megabytes MB ($percent%)";
    }

    $quota_per_user = config('uploadUserQuotaInMB');
    $total_disk_usage = 0;
    $total_file_count = 0;

    foreach ($files_per_user as $row) {
        $user_name = $row['author'] ? $row['author'] : 'anonymous';
        $file_count = $row['count'];
        $disk_usage = format_size($row['usage'], $quota_per_user * MEGABYTES);
        echo "    <tr><td>$user_name</td><td>$file_count</td><td>$disk_usage</td></tr>";
        $total_disk_usage += $row['usage'];
        $total_file_count += $file_count;
    }

    $total_disk_usage = format_size($total_disk_usage, config('uploadTotalQuotaInMB') * MEGABYTES);
    echo "    <tr><td>Total</td><td>$total_file_count</td><td>$total_disk_usage</td>";

    echo <<< END
    </tbody>
</table>
<form method="POST"><button name="action" value="cleanup-uploaded-files" type="submit">Cleanup uploaded files</button></form>
END;

    if ($action == 'cleanup-uploaded-files') {
        set_time_limit(300);

        $old_files = prune_files_older_than_months($db);
        $orphaned_disk_files = cleanup_zombie_files($db);
        $orphaned_db_records = cleanup_ghost_records($db);

        $total_files_cleaned = $old_files['deleted_count'] + $orphaned_disk_files['files_deleted'] + $orphaned_db_records['db_records_cleaned'];
        $total_disk_space_freed_mb = $old_files['space_freed_mb'] + $orphaned_disk_files['disk_space_freed_mb'];
        $total_quota_freed_mb = $old_files['space_freed_mb'] + $orphaned_db_records['quota_freed_mb'];
        $total_failures = $old_files['failed_count'] + $orphaned_disk_files['files_failed'];

        echo '<h3>Cleanup Results:</h3>';
        echo '<p>Files cleaned: ' . $total_files_cleaned . '</p>';
        echo '<p>Disk space freed: ' . $total_disk_space_freed_mb . ' MB</p>';
        echo '<p>Quota freed: ' . $total_quota_freed_mb . ' MB</p>';
        if ($old_files['deleted_count'] > 0)
            echo '<p>Old files (&gt;' . config('uploadFileCleanupMonths', 4) . ' months): ' . $old_files['deleted_count'] . ' deleted</p>';
        if ($orphaned_disk_files['files_deleted'] > 0)
            echo '<p>Zombie files: ' . $orphaned_disk_files['files_deleted'] . ' deleted</p>';
        if ($orphaned_db_records['db_records_cleaned'] > 0)
            echo '<p>Ghost records: ' . $orphaned_db_records['db_records_cleaned'] . ' cleaned</p>';
        if ($total_failures > 0)
            echo '<p style="color: red;">Failures: ' . $total_failures . '</p>';
    }

}

include('../include/admin-footer.php');

?>
