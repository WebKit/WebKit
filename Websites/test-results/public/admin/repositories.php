<?php

include('../include/admin-header.php');

if ($db) {
    if ($action == 'update') {
        if (update_field('repositories', NULL, 'name')
            || update_field('repositories', NULL, 'url')
            || update_field('repositories', NULL, 'blame_url'))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    }

    $page = new AdministrativePage($db, 'repositories', NULL, array(
        'name' => array('editing_mode' => 'string'),
        'url' => array('label' => 'Revision URL (At revision $1)', 'editing_mode' => 'url'),
        'blame_url' => array('label' => 'Blame URL (From revision $1 to revision $2)', 'editing_mode' => 'url')
    ));

    $page->render_table('name');
}

include('../include/admin-footer.php');

?>
