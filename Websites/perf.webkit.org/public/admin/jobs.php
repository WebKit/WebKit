<?php

require('../include/admin-header.php');

if ($action == 'delete') {
    if (array_key_exists('id', $_POST)) {
        $job_id = intval($_POST['id']);
        execute_query_and_expect_one_row_to_be_affected('DELETE FROM jobs WHERE job_id = $1', array($job_id),
            "Deleted job $job_id", "Could not delete job $job_id");
    } else
        notice('Invalid ID.');
} else if ($action == 'manifest') {
    execute_query_and_expect_one_row_to_be_affected('INSERT INTO jobs (job_type) VALUES (\'manifest\')', array(),
        'Requested to regenerate the manifest.', 'Could not add a job');
}

if ($db) {

?>
<table>
<thead>
    <tr><td>ID</td><td>Type</td><td>Created At</td><td>Started At</td><td>PID</td><td>Attempts</td><td>Payload</td><td>Log</td><td>Actions</td>
</thead>
<tbody>
<?php

    function get_value_with_default($array, $key, $default) {
        $value = $array[$key];
        if (!$value)
            $value = $default;
        return $value;
    }

    # FIXME: Add a navigation bar for when there are more than 50 jobs.
    $uncompleted_jobs = $db->query_and_fetch_all('SELECT * FROM jobs WHERE job_completed_at IS NULL LIMIT 50');
    if ($uncompleted_jobs) {
        foreach ($uncompleted_jobs as $job) {
            $id = $job['job_id'];
            $started_at = get_value_with_default($job, 'job_started_at', '');
            $pid = get_value_with_default($job, 'job_started_by_pid', '');
            echo <<< EOF
    <tr>
        <td>$id</td>
        <td>{$job['job_type']}</td>
        <td>{$job['job_created_at']}</td>
        <td>$started_at</td><td>$pid</td>
        <td>{$job['job_attempts']}</td>
        <td><pre class="payload">{$job['job_payload']}</pre></td>
        <td><pre>{$job['job_log']}</pre></td>
        <td><form method='POST'><button type='submit' name='action' value='delete'>Delete</button><input type='hidden' name='id' value='$id'></form></td>
    </tr>
EOF;
        }
    }

?></tbody>
</table>

<script>

var payloadPres = document.querySelectorAll('.payload');
for (var i = 0; i < payloadPres.length; ++i) {
    var pre = payloadPres[i];
    try {
        pre.textContent = JSON.stringify(JSON.parse(pre.textContent), null, '  ');
    } catch (exception) { } // Ignore exceptions.
}

</script>

<section class="action-field">
<h2>New job</h2>
<form method='POST'><button type='submit' name='action' value='manifest'>Re-generate manifest</button></form>
</section>

<?php

}

include('../include/admin-footer.php');

?>
