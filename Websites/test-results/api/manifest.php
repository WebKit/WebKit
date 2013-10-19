<?php

require_once('../include/json-shared.php');

$db = connect();

exit_with_success(array('tests' => $db->fetch_table('tests'),
    'builders' => $db->fetch_table('builders'),
    'slaves' => $db->fetch_table('slaves'),
    'repositories' => $db->fetch_table('repositories')));

?>
