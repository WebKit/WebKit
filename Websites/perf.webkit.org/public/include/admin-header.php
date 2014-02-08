<?php

require_once('db.php');
require_once('manifest.php');

?><!DOCTYPE html>
<html>
<head>
<title>WebKit Perf Monitor</title>
<link rel="stylesheet" href="/common.css">
<link rel="stylesheet" href="/admin/admin.css">
</head>
<body>
<header id="title">
<h1><a href="/">WebKit Perf Monitor</a></h1>
<ul>
    <li><a href="/admin/platforms">Platforms</a></li>
    <li><a href="/admin/tests">Tests</a></li>
    <li><a href="/admin/jobs">Jobs</a></li>
    <li><a href="/admin/aggregators">Aggregators</a></li>
    <li><a href="/admin/builders">Builders</a></li>
    <li><a href="/admin/repositories">Repositories</a></li>
    <li><a href="/admin/bug-trackers">Bug Trackers</a></li>
</ul>
</header>

<div id="mainContents">
<?php

function notice($message) {
    echo "<p class='notice'>$message</p>";
}

$db = new Database;
if (!$db->connect()) {
    notice('Failed to connect to the database');
    $db = NULL;
} else
    $action = array_key_exists('action', $_POST) ? $_POST['action'] : NULL;

function execute_query_and_expect_one_row_to_be_affected($query, $params, $success_message, $failure_message) {
    global $db;

    foreach ($params as &$param) {
        if ($param == '')
            $param = NULL;
    }

    $affected_rows = $db->query_and_get_affected_rows($query, $params);
    if ($affected_rows) {
        assert('$affected_rows == 1');
        notice($success_message);
        return true;
    }

    notice($failure_message);
    return false;
}

function update_field($table, $prefix, $field_name) {
    global $db;

    if (!array_key_exists('id', $_POST) || (array_get($_POST, 'updated-column') != $field_name && !array_key_exists($field_name, $_POST)))
        return FALSE;

    $id = intval($_POST['id']);
    $prefixed_field_name = $prefix . '_' . $field_name;
    $id_field_name = $prefix . '_id';

    execute_query_and_expect_one_row_to_be_affected("UPDATE $table SET $prefixed_field_name = \$2 WHERE $id_field_name = \$1",
        array($id, array_get($_POST, $field_name)),
        "Updated the $prefix $id",
        "Could not update $prefix $id");

    return TRUE;
}

function regenerate_manifest() {
    global $db;

    $generator = new ManifestGenerator($db);
    if (!$generator->generate()) {
        notice("Failed to generate the manifest (before trying to write into the filesystem).");
        return FALSE;
    }

    if (!$generator->store()) {
        notice("Failed to save the generated manifest into the filesystem");
        return FALSE;
    }

    return TRUE;
}

function add_job($type, $payload = null) {
    global $db;

    if ($db->insert_row('jobs', 'job', array('type' => $type, 'payload' => $payload)))
        notice("Added a job of type $type");
    else
        notice("Failed to add job of type $type");
}

class AdministrativePage {
    private $table;
    private $prefix;
    private $column_to_be_ordered_by;
    private $column_info;

    function __construct($db, $table, $prefix, $column_info) {
        $this->db = $db;
        $this->table = $table;
        $this->prefix = $prefix;
        $this->column_info = $column_info;
    }

    private function name_to_titlecase($name) {
        return ucwords(str_replace('_', ' ', $name));
    }

    private function column_label($name) {
        return array_get($this->column_info[$name], 'label', $this->name_to_titlecase($name));
    }

    private function render_form_control_for_column($editing_mode, $name, $value = '', $show_update_button_if_needed = FALSE) {
        $show_update_button = FALSE;
        switch ($editing_mode) {
        case 'text':
            echo <<< END
<textarea name="$name" rows="7" cols="50">$value</textarea><br>
END;
            $show_update_button = $show_update_button_if_needed;
            break;
        case 'boolean':
            $checkedness = $this->db->is_true($value) ? ' checked' : '';
            echo <<< END
<input type="checkbox" name="$name"$checkedness>
END;
            $show_update_button = $show_update_button_if_needed;
            break;
        case 'url':
            echo <<< END
<input type="text" name="$name" value="$value" size="70">
END;
            break;
        default:
            assert($editing_mode == 'string');
            echo <<< END
<input type="text" name="$name" value="$value">
END;
        }

        if ($show_update_button) {
            echo <<< END

<button type="submit" name="action" value="update">Update</button>
END;
        }
    }

