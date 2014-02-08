<?php

include('../include/admin-header.php');

function merge_platforms($platform_to_merge, $destination_platform) {
    global $db;

    $db->begin_transaction();

    // First, move all test runs to the test configurations in the destination for all test configurations that
    // exist in both the original platform and the platform into which we're merging.
    if (!$db->query_and_get_affected_rows('UPDATE test_runs SET run_config = destination.config_id
        FROM test_configurations as merged, test_configurations as destination
        WHERE merged.config_platform = $1 AND destination.config_platform = $2 AND run_config = merged.config_id
            AND destination.config_metric = merged.config_metric', array($platform_to_merge, $destination_platform))) {
        $db->rollback_transaction();
        return notice("Failed to migrate test runs for $platform_to_merge that have test configurations in $destination_platform.");
    }

    // Then migrate test configurations that don't exist in the destination platform to the new platform
    // so that test runs associated with those configurations are moved to the destination.
    if ($db->query_and_get_affected_rows('UPDATE test_configurations SET config_platform = $2
        WHERE config_platform = $1 AND config_metric NOT IN (SELECT config_metric FROM test_configurations WHERE config_platform = $2)',
        array($platform_to_merge, $destination_platform)) === FALSE) {
        $db->rollback_transaction();
        return notice("Failed to migrate test configurations for $platform_to_merge.");
    }

    if ($db->query_and_fetch_all('SELECT * FROM test_runs, test_configurations WHERE run_config = config_id AND config_platform = $1', array($platform_to_merge))) {
        // We should never reach here.
        $db->rollback_transaction();
        return notice('Failed to migrate all test runs.');
    }

    $db->query_and_get_affected_rows('DELETE FROM platforms WHERE platform_id = $1', array($platform_to_merge));
    $db->commit_transaction();
}

if ($db) {
    if ($action == 'update') {
        if (update_field('platforms', 'platform', 'name')
            || update_field('platforms', 'platform', 'hidden'))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    } else if ($action == 'merge')
        merge_platforms(intval($_POST['id']), intval($_POST['destination']));

    $platforms = $db->fetch_table('platforms', 'platform_name');

    function merge_list($platform_row) {
        global $db;
        global $platforms;

        $id = $platform_row['platform_id'];
        $content = <<< END
<form method="POST"><input type="hidden" name="id" value="$id">
<select name="destination">
END;

        foreach ($platforms as $platform) {
            if ($platform['platform_id'] == $id)
                continue;
            $content .= <<< END
<option value="{$platform['platform_id']}">{$platform['platform_name']}</option>
END;
        }

        $content .= <<< END
</select>
<button type="submit" name="action" value="merge">Merge</button>
</form>
END;
        return array($content);
    }

    $page = new AdministrativePage($db, 'platforms', 'platform', array(
        'name' => array('editing_mode' => 'string'),
        'hidden' => array('editing_mode' => 'boolean'),
        'merge into' => array('custom' => function ($platform_row) { return merge_list($platform_row); }),
    ));

    $page->render_table('name');
}

include('../include/admin-footer.php');

?>
