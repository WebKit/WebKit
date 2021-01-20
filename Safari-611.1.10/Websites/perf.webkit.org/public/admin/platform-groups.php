<?php

require('../include/admin-header.php');

if ($db) {
    if ($action == 'add') {
        if ($db->insert_row('platform_groups', 'platformgroup', array('name' => $_POST['name']))) {
            notice('Inserted the new platform group');
            regenerate_manifest();
        } else
            notice('Could not add the platform group.');
    } else if ($action == 'update') {
        if (update_field('platform_groups', 'platformgroup', 'name'))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    }

    $page = new AdministrativePage($db, 'platform_groups', 'platformgroup', array('name' => array('size' => 50, 'editing_mode' => 'string')));

    $page->render_table('name');
    $page->render_form_to_add();
}

require('../include/admin-footer.php');

?>
