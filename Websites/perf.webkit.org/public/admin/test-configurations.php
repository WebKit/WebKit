<?php

require('../include/admin-header.php');
require('../include/test-name-resolver.php');

function add_run($metric_id, $platform_id, $type, $date, $mean) {
    global $db;

    $db->begin_transaction();

    $config_id = $db->select_or_insert_row('test_configurations', 'config', array('metric' => $metric_id, 'platform' => $platform_id, 'type' => $type));
    if (!$config_id) {
        $db->rollback_transaction();
        return notice("Failed to add the configuration for metric $metric_id and platform $platform_id");
    }

    $build_id = $db->insert_row('builds', 'build', array('number' => 0, 'time' => $date));
    if (!$build_id) {
        $db->rollback_transaction();
        return notice("Failed to add a build");
    }

    // FIXME: Should we insert run_iterations?
    $run_id = $db->insert_row('test_runs', 'run', array('config' => $config_id, 'build' => $build_id, 'iteration_count_cache' => 1, 'mean_cache' => $mean));
    if (!$run_id) {
        $db->rollback_transaction();
        return notice("Failed to add a run");
    }

    $db->commit_transaction();
    notice("Added a baseline test run.");

    regenerate_manifest();
}

function delete_run($run_id, $build_id) {
    global $db;

    $db->begin_transaction();

    $build_counts = $db->query_and_fetch_all('SELECT COUNT(*) FROM test_runs WHERE run_build = $1', array($build_id));
    if (!$build_counts) {
        $db->rollback_transaction();
        return notice("Failed to obtain the number of runs for the build $build_id");
    }

    if ($build_counts[0]['count'] != 1) {
        $db->rollback_transaction();
        return notice("The build $build_id doesn't have exactly one run. Either the build id is wrong or it's not a synthetic build.");
    }

    $removed_runs = $db->query_and_fetch_all('DELETE FROM test_runs WHERE run_id = $1 RETURNING run_build', array($run_id));
    if (!$removed_runs || count($removed_runs) != 1) {
        $db->rollback_transaction();
        return notice("Failed to delete the run $run_id.");
    }
    $associated_build = $removed_runs[0]['run_build'];
    if ($associated_build != $build_id) {
        $db->rollback_transaction();
        return notice("Failed to delete the run $run_id because it was associated with the build $associated_build instead of the build $build_id");
    }

    $removed_builds = $db->query_and_get_affected_rows('DELETE FROM builds WHERE build_id = $1', array($build_id));
    if (!$removed_runs || count($removed_runs) != 1) {
        $db->rollback_transaction();
        return notice("Failed to delete the build $build_id.");
    }

    $db->commit_transaction();

    regenerate_manifest();
}

if ($db) {
    date_default_timezone_set('Etc/UTC');

    if ($action == 'add-run' && array_get($_POST, 'metric') && array_get($_POST, 'platform')
        && array_get($_POST, 'config-type') && array_get($_POST, 'date') && array_get($_POST, 'mean'))
        add_run(intval($_POST['metric']), intval($_POST['platform']), $_POST['config-type'], $_POST['date'], floatval($_POST['mean']));
    else if ($action == 'delete-run' && array_get($_POST, 'run') && array_get($_POST, 'build'))
        delete_run(intval($_POST['run']), intval($_POST['build']));
    else if ($action)
        notice("Invalid arguments");

    $metric_id = intval(array_get($_GET, 'metric'));

    $test_name_resolver = new TestNameResolver($db);
    $full_metric_name = $test_name_resolver->full_name_for_metric($metric_id);
    echo "<h2>$full_metric_name</h2>";

    $page = new AdministrativePage($db, 'platforms', 'platform', array(
        'name' => array('label' => 'Platform Name'),
        'Configurations' => array('subcolumns'=> array('ID', 'Type'), 'custom' => function ($platform_row) {
            return generate_rows_for_configurations($platform_row['platform_id']);
        }),
        'Baseline Test Runs' => array('subcolumns'=> array('Run ID', 'Build ID', 'Time', 'Mean', 'Actions'), 'custom' => function ($platform_row) {
            return generate_rows_for_test_runs($platform_row['platform_id'], 'baseline');
        }),
        'Target Test Runs' => array('subcolumns'=> array('Run ID', 'Build ID', 'Time', 'Mean', 'Actions'), 'custom' => function ($platform_row) {
            return generate_rows_for_test_runs($platform_row['platform_id'], 'target');
        }),
    ));

    function generate_rows_for_configurations($platform_id) {
        global $test_name_resolver;
        global $metric_id;
        $rows = array();
        if ($configurations = $test_name_resolver->configurations_for_metric_and_platform($metric_id, $platform_id)) {
            foreach ($configurations as $config)
                array_push($rows, array($config['config_id'], $config['config_type']));
        }
        return $rows;
    }

    function generate_rows_for_test_runs($platform_id, $config_type) {
        global $metric_id;
        global $db;

        $baseline_runs = $db->query_and_fetch_all('SELECT * FROM test_runs, test_configurations, builds
            WHERE run_config = config_id AND run_build = build_id
            AND config_metric = $1 AND config_platform = $2 AND config_type = $3
            ORDER BY build_time DESC LIMIT 6', array($metric_id, $platform_id, $config_type));

        $rows = array();
        if ($baseline_runs) {
            foreach ($baseline_runs as $run) {
                $deletion_form = <<< END
<form method="POST">
<input type="hidden" name="run" value="{$run['run_id']}">
<input type="hidden" name="build" value="{$run['run_build']}">
<button type="submit" name="action" value="delete-run">Delete</button>
</form>
END;
                array_push($rows, array($run['run_id'], $run['build_id'], $run['build_time'], $run['run_mean_cache'], $deletion_form));
            }

        }

        $form = <<< END
<form method="POST">
<input type="hidden" name="metric" value="$metric_id">
<input type="hidden" name="platform" value="$platform_id">
<input type="hidden" name="config-type" value="$config_type">
<label>Date: <input type="text" name="date"></label>
<label>Mean: <input type="text" name="mean"></label>
<button type="submit" name="action" value="add-run">Add</button>
</form>
END;

        array_push($rows, $form);

        return $rows;
    }

    $page->render_table('name');
}

require('../include/admin-footer.php');

?>
