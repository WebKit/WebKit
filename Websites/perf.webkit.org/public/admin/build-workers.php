<?php

require('../include/admin-header.php');

if ($db) {

    if ($action == 'add') {
        if ($db->insert_row('build_workers', 'worker', array('name' => $_POST['name'], 'password_hash' => hash('sha256', $_POST['password'])))) {
            notice('Inserted the new worker.');
            regenerate_manifest();
        } else
            notice('Could not add the worker.');
    } else if ($action == 'update') {
        if (update_field('build_workers', 'worker', 'name'))
            regenerate_manifest();
        else if (update_field('build_workers', 'worker', 'password_hash', hash('sha256', $_POST['new_password'])))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    }

    $page = new AdministrativePage($db, 'build_workers', 'worker', array(
        'name' => array('size' => 50, 'editing_mode' => 'string'),
        'password_hash' => array(),
        'password' => array('pre_insertion' => TRUE, 'editing_mode' => 'string'),
        'new_password' => array('post_insertion' => TRUE, 'editing_mode' => 'string'),
    ));

    $page->render_table('name');
    $page->render_form_to_add();
}

require('../include/admin-footer.php');

?>
