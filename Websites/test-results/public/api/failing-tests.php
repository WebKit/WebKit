<?php

require_once('../include/json-shared.php');
require_once('../include/test-results.php');

function main() {
    require_existence_of($_GET, array('builder' => '/^[A-Za-z0-9 \(\)\-_]+$/'));
    $builder_param = $_GET['builder'];

    $db = connect();
    $builder_row = $db->select_first_row('builders', NULL, array('name' => $builder_param));
    if (!$builder_row) {
        $builder_row = $db->select_first_row('builders', NULL, array('id' => $builder_param));
        if (!$builder_row)
            exit_with_error('BuilderNotFound');
    }
    $builder_id = $builder_row['id'];

    $generator = new ResultsJSONGenerator($db, $builder_id);

    if (!$generator->generate('wrongexpectations'))
        exit_with_error('ResultsWithWrongExpectationsNotFound', array('builderId' => $builder_id));
    else if (!$generator->generate('flaky'))
        exit_with_error('FlakyResultsNotFound', array('builderId' => $builder_id));
    else
        exit_with_success();
}

main();

?>
