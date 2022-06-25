<?php

class TestPathResolver {
    function __construct($db) {
        $this->db = $db;
        $this->id_to_test_map = NULL;
    }

    function ancestors_for_test($test_id) {
        $id_to_test = $this->ensure_id_to_test_map();
        $ancestors = array();
        for (; $test_id; $test_id = $id_to_test[$test_id]['test_parent'])
            array_push($ancestors, $test_id);
        return $ancestors;
    }

    function path_for_test($test_id) {
        $id_to_test = $this->ensure_id_to_test_map();
        $path = array();
        while ($test_id) {
            $test = $id_to_test[$test_id];
            $test_id = $test['test_parent'];
            array_unshift($path, $test['test_name']);
        }
        return $path;
    }

    private function ensure_id_to_test_map() {
        if ($this->id_to_test_map == NULL) {
            $map = array();
            foreach ($this->db->fetch_table('tests') as $row)
                $map[$row['test_id']] = $row;
            $this->id_to_test_map = $map;
        }
        return $this->id_to_test_map;
    }
}

?>
