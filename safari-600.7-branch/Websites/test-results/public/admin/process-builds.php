<?php

require_once('../include/test-results.php');

ignore_user_abort(true); 
set_time_limit(0);

function process_latest_five_builds($db) {
    $build_rows = $db->query_and_fetch_all('SELECT id, builder FROM builds
        WHERE start_time IS NOT NULL AND is_processed = FALSE ORDER BY end_time DESC LIMIT 5');
    if (!$build_rows)
        return FALSE;

    foreach ($build_rows as $row) {
        echo "Build {$row['id']} for builder {$row['builder']}:\n";
        echo "    Updating flakiness...";
        flush();

        $start_time = microtime(true);
        update_flakiness_after_inserting_build($db, $row['id']);
        $time = microtime(true) - $start_time;

        echo "($time s)\n";
        echo "    Generating JSONs...";
        flush();

        $start_time = microtime(true);
        $generator = new ResultsJSONGenerator($db, $row['builder']);
        $generator->generate('wrongexpectations');
        $generator->generate('flaky');
        $time = microtime(true) - $start_time;

        echo "($time s)\n";
        flush();

        $db->query_and_get_affected_rows('UPDATE builds SET is_processed = TRUE where id = $1', array($row['id']));

        sleep(1);
    }

    return TRUE;
}

function main() {
    $db = new Database;
    if (!$db->connect()) {
        echo "Failed to connect to the database";
        exit(1);
    }

    $wait = config('defaultBuildWaitInterval');
    while (1) {
        if (process_latest_five_builds($db))
            $wait = max(1, $wait * 0.8);
        else
            $wait *= 2;
        echo "Sleeping $wait s...\n";
        sleep($wait);
    }
}

main();

?>
