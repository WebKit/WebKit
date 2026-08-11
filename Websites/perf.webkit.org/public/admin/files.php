<?php

include('../include/admin-header.php');
include('../include/uploaded-file-helpers.php');

if ($db) {

    if ($action == 'prune-zombie-files') {
        $uploaded_file_rows = $db->select_rows('uploaded_files', 'file', array('deleted_at' => NULL));
        $file_by_id = array();
        foreach ($uploaded_file_rows as &$row)
            $file_by_id[$row['file_id']] = &$row;

        $file_name_list = scandir(config_path('uploadDirectory', ''));
        foreach ($file_name_list as $file_name) {
            if ($file_name == '.' || $file_name == '..')
                continue;
            if (!preg_match('/^(\d+)((\.[A-Za-z0-9]{1,5}){1,2})$/', $file_name, $matches)) {
                echo 'Unknown file: ' . htmlspecialchars($file_name) . '<br>';
                continue;
            }
            $file_name = $matches[0];
            $file_path = config_path('uploadDirectory', $file_name);
            $file_id = $matches[1];
            if (!array_key_exists($file_id, $file_by_id)) {
                echo "Deleting a zombie file: $file_name...";
                if (file_exists($file_path) && !unlink($file_path))
                    echo "Failed";
                else
                    echo "Deleted";
                echo "<br>";
            }
        }
    }

    $files_per_user = $db->query_and_fetch_all('SELECT file_author AS "author", SUM(file_size) AS "usage", COUNT(file_id) AS "count"
        FROM uploaded_files GROUP BY file_author');

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
<form method="POST"><button name="action" value="prune-zombie-files" type="submit">Prune zombie files (slow)</button></form>
END;

}

include('../include/admin-footer.php');

?>
