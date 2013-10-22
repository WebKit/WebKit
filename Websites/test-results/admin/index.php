<?php

include('../include/admin-header.php');
include('../include/test-results.php');

notice("FIXME: This page is broken!");

define('MAX_FETCH_COUNT', 10);

function fetch_builders($db, $master) {
    flush();
    $builders_json = fetch_and_parse_test_results_json("http://$master/json/builders/");
    if (!$builders_json)
        return notice("Failed to fetch or decode /json/builders from $master");

    echo "<h2>Fetching builds from $master</h2>\n";
    echo "<ul>\n";

    foreach ($builders_json as $builder_name => $builder_data) {
        if (!stristr($builder_name, 'Test') || stristr($builder_name, 'Apple Win'))
            continue;
        $builder_id = $db->select_or_insert_row('builders', NULL, array('master' => $master, 'name' => $builder_name));
        $escaped_builder_name = htmlspecialchars($builder_name);
        echo "<li><em>$escaped_builder_name</em> (id: $builder_id) - ";

        $fetchCount = 0;
        $builds = $builder_data['cachedBuilds'];
        foreach (array_reverse($builds) as $build_number) {
            $build_number = intval($build_number);
            $build = $db->select_or_insert_row('builds', NULL, array('builder' => $builder_id, 'number' => $build_number), NULL, '*');
            if ($db->is_true($build['fetched']))
                $class = 'fetched';
            else if ($fetchCount >= MAX_FETCH_COUNT)
                $class = 'unfetched';
            else {
                $class = fetch_build($db, $master, $builder_name, $build['id'], $build_number) ? 'fetched' : 'unfetched';
                $fetchCount++;
            }
            echo "<span class=\"$class\">$build_number</a> ";
        }
        echo "</li>\n";
    }
    echo "<ul>\n";

    return TRUE;
}

function urlencode_without_plus($url) {
    return str_replace('+', '%20', urlencode($url));
}

function fetch_build($db, $master, $builder_name, $build_id, $build_number) {
    flush();
    $builder_name = urlencode_without_plus($builder_name);
    $build_json = fetch_and_parse_test_results_json("http://$master/json/builders/$builder_name/builds/$build_number");
    if (!$build_json || !array_key_exists('times', $build_json) || count($build_json['times']) != 2)
        return FALSE;

    $revision = NULL;
    $slavename = NULL;
    foreach ($build_json['properties'] as $property) {
        if ($property[0] == 'got_revision')
            $revision = $property[1];
        if ($property[0] == 'slavename')
            $slavename = $property[2];
    }
    if (!$revision || !$slavename)
        return FALSE;

    $start_time = float_to_time($build_json['times'][0]);
    $end_time = float_to_time($build_json['times'][1]);

    flush();
    $full_results = fetch_and_parse_test_results_json("http://$master/results/$builder_name/r$revision%20($build_number)/full_results.json", TRUE);
    return store_test_results($db, $full_results, $build_id, $revision, $start_time, $end_time, $slavename);
}

$db = new Database;
if (!$db->connect())
    notice('Failed to connect to the database');
else if (array_key_exists('master', $_GET)) {
    $master = htmlspecialchars($_GET['master']); // Okay since hostname shoudln't contain any HTML special characters.
    if (in_array($master, config('masters')))
        fetch_builders($db, $master);
    else
        notice("The master $master not found");
} else {
    echo "<ul>\n";
    foreach (config('masters') as $master)
        echo "<li><a href=\"?master=$master\">$master</a></li>\n";
    echo "</ul>\n";
}

?></div>

</body>
</html>
