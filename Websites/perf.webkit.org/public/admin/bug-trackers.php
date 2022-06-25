<?php

include('../include/admin-header.php');

if ($db) {
    if ($action == 'add') {
        if ($db->insert_row('bug_trackers', 'tracker', array(
            'name' => $_POST['name'], 'new_bug_url' => $_POST['new_bug_url']))) {
            notice('Inserted the new bug tracker.');
            regenerate_manifest();
        } else
            notice('Could not add the bug tracker.');
    } else if ($action == 'update') {
        if (update_field('bug_trackers', 'tracker', 'name')
            || update_field('bug_trackers', 'tracker', 'bug_url')
            || update_field('bug_trackers', 'tracker', 'new_bug_url'))
            regenerate_manifest();
        else
            notice('Invalid parameters.');
    } else if ($action == 'associate') {
        $tracker_id = intval($_POST['id']);
        $db->query_and_get_affected_rows("DELETE FROM tracker_repositories WHERE tracrepo_tracker = $1", array($tracker_id));

        $suceeded = TRUE;
        $tracker_repositories = array_get($_POST, 'tracker_repositories');
        if ($tracker_repositories) {
            foreach ($tracker_repositories as $repository_id) {
                if (!$db->insert_row('tracker_repositories', 'tracrepo',
                    array('tracker' => $tracker_id, 'repository' => $repository_id), NULL)) {
                    $suceeded = TRUE;
                    notice("Failed to associate repository $repository_id with tracker $tracker_id.");
                }
            }
        }
        if ($suceeded) {
            notice('Updated the association.');
            regenerate_manifest();
        }
    }

    function associated_repositories($row) {
        global $db;

        $tracker_repositories = $db->query_and_fetch_all('SELECT * FROM repositories LEFT OUTER JOIN tracker_repositories
            ON tracrepo_repository = repository_id AND (tracrepo_tracker = $1 OR tracrepo_tracker IS NULL)
            ORDER BY repository_name', array($row['tracker_id']));

        $content = <<< END
<form method="POST"><input type="hidden" name="id" value="{$row['tracker_id']}">
END;

        foreach ($tracker_repositories as $repository) {
            $id = intval($repository['repository_id']);
            $name = htmlspecialchars($repository['repository_name']);

            $checked = $repository['tracrepo_tracker'] ? ' checked' : '';
            $content .= "<label><input type=\"checkbox\" name=\"tracker_repositories[]\" value=\"{$id}\"$checked>$name</label>";
        }

        $content .= <<< END
<button type="submit" name="action" value="associate">Save</button></form>
END;
        return array($content);
    }

    $page = new AdministrativePage($db, 'bug_trackers', 'tracker', array(
        'name' => array('editing_mode' => 'string'),
        'bug_url' => array('editing_mode' => 'url', 'label' => 'Bug URL ($number)'),
        'new_bug_url' => array('editing_mode' => 'text', 'label' => 'New Bug URL ($title, $description)'),
        'Associated repositories' => array('custom' => function ($row) { return associated_repositories($row); }),
    ));

    $page->render_table('name');
    $page->render_form_to_add('New Bug Tracker');

}

include('../include/admin-footer.php');

?>
