<?php

include('../include/admin-header.php');

if ($db) {

    if ($action == 'add') {
        if (array_key_exists('name', $_POST) && array_key_exists('definition', $_POST)) {
            if ($db->insert_row('aggregators', 'aggregator', array('name' => $_POST['name'], 'definition' => $_POST['definition'])))
                notice('Inserted the new aggregator');
            else
                notice('Could not add the aggregator');
        } else
            notice('Invalid parameters.');
    } else if ($action == 'update') {
        if (!update_field('aggregators', 'aggregator', 'name') && !update_field('aggregators', 'aggregator', 'definition'))
            notice('Invalid parameters.');
    }

    $page = new AdministrativePage($db, 'aggregators', 'aggregator', array(
        'name' => array('editing_mode' => 'string'),
        'definition' => array('editing_mode' => 'text'),
    ));

    $page->render_table('name');
    $page->render_form_to_add();

}

include('../include/admin-footer.php');

?>
