<?php

function ends_with($str, $key) {
    return strrpos($str, $key) == strlen($str) - strlen($key);
}

function ctype_alnum_underscore($str) {
    return ctype_alnum(str_replace('_', '', $str));
}

function &array_ensure_item_has_array(&$array, $key) {
    if (!array_key_exists($key, $array))
        $array[$key] = array();
    return $array[$key];
}

function array_get($array, $key, $default = NULL) {
    if (!array_key_exists($key, $array))
        return $default;
    return $array[$key];
}

function array_set_default(&$array, $key, $default) {
    if (!array_key_exists($key, $array))
        $array[$key] = $default;
}

$_config = NULL;

function config($key) {
    global $_config;
    if (!$_config)
        $_config = json_decode(file_get_contents(dirname(__FILE__) . '/../../config.json'), true);
    return $_config[$key];
}

if (config('debug')) {
    error_reporting(E_ALL | E_STRICT);
    ini_set('display_errors', 'On');
} else
    error_reporting(E_ERROR);

class Database
{
    private $connection = false;

    function __destruct() {
        if ($this->connection)
            pg_close($this->connection);
        $this->connection = false;
    }

    function is_true($value) {
        return $value == 't';
    }

    function connect() {
        $databaseConfig = config('database');
        $this->connection = pg_connect('host=' . $databaseConfig['host'] . ' port=' . $databaseConfig['port']
            . ' dbname=' . $databaseConfig['name'] . ' user=' . $databaseConfig['username'] . ' password=' . $databaseConfig['password']);
        return $this->connection ? true : false;
    }

    private function prefixed_column_names($columns, $prefix = NULL) {
        if (!$prefix)
            return join(', ', $columns);
        return $prefix . '_' . join(', ' . $prefix . '_', $columns);
    }

    private function prefixed_name($column, $prefix = NULL) {
        return $prefix ? $prefix . '_' . $column : $column;
    }

    private function prepare_params($params, &$placeholders, &$values) {
        $column_names = array_keys($params);

        $i = count($values) + 1;
        foreach ($column_names as $name) {
            assert(ctype_alnum_underscore($name));
            array_push($placeholders, '$' . $i);
            array_push($values, $params[$name]);
            $i++;
        }

        return $column_names;
    }

    function insert_row($table, $prefix, $params, $returning = 'id') {
        $placeholders = array();
        $values = array();
        $column_names = $this->prepare_params($params, $placeholders, $values);

        assert(!$prefix || ctype_alnum_underscore($prefix));
        $column_names = $this->prefixed_column_names($column_names, $prefix);
        $placeholders = join(', ', $placeholders);

        if ($returning) {
            $returning_column_name = $this->prefixed_name($returning, $prefix);
            $rows = $this->query_and_fetch_all("INSERT INTO $table ($column_names) VALUES ($placeholders) RETURNING $returning_column_name", $values);
            return $rows ? $rows[0][$returning_column_name] : NULL;
        }

        return $this->query_and_get_affected_rows("INSERT INTO $table ($column_names) VALUES ($placeholders)", $values) == 1;
    }

    function select_or_insert_row($table, $prefix, $select_params, $insert_params = NULL, $returning = 'id') {
        $values = array();

        $select_placeholders = array();
        $select_column_names = $this->prepare_params($select_params, $select_placeholders, $values);
        $select_values = array_slice($values, 0);

        if ($insert_params === NULL)
            $insert_params = $select_params;
        $insert_placeholders = array();
        $insert_column_names = $this->prepare_params($insert_params, $insert_placeholders, $values);

        assert(!!$returning);
        assert(!$prefix || ctype_alnum_underscore($prefix));
        $returning_column_name = $returning == '*' ? '*' : $this->prefixed_name($returning, $prefix);
        $select_column_names = $this->prefixed_column_names($select_column_names, $prefix);
        $select_placeholders = join(', ', $select_placeholders);
        $query = "SELECT $returning_column_name FROM $table WHERE ($select_column_names) = ($select_placeholders)";

        $insert_column_names = $this->prefixed_column_names($insert_column_names, $prefix);
        $insert_placeholders = join(', ', $insert_placeholders);
        $rows = $this->query_and_fetch_all("INSERT INTO $table ($insert_column_names) SELECT $insert_placeholders WHERE NOT EXISTS
            ($query) RETURNING $returning_column_name", $values);
        if (!$rows)
            $rows = $this->query_and_fetch_all($query, $select_values);

        return $rows ? ($returning == '*' ? $rows[0] : $rows[0][$returning_column_name]) : NULL;
    }

    function select_first_row($table, $prefix, $params, $order_by = NULL) {
        $placeholders = array();
        $values = array();
        $column_names = $this->prefixed_column_names($this->prepare_params($params, $placeholders, $values), $prefix);
        $placeholders = join(', ', $placeholders);
        $query = "SELECT * FROM $table WHERE ($column_names) = ($placeholders)";
        if ($order_by) {
            assert(!ctype_alnum_underscore($order_by));
            $query .= ' ORDER BY ' . $this->prefixed_name($order_by, $prefix);
        }
        $rows = $this->query_and_fetch_all($query . ' LIMIT 1', $values);

        return $rows ? $rows[0] : NULL;
    }

    function query_and_get_affected_rows($query, $params = array()) {
        if (!$this->connection)
            return FALSE;
        $result = pg_query_params($this->connection, $query, $params);
        if (!$result)
            return FALSE;
        return pg_affected_rows($result);
    }

    function query_and_fetch_all($query, $params = array()) {
        if (!$this->connection)
            return false;
        $result = pg_query_params($this->connection, $query, $params);
        if (!$result)
            return false;
        return pg_fetch_all($result);
    }

    function query($query, $params = array()) {
        if (!$this->connection)
            return FALSE;
        return pg_query_params($this->connection, $query, $params);
    }

    function fetch_next_row($result) {
        return pg_fetch_assoc($result);
    }

    function fetch_table($table_name, $column_to_be_ordered_by = null) {
        if (!$this->connection || !ctype_alnum_underscore($table_name) || ($column_to_be_ordered_by && !ctype_alnum_underscore($column_to_be_ordered_by)))
            return false;
        $clauses = '';
        if ($column_to_be_ordered_by)
            $clauses .= 'ORDER BY ' . $column_to_be_ordered_by;
        return $this->query_and_fetch_all("SELECT * FROM $table_name $clauses");
    }

    function begin_transaction() {
        return $this->connection and pg_query($this->connection, "BEGIN");
    }

    function commit_transaction() {
        return $this->connection and pg_query($this->connection, 'COMMIT');
    }

    function rollback_transaction() {
        return $this->connection and pg_query($this->connection, 'ROLLBACK');
    }

}

?>
