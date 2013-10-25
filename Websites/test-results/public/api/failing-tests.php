<?php

require_once('../include/json-shared.php');
require_once('../include/test-results.php');

function main() {
    require_existence_of($_GET, array('builder' => '/^[A-Za-z0-9 \(\)\-_]+$/'));
    $builder_name = $_GET['builder'];

    $db = connect();
    $builder_row = $db->select_first_row('builders', NULL, array('name' => $builder_name));
    if (!$builder_row)
        exit_with_error('BuilderNotFound');
    $builder_id = $builder_row['id'];

    $generator = new ResultsJSONGenerator($db, $builder_id);

    if ($generator->generate())
        exit_with_success();
    else
        exit_with_error('ResultsNotFound');
}

main();

?>
