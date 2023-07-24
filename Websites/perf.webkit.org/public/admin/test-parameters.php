<?php

require('../include/admin-header.php');

if ($db) {
    $key_types = $db->fetch_enum_values('test_parameter_types');
    if ($action == 'add') {
        $key_type = $_POST['type'];
        if (!in_array($key_type, $key_types)) {
            notice('Invalid key type ' . $key_type);
        } elseif ($db->insert_row('test_parameters', 'testparam', array(
            'name' => $_POST['name'], 'disabled' => Database::to_database_boolean(array_get($_POST, 'disabled', false)),
            'has_value' => Database::to_database_boolean(array_get($_POST, 'has_value', false)),
            'has_file' => Database::to_database_boolean(array_get($_POST, 'has_file', false)),
            'type' => $key_type, 'description' => $_POST['description']
            ))) {
            notice('Inserted the new test parameter.');
            regenerate_manifest();
        } else
            notice('Could not insert test parameter.');
    } else if ($action == 'update') {
        if (update_field('test_parameters', 'testparam', 'name') ||
            update_field('test_parameters', 'testparam', 'type') ||
            update_field('test_parameters', 'testparam', 'description') ||
            update_boolean_field('test_parameters', 'testparam', 'disabled') ||
            update_boolean_field('test_parameters', 'testparam', 'has_value') ||
            update_boolean_field('test_parameters', 'testparam', 'has_file'))
            regenerate_manifest();
        else
            notice('Invalid parameters update.');
    }

    $page = new AdministrativePage($db, 'test_parameters', 'testparam', array(
        'name' => array('editing_mode' => 'string'),
        'type' => array('selections' => $key_types, 'editing_mode' => 'select'),
        'disabled' => array('editing_mode' => 'boolean'),
        'has_value' => array('editing_mode' => 'boolean'),
        'has_file' => array('editing_mode' => 'boolean'),
        'description' => array('editing_mode' => 'text'),
    ));

    $page->render_table('name');
    $page->render_form_to_add();
}

require('../include/admin-footer.php');

?>
