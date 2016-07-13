<?php

require('../include/admin-header.php');

if ($db) {

    if ($action == 'add') {
        if ($db->insert_row('builders', NULL, array(
            'name' => $_POST['name'], 'password_hash' => hash('sha256', $_POST['password']), 'build_url' => array_get($_POST, 'build_url')))) {
            notice('Inserted the new builder.');
            regenerate_manifest();
        } else
            notice('Could not add the builder.');
    } else if ($action == 'update') {
        if (update_field('builders', NULL, 'name') || update_field('builders', NULL, 'build_url'))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    }

    $page = new AdministrativePage($db, 'builders', NULL, array(
        'master' => array(),
        'name' => array('size' => 50, 'editing_mode' => 'string'),
        'build_url' => array('label' => 'Build URL', 'size' => 100, 'editing_mode' => 'url'),
    ));

    $page->render_table('name');
    $page->render_form_to_add();
}

require('../include/admin-footer.php');

?>
