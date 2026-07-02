<?php

require('../include/admin-header.php');

if ($db) {

    if ($action == 'add') {
        if ($db->insert_row('builders', 'builder', array(
            'name' => $_POST['name'], 'password_hash' => hash('sha256', $_POST['password']), 'build_url' => array_get($_POST, 'build_url')))) {
            notice('Inserted the new builder.');
            regenerate_manifest();
        } else
            notice('Could not add the builder.');
    } else if ($action == 'update') {
        if (update_field('builders', 'builder', 'name') || update_field('builders', 'builder', 'build_url'))
            regenerate_manifest();
        else if (update_field('builders', 'builder', 'password_hash', hash('sha256', $_POST['new_password'])))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    }

    $page = new AdministrativePage($db, 'builders', 'builder', array(
        'name' => array('size' => 50, 'editing_mode' => 'string'),
        'password_hash' => array(),
        'password' => array('pre_insertion' => TRUE, 'editing_mode' => 'string'),
        'new_password' => array('post_insertion' => TRUE, 'editing_mode' => 'string'),
        'build_url' => array('label' => 'Build URL', 'size' => 100, 'editing_mode' => 'url'),
    ));

    $page->render_table('name');
    $page->render_form_to_add();
}

require('../include/admin-footer.php');

?>
