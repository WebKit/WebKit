<?php

require_once('db.php');

?><!DOCTYPE html>
<html>
<head>
<title>WebKit Test Results</title>
<link rel="stylesheet" href="/common.css">
<link rel="stylesheet" href="/admin/admin.css">
</head>
<body>
<header id="title">
<h1><a href="/">WebKit Perf Monitor</a></h1>
<ul>
    <li><a href="/admin/">Admin</a></li>
    <li><a href="/admin/builders">Builders</a></li>
    <li><a href="/admin/repositories">Repositories</a></li>
</ul>
</header>

<div id="mainContents">
<?php

function regenerate_manifest() {
    // manifest.php doesn't need to be regenerated but WebKit Perf Monitor generates a static manifest.json.
}

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

    if (!array_key_exists('id', $_POST) || !array_key_exists($field_name, $_POST))
        return FALSE;

    $id = intval($_POST['id']);
    $prefixed_field_name = $prefix . $field_name;
    $id_field_name = $prefix ? $prefix . '_id' : 'id';

    execute_query_and_expect_one_row_to_be_affected("UPDATE $table SET $prefixed_field_name = \$2 WHERE $id_field_name = \$1",
        array($id, $_POST[$field_name]),
        "Updated the $prefix $id",
        "Could not update $prefix $id");

    return TRUE;
}

class AdministrativePage {
    private $table;
    private $prefix;
    private $column_to_be_ordered_by;
    private $column_info;

    function __construct($db, $table, $prefix, $column_info) {
        $this->db = $db;
        $this->table = $table;
        $this->prefix = $prefix ? $prefix . '_' : '';
        $this->column_info = $column_info;
    }

    private function name_to_titlecase($name) {
        return ucwords(str_replace('_', ' ', $name));
    }

    private function column_label($name) {
        return array_get($this->column_info[$name], 'label', $this->name_to_titlecase($name));
    }

    private function render_form_control_for_column($editing_mode, $name, $value = '', $show_update_button_if_needed = FALSE, $size = NULL) {
        if ($editing_mode == 'text') {
            echo <<< END
<textarea name="$name" rows="7" cols="50">$value</textarea><br>
END;
            if ($show_update_button_if_needed) {
                echo <<< END

<button type="submit" name="action" value="update">Update</button>
END;
            }
            return;
        }

        if ($editing_mode == 'url') {
            if (!$size)
                $size = 70;
            echo <<< END
<input type="text" name="$name" value="$value" size="$size">
END;
            return;
        }

        $sizeIfExits = $size ? " size=\"$size\"" : '';
        echo <<< END
<input type="text" name="$name" value="$value"$sizeIfExits>
END;
    }

    function render_table($column_to_be_ordered_by) {
        $column_names = array_keys($this->column_info);
        $labels = array();
        foreach ($column_names as $name) {
            if (array_get($this->column_info[$name], 'pre_insertion'))
                continue;
            array_push($labels, htmlspecialchars($this->column_label($name)));
        }


        $headers = join('</td><td>', $labels);
        echo <<< END
<table>
<thead><tr><td>ID</td><td>$headers</td></tr></thead>
<tbody>

END;

        assert(ctype_alnum_underscore($column_to_be_ordered_by));
        $rows = $this->db->fetch_table($this->table, $this->prefix . $column_to_be_ordered_by);
        if ($rows) {
            foreach ($rows as $row) {
                $id = intval($row[$this->prefix . 'id']);
                echo "<tr>\n<td>$id</td>\n";
                foreach ($column_names as $name) {
                    if (array_get($this->column_info[$name], 'pre_insertion'))
                        continue;

                    $custom = array_get($this->column_info[$name], 'custom');
                    if ($custom) {
                        echo "<td>";
                        $custom($row);
                        echo "</td>\n";
                        continue;
                    }

                    $value = htmlspecialchars($row[$this->prefix . $name], ENT_QUOTES);
                    $editing_mode = array_get($this->column_info[$name], 'editing_mode');
                    if (!$editing_mode) {
                        echo "<td>$value</td>\n";
                        continue;
                    }

                    echo <<< END
<td>
<form method="POST">
<input type="hidden" name="id" value="$id">
<input type="hidden" name="action" value="update">

END;
                    $size = array_get($this->column_info[$name], 'size');
                    $this->render_form_control_for_column($editing_mode, $name, $value, TRUE, $size);
                    echo "</form></td>\n";

                }
                echo "</tr>\n";
            }
        }
        echo <<< END
</tbody>
</table>
END;
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