    function render_table($column_to_be_ordered_by) {
        $column_names = array_keys($this->column_info);
        $column_to_subcolumn_names = array();
        foreach ($column_names as $name) {
            if (array_get($this->column_info[$name], 'pre_insertion'))
                continue;
            $subcolumns = array_get($this->column_info[$name], 'subcolumns', array());
            if (!$subcolumns || !array_get($this->column_info[$name], 'custom'))
                continue;
            $column_to_subcolumn_names[$name] = $subcolumns;
        }

        $rowspan_if_needed = $column_to_subcolumn_names ? ' rowspan="2"' : '';

        $headers = "<tr><td$rowspan_if_needed>ID</td>";
        foreach ($column_names as $name) {
            if (array_get($this->column_info[$name], 'pre_insertion'))
                continue;
            $label = htmlspecialchars($this->column_label($name));
            if (array_get($column_to_subcolumn_names, $name)) {
                $count = count($column_to_subcolumn_names[$name]);
                $headers .= "<td colspan=\"$count\">$label</td>";
            } else
                $headers .= "<td$rowspan_if_needed>$label</td>";
        }
        $headers .= "</tr>\n";

        if ($column_to_subcolumn_names) {
            $headers .= '<tr>';
            foreach ($column_names as $name) {
                $subcolumn_names = array_get($column_to_subcolumn_names, $name);
                if (!$subcolumn_names)
                    continue;
                foreach ($subcolumn_names as $label)
                    $headers .= '<td>' . htmlspecialchars($label) . '</td>';
            }
            $headers .= "</tr>\n";
        }

        echo <<< END
<table>
<thead>
$headers
</thead>
<tbody>

END;

        assert(ctype_alnum_underscore($column_to_be_ordered_by));
        $rows = $this->db->fetch_table($this->table, $this->prefix . '_' . $column_to_be_ordered_by);
        if ($rows) {
            foreach ($rows as $row) {
                $id = intval($row[$this->prefix . '_id']);

                $custom_cells_list = array();
                $maximum_rows = 1;
                foreach ($column_names as $name) {
                    if (array_get($this->column_info[$name], 'pre_insertion'))
                        continue;

                    if ($custom = array_get($this->column_info[$name], 'custom')) {
                        $custom_cells_list[$name] = $custom($row);
                        $maximum_rows = max($maximum_rows, count($custom_cells_list[$name]));
                    }
                }

                $rowspan_if_needed = $maximum_rows > 1 ? ' rowspan="' . $maximum_rows . '"' : '';

                echo "<tr>\n<td$rowspan_if_needed>$id</td>\n";
                foreach ($column_names as $name) {
                    if (array_get($this->column_info[$name], 'pre_insertion'))
                        continue;

                    if (array_key_exists($name, $custom_cells_list)) {
                        $this->render_custom_cells(array_get($column_to_subcolumn_names, $name), $custom_cells_list[$name], 0);
                        continue;
                    }

                    $value = htmlspecialchars($row[$this->prefix . '_' . $name], ENT_QUOTES);
                    $editing_mode = array_get($this->column_info[$name], 'editing_mode');
                    if (!$editing_mode) {
                        echo "<td$rowspan_if_needed>$value</td>\n";
                        continue;
                    }

                    echo <<< END
<td$rowspan_if_needed>
<form method="POST">
<input type="hidden" name="id" value="$id">
<input type="hidden" name="action" value="update">
<input type="hidden" name="updated-column" value="$name">

END;
                    $this->render_form_control_for_column($editing_mode, $name, $value, TRUE);
                    echo "</form></td>\n";

                }
                echo "</tr>\n";

                for ($row = 1; $row < $maximum_rows; $row++) {
                    echo "<tr>\n";
                    foreach ($column_names as $name) {
                        if (array_key_exists($name, $custom_cells_list))
                            $this->render_custom_cells(array_get($column_to_subcolumn_names, $name), $custom_cells_list[$name], $row);
                    }
                    echo "</tr>\n";
                }
            }
        }
        echo <<< END
</tbody>
</table>
END;
    }

    function render_custom_cells($subcolum_names, $rows, $row_index) {
        $cells = array_get($rows, $row_index, array());
        if (!is_array($cells))
            $cells = array($cells);

        if ($subcolum_names && count($cells) <= 1) {
            $colspan = count($subcolum_names);
            $content = $cells ? $cells[0] : '';
            echo "<td colspan=\"$colspan\">$content</td>\n";
            return;
        }

        for ($i = 0; $i < ($subcolum_names ? count($subcolum_names) : 1); $i++)
            echo '<td>' . array_get($cells, $i, '') . '</td>';
        echo "\n";
    }

    function render_form_to_add($title = NULL) {

        if (!$title) # Can't use the table name since it needs to be singular.
            $title = 'New ' . $this->name_to_titlecase($this->prefix);

echo <<< END
<section class="action-field">
<h2>$title</h2>
<form method="POST">

END;
        foreach (array_keys($this->column_info) as $name) {
            $editing_mode = array_get($this->column_info[$name], 'editing_mode');
            if (array_get($this->column_info[$name], 'custom') || !$editing_mode)
                continue;

            $label = htmlspecialchars($this->column_label($name));
            echo "<label>$label<br>\n";
            $this->render_form_control_for_column($editing_mode, $name);
            echo "</label><br>\n";
        }

echo <<< END

<button type="submit" name="action" value="add">Add</button>
</form>
</section>
END;

    }

}

?>
