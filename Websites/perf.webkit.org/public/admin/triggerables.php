<?php

require('../include/admin-header.php');

if ($db) {

    if ($action == 'add') {
        if ($db->insert_row('build_triggerables', 'triggerable', array('name' => $_POST['name']))) {
            notice('Inserted the new triggerable.');
            regenerate_manifest();
        } else
            notice('Could not add the triggerable.');
    } else if ($action == 'update') {
        if (update_field('build_triggerables', 'triggerable', 'name'))
            regenerate_manifest();
        else if (update_field('build_triggerables', 'triggerable', 'disabled', Database::to_database_boolean(array_get($_POST, 'disabled'))))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    } else if ($action == 'update-group-name') {
        if (update_field('triggerable_repository_groups', 'repositorygroup', 'name'))
            regenerate_manifest();
    } else if ($action == 'update-group-description') {
        if (update_field('triggerable_repository_groups', 'repositorygroup', 'description'))
            regenerate_manifest();
    } else if ($action == 'update-group-hidden') {
        if (update_field('triggerable_repository_groups', 'repositorygroup', 'hidden'))
            regenerate_manifest();
    } else if ($action == 'update-group-accept-roots') {
        if (update_field('triggerable_repository_groups', 'repositorygroup', 'accepts_roots',
            Database::to_database_boolean(array_get($_POST, 'accepts'))))
            regenerate_manifest();
    } else if ($action == 'update-repositories') {
        $group_id = intval($_POST['group']);

        $db->begin_transaction();
        $db->query_and_get_affected_rows("DELETE FROM triggerable_repositories WHERE trigrepo_group = $1", array($group_id));
        $suceeded = insert_triggerable_repositories($db, $group_id, array_get($_POST, 'repositories'));
        if ($suceeded) {
            $db->commit_transaction();
            notice('Updated the association.');
            regenerate_manifest();
        } else
            $db->rollback_transaction();
    } else if ($action == 'update-accept-patch') {
        $group_id = intval($_POST['group']);
        $repositories_that_accepts_patch = array_get($_POST, 'repositories', array());

        $db->begin_transaction();
        if (!$db->query_and_get_affected_rows("UPDATE triggerable_repositories SET trigrepo_accepts_patch = FALSE WHERE trigrepo_group = $1", array($group_id))) {
            notice('Failed to update the accept-patch status.');
            $db->rollback_transaction();
        } else {
            foreach ($repositories_that_accepts_patch as $repository_id) {
                if (!$db->query_and_get_affected_rows("UPDATE triggerable_repositories SET trigrepo_accepts_patch = TRUE
                    WHERE trigrepo_group = $1 AND trigrepo_repository = $2", array($group_id, $repository_id))) {
                    notice('Failed to update the accept-patch status.');
                    $db->rollback_transaction();
                }
            }
            $db->commit_transaction();
            notice('Updated the accept-patch status.');
            regenerate_manifest();
        }
    } else if ($action == 'add-repository-group') {
        $triggerable_id = intval($_POST['triggerable']);
        $name = $_POST['name'];

        $db->begin_transaction();
        $group_id = $db->insert_row('triggerable_repository_groups', 'repositorygroup', array('name' => $name, 'triggerable' => $triggerable_id));
        if (!$group_id)
            notice('Failed to insert the specified repository group.');
        else if (!insert_triggerable_repositories($db, $group_id, array_get($_POST, 'repositories')))
            $db->rollback_transaction();
        else {
            $db->commit_transaction();
            notice('Updated the association.');
            regenerate_manifest();
        }
    }

    $repository_rows = $db->select_rows('repositories', 'repository', array('owner' => NULL), 'name');

    $page = new AdministrativePage($db, 'build_triggerables', 'triggerable', array(
        'name' => array('editing_mode' => 'string'),
        'disabled' => array('editing_mode' => 'boolean', 'post_insertion' => TRUE),
        'repositories' => array(
            'label' => 'Repository Groups',
            'subcolumns'=> array('ID', 'Name', 'Description', 'Hidden', 'Accepts Roots', 'Repositories', 'Accept patches'),
            'custom' => function ($triggerable_row) use (&$db, &$repository_rows) {
                return generate_repository_list($db, $triggerable_row['triggerable_id'], $repository_rows);
            }),
    ));

    $page->render_table('name');
    $page->render_form_to_add();
}

function insert_triggerable_repositories($db, $group_id, $repositories)
{
    if (!$repositories)
        return TRUE;
    foreach ($repositories as $repository_id) {
        if (!$db->insert_row('triggerable_repositories', 'trigrepo', array('group' => $group_id, 'repository' => $repository_id), NULL)) {
            notice("Failed to associate repository $repository_id with repository group $group_id.");
            return FALSE;
        }
    }
    return TRUE;
}


function generate_repository_list($db, $triggerable_id, $repository_rows) {
    $group_forms = array();

    $repository_groups = $db->select_rows('triggerable_repository_groups', 'repositorygroup', array('triggerable' => $triggerable_id), 'name');
    foreach ($repository_groups as $group_row) {
        $group_id = $group_row['repositorygroup_id'];
        $group_name = $group_row['repositorygroup_name'];
        $group_description = $group_row['repositorygroup_description'];
        $checked_if_hidden = Database::is_true($group_row['repositorygroup_hidden']) ? 'checked' : '';
        $checked_if_accepts_roots = Database::is_true($group_row['repositorygroup_accepts_roots']) ? 'checked' : '';

        $group_name_form = <<< END
            <form method="POST">
            <input type="hidden" name="action" value="update-group-name">
            <input type="hidden" name="id" value="$group_id">
            <input type="text" name="name" value="$group_name">
            </form>
END;

        $group_description_form = <<< END
            <form method="POST">
            <input type="hidden" name="action" value="update-group-description">
            <input type="hidden" name="id" value="$group_id">
            <input name="description" value="$group_description">
            </form>
END;

        $group_hidden_form = <<< END
            <form method="POST">
            <input type="hidden" name="action" value="update-group-hidden">
            <input type="hidden" name="id" value="$group_id">
            <input type="checkbox" name="hidden" $checked_if_hidden>
            <button type="submit">Save</button>
            </form>
END;

        $group_accepts_roots = <<< END
            <form method="POST">
            <input type="hidden" name="action" value="update-group-accept-roots">
            <input type="hidden" name="id" value="$group_id">
            <input type="checkbox" name="accepts" $checked_if_accepts_roots>
            <button type="submit">Save</button>
            </form>
END;

        $group_repository_rows = $db->select_rows('triggerable_repositories', 'trigrepo', array('group' => $group_id));
        $repositories_in_group = array();
        $repositories_that_accepts_patch = array();
        foreach ($repository_rows as $row)
            $repositories_in_group[$row['repository_id']] = FALSE;
        foreach ($group_repository_rows as $row) {
            $repository_id = $row['trigrepo_repository'];
            $repositories_in_group[$repository_id] = TRUE;
            $repositories_that_accepts_patch[$repository_id] = Database::is_true($row['trigrepo_accepts_patch']);
        }

        array_push($group_forms, array($group_id, $group_name_form, $group_description_form, $group_hidden_form, $group_accepts_roots,
            generate_repository_form('update-repositories', $group_id, generate_repository_checkboxes($db, $repository_rows, $repositories_in_group)),
            generate_repository_form('update-accept-patch', $group_id, generate_repository_checkboxes($db, $repository_rows, $repositories_that_accepts_patch)),
        ));
    }

    $new_group_checkboxes = generate_repository_checkboxes($db, $repository_rows);
    $new_group_form = <<< END
        <form method="POST">
        <input type="hidden" name="action" value="add-repository-group">
        <input type="hidden" name="triggerable" value="$triggerable_id">
        <input type="text" name="name" value="" required><br>
        $new_group_checkboxes
        <br><button type="submit">Add</button></form>
END;

    array_push($group_forms, $new_group_form);

    return $group_forms;
}

function generate_repository_form($action, $group_id, $checkboxes)
{
    return <<< END
        <form method="POST">
        <input type="hidden" name="action" value="$action">
        <input type="hidden" name="group" value="$group_id">
        $checkboxes
        <br><button type="submit">Save</button></form>
END;
}

function generate_repository_checkboxes($db, $repository_rows, $selected_repositories = array())
{
    $form = '';
    foreach ($repository_rows as $row) {
        $id = $row['repository_id'];
        if (!array_key_exists($id, $selected_repositories))
            continue;
        $name = $row['repository_name'];
        $checked = $selected_repositories[$id] ? 'checked' : '';
        $form .= "<label><input type=\"checkbox\" name=\"repositories[]\" value=\"$id\" $checked>$name</label>";
    }
    return $form;
}

require('../include/admin-footer.php');

?>
