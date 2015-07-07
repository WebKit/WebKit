<?php

require_once('../include/json-shared.php');

$db = connect();

$repositories = $db->fetch_table('repositories');
foreach ($repositories as &$value)
    $value['blameUrl'] = $value['blame_url'];

$builders = $db->fetch_table('builders');
foreach ($builders as &$value)
    $value['buildUrl'] = $value['build_url'];

exit_with_success(array('tests' => $db->fetch_table('tests'),
    'builders' => $builders,
    'slaves' => $db->fetch_table('slaves'),
    'repositories' => $repositories,
    'testCategories' => config('testCategories'),
    'newBugLinks' => config('newBugLinks')));
?>
