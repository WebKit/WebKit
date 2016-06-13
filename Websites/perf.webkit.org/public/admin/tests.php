<?php

require_once('../include/admin-header.php');
require_once('../include/test-name-resolver.php');

if ($action == 'update') {
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
            else
                notice("Inserted the metric for test $id. Aggregation for $metric_id is needed.");
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
    $triggerables = array();
    if ($triggerable_table = $db->fetch_table('build_triggerables', 'triggerable_name')) {
        foreach ($triggerable_table as $triggerable_row)
            $triggerables[$triggerable_row['triggerable_id']] = $triggerable_row;
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
    <tr><td>Test ID</td><td>Full Name</td><td>Parent ID</td><td>URL</td><td>Triggerables</td>
        <td>Metric ID</td><td>Metric Name</td><td>Aggregator</td>
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
            if ($child_metrics)
                $linked_test_name = '<a href="/admin/tests/' . htmlspecialchars($test['full_name']) . '">' . $linked_test_name . '</a>';

            $tbody_class = $odd ? 'odd' : 'even';
            $odd = !$odd;

            $test_url = htmlspecialchars($test['test_url']);

            $triggerable_platform_list = '';

            $triggerable_platforms = $db->query_and_fetch_all('SELECT * FROM platforms JOIN triggerable_configurations
                ON trigconfig_platform = platform_id AND trigconfig_test = $1 ORDER BY trigconfig_triggerable', array($test_id));
            $previous_triggerable = null;
            foreach ($triggerable_platforms as $row) {
                if ($previous_triggerable != $row['trigconfig_triggerable']) {
                    $previous_triggerable = $row['trigconfig_triggerable'];
                    if ($triggerable_platform_list)
                        $triggerable_platform_list .= '<br>';
                    $name = $triggerables[$previous_triggerable]['triggerable_name'];
                    $triggerable_platform_list .= "<strong>$name:</strong> ";
                } else
                    $triggerable_platform_list .= ', ';
                $triggerable_platform_list .= $row['platform_name'];
            }

            echo <<<EOF
    <tbody class="$tbody_class">
    <tr>
        <td rowspan="$row_count">$test_id</td>
        <td rowspan="$row_count">$linked_test_name</td>
        <td rowspan="$row_count">{$test['test_parent']}</td>
        <td rowspan="$row_count">
        <form method="POST"><input type="hidden" name="id" value="$test_id">
        <input type="hidden" name="action" value="update">
        <input type="url" name="url" value="$test_url" size="30"></form></td>
        <td rowspan="$row_count">$triggerable_platform_list</td>
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
    </tr>
EOF;
                }
            } else
                echo '<td colspan="3"></td>';

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
