<?php

require_once('../include/admin-header.php');
require_once('../include/test-name-resolver.php');

if ($action == 'dashboard') {
    if (array_key_exists('metric_id', $_POST)) {
        $metric_id = intval($_POST['metric_id']);
        $config_ids = array();
        $succeeded = TRUE;
        if (!$db->query_and_get_affected_rows("UPDATE test_configurations SET config_is_in_dashboard = false WHERE config_metric = $1", array($metric_id))) {
            $succeeded = FALSE;
            notice("Failed to remove some configurations from the dashboard.");
        }

        else if (array_key_exists('metric_platforms', $_POST)) {
            foreach ($_POST['metric_platforms'] as $platform_id) {
                if (!$db->query_and_get_affected_rows("UPDATE test_configurations SET config_is_in_dashboard = true
                    WHERE config_metric = $1 AND config_platform = $2", array($metric_id, $platform_id))) {
                    $succeeded = FALSE;
                    notice("Failed to add configurations for metric $metric_id and platform $platform_id to the dashboard.");
                }
            }
        }

        if ($succeeded) {
            notice("Updated the dashboard.");
            regenerate_manifest();
        }
    } else
        notice('Invalid parameters');
} else if ($action == 'update') {
    if (!update_field('tests', 'test', 'url'))
        notice('Invalid parameters');
} else if ($action == 'add') {
    if (array_key_exists('test_id', $_POST) && array_key_exists('metric_name', $_POST)) {
        $id = intval($_POST['test_id']);
        $aggregator = intval($_POST['metric_aggregator']);
        if (!$aggregator)
            notice('Invalid aggregator. You must specify one.');
        else {
            $metric_id = $db->insert_row('test_metrics', 'metric',
                array('test' => $id, 'name' => $_POST['metric_name'], 'aggregator' => $aggregator));
            if (!$metric_id)
                notice("Could not insert the new metric for test $id");
            else {
                add_job('aggregate', '{"metricIds": [ ' . $metric_id . ']}');
                notice("Inserted the metric for test $id");
            }
        }
    } else if (array_key_exists('metric_id', $_POST))
        regenerate_manifest();
    else
        notice('Invalid parameters');
}

if ($db) {
    $aggregators = array();
    if ($aggregators_table = $db->fetch_table('aggregators')) {
        foreach ($aggregators_table as $aggregator_row)
            $aggregators[$aggregator_row['aggregator_id']] = $aggregator_row['aggregator_name'];
    }

    $test_name_resolver = new TestNameResolver($db);
    if ($test_name_resolver->tests()) {
        $name_to_platform = array();

        foreach ($db->fetch_table('platforms') as $platform)
            $name_to_platform[$platform['platform_name']] = $platform;

        $platform_names = array_keys($name_to_platform);
        asort($platform_names);

        $odd = false;
        $selected_parent_full_name = trim(array_get($_SERVER, 'PATH_INFO', ''), '/');
        $selected_parent = $test_name_resolver->test_id_for_full_name($selected_parent_full_name);
        if ($selected_parent)
            echo '<h2>' . htmlspecialchars($selected_parent_full_name) . '</h2>';

?>
<table>
<thead>
    <tr><td>Test ID</td><td>Full Name</td><td>Parent ID</td><td>URL</td>
        <td>Metric ID</td><td>Metric Name</td><td>Aggregator</td><td>Dashboard</td>
</thead>
<tbody>
<?php

        foreach ($test_name_resolver->tests() as $test) {
            if ($test['test_parent'] != $selected_parent['test_id'])
                continue;

            $test_id = $test['test_id'];
            $test_metrics = $test_name_resolver->metrics_for_test_id($test_id);
            $row_count = count($test_metrics);
            $child_metrics = $test_name_resolver->child_metrics_for_test_id($test_id);
            $linked_test_name = htmlspecialchars($test['full_name']);
            if ($child_metrics) {
                $row_count++;
                $linked_test_name = '<a href="/admin/tests/' . htmlspecialchars($test['full_name']) . '">' . $linked_test_name . '</a>';
            }

            $tbody_class = $odd ? 'odd' : 'even';
            $odd = !$odd;

            $test_url = htmlspecialchars($test['test_url']);

            echo <<<EOF
    <tbody class="$tbody_class">
    <tr>
        <td rowspan="$row_count">$test_id</td>
        <td rowspan="$row_count">$linked_test_name</td>
        <td rowspan="$row_count">{$test['test_parent']}</td>
        <td rowspan="$row_count">
        <form method="POST"><input type="hidden" name="id" value="$test_id">
        <input type="hidden" name="action" value="update">
        <input type="url" name="url" value="$test_url" size="80"></form></td>
EOF;

            if ($test_metrics) {
                $has_tr = true;
                foreach ($test_metrics as $metric) {
                    $aggregator_name = array_get($aggregators, $metric['metric_aggregator'], '');
                    if ($aggregator_name) {
                        $aggregator_action = '<form method="POST"><input type="hidden" name="metric_id" value="'. $metric['metric_id']
                            . '"><button type="submit" name="action" value="add">Regenerate</button></form>';
                    } else
                        $aggregator_action = '';

                    if (!$has_tr)
                        echo <<<EOF

    <tr>
EOF;
                    $has_tr = false;

                    $metric_id = $metric['metric_id'];
                    echo <<<EOF
        <td><a href="/admin/test-configurations?metric=$metric_id">$metric_id</a></td>
        <td>{$metric['metric_name']}</td>
        <td>$aggregator_name $aggregator_action</td>
        <td><form method="POST"><input type="hidden" name="metric_id" value="$metric_id">
EOF;

                    foreach ($platform_names as $platform_name) {
                        $platform_name = htmlspecialchars($platform_name);
                        $platform = $name_to_platform[$platform_name];
                        $configurations = $test_name_resolver->configurations_for_metric_and_platform($metric_id, $platform['platform_id']);
                        if (!$configurations)
                            continue;
                        echo "<label><input type=\"checkbox\" name=\"metric_platforms[]\" value=\"{$platform['platform_id']}\"";
                        if ($db->is_true($configurations[0]['config_is_in_dashboard']))
                            echo ' checked';
                        else if ($db->is_true($platform['platform_hidden']))
                            echo 'disabled';
                        echo ">$platform_name</label>";
                    }

                    echo <<<EOF
        <button type="submit" name="action" value="dashboard">Save</button></form>
        </td>
    </tr>
EOF;
                }
            }

            if ($child_metrics) {
                echo <<<EOF
        <td colspan="5"><form method="POST">
        <input type="hidden" name="test_id" value="$test_id">
        <label>Name<select name="metric_name">
EOF;

                foreach ($child_metrics as $metric_name) {
                    $metric_name = htmlspecialchars($metric_name);
                    echo "
            <option>$metric_name</option>";
                }

                echo <<<EOF
        </select></label>
        <label>Aggregator
        <select name="metric_aggregator">
            <option value="">-</option>
EOF;
            foreach ($aggregators as $id => $name) {
                $name = htmlspecialchars($name);
                echo "
            <option value=\"$id\">$name</option>";
            }
            echo <<<EOF
        </select></label>
        <button type="submit" name="action" value="add">Add</button></form></td>
    </tr>
EOF;
            }
            echo <<<EOF
    </tbody>
EOF;
        }

        ?></tbody>
</table>

<?php

    }

}

include('../include/admin-footer.php');

?>
