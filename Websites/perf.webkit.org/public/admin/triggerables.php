<?php

require('../include/admin-header.php');

if ($db) {

    if ($action == 'add') {
        if ($db->insert_row('build_triggerables', 'triggerable', array('name' => $_POST['name'], 'location' => $_POST['location']))) {
            notice('Inserted the new triggerable.');
            regenerate_manifest();
        } else
            notice('Could not add the triggerable.');
    } else if ($action == 'update') {
        if (update_field('build_triggerables', 'triggerable', 'name'))
            regenerate_manifest();
        else if (update_field('build_triggerables', 'triggerable', 'location'))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    } else if ($action == 'update-repositories') {
        $triggerable_id = intval($_POST['id']);

        $db->begin_transaction();
        $db->query_and_get_affected_rows("DELETE FROM triggerable_repositories WHERE trigrepo_triggerable = $1", array($triggerable_id));

        $repositories = array_get($_POST, 'repositories');
        $suceeded = TRUE;
        if ($repositories) {
            foreach ($repositories as $repository_id) {
                if (!$db->insert_row('triggerable_repositories', 'trigrepo', array('triggerable' => $triggerable_id, 'repository' => $repository_id), NULL)) {
                    $suceeded = FALSE;
                    notice("Failed to associate repository $repository_id with triggerable $triggerable_id.");
                    break;
                }
            }
        }
        if ($suceeded) {
            $db->commit_transaction();
            notice('Updated the association.');
            regenerate_manifest();
        } else
            $db->rollback_transaction();
    }

    $repository_rows = $db->fetch_table('repositories', 'repository_name');
    $repository_names = array();


    $page = new AdministrativePage($db, 'build_triggerables', 'triggerable', array(
        'name' => array('editing_mode' => 'string'),
        'repositories' => array('custom' => function ($triggerable_row) use (&$repository_rows) {
            return array(generate_repository_checkboxes($triggerable_row['triggerable_id'], $repository_rows));
        }),
    ));

    function generate_repository_checkboxes($triggerable_id, $repository_rows) {
        global $db;

        $repository_rows = $db->query_and_fetch_all('SELECT * FROM repositories LEFT OUTER JOIN triggerable_repositories
            ON trigrepo_repository = repository_id AND trigrepo_triggerable = $1 ORDER BY repository_name', array($triggerable_id));

        $form = <<< END
<form method="POST">
<input type="hidden" name="id" value="$triggerable_id">
<input type="hidden" name="action" value="update-repositories">
END;

        foreach ($repository_rows as $row) {
            $checked = $row['trigrepo_triggerable'] ? ' checked' : '';
            $form .= <<< END
<label><input type="checkbox" name="repositories[]" value="{$row['repository_id']}"$checked>{$row['repository_name']}</label>
END;
        }

        return $form . <<< END
<button>Save</button>
</form>
END;
    }

    $page->render_table('name');
    $page->render_form_to_add();
}

require('../include/admin-footer.php');

?>
