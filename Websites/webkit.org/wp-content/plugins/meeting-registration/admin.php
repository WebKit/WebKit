
<style>
    .column-name {
        font-weight: 600;
    }

    .checked-box {
        border: 1px solid #8c8f94;
        border-radius: 4px;
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen-Sans, Ubuntu, Cantarell, "Helvetica Neue", sans-serif;

        display: inline-block;
        line-height: 0;
        height: 1rem;
        margin: -0.25rem 0.25rem 0 0;
        outline: 0;
        padding: 0!important;
        text-align: center;
        vertical-align: middle;
        width: 1rem;
        min-width: 1rem;
        box-shadow: inset 0 1px 2px rgba(0,0,0,.1);
        transition: .05s border-color ease-in-out;
    }

    .checked-box::before {
        content: url("data:image/svg+xml;utf8,%3Csvg%20xmlns%3D%27http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%27%20viewBox%3D%270%200%2020%2020%27%3E%3Cpath%20d%3D%27M14.83%204.89l1.34.94-5.81%208.38H9.02L5.78%209.67l1.34-1.25%202.57%202.4z%27%20fill%3D%27%233582c4%27%2F%3E%3C%2Fsvg%3E");
        margin: -0.1875rem 0 0 -0.05rem;
        height: 1.3125rem;
        width: 1.3125rem;
    }

    #new-meeting-form {
        display: none;
    }

    #new-meeting-form.show {
        display: inline-block;
    }
</style>
<div class="wrap">

    <h1>WebKit Contributors Meeting Registrations</h1>
    <h2><?php esc_html_e($current_meeting_term->name); ?></h2>

    <div class="tablenav top">
        <a href="<?php echo admin_url('admin.php?page=' . WebKit_Meeting_Registration::ADMIN_PAGE_SLUG . '&download=csv'); ?>" class="button action">Download</a>
        <div class="tablenav-pages">
            <span class="displaying-num"><?php echo esc_html("$total $registration_label"); ?></span>
        </div>
    </div>
    <form action="" method="post">
    <table class="wp-list-table widefat fixed striped table-view-list registrations">
        <thead>
            <tr>
                <td class="column-cb check-column">
                    <label class="screen-reader-text" for="cb-select-all-1">Select All</label>
                    <input type="checkbox" id="cb-select-all-1">
                </td>
                <th>Name</th>
                <th>Affiliation</th>
                <th>GitHub</th>
                <th>Slack</th>
                <th>Interests</th>
                <th>Optional</th>
                <th>Contributor</th>
            </tr>
        </thead>
        <tbody>
        <?php
            foreach ($posts_array as $entry):
                $registration = WebKit_Meeting_Registration::full_registration($entry);
        ?>
        <tr>
            <th class="check-column"><input type="checkbox" name="delete[]" value="<?php echo esc_attr($registration->id); ?>"></th>
            <td class="column-name"><?php echo esc_html($registration->contributor_name); ?><br>
                <a href="mailto:<?php echo esc_attr($registration->contributor_email); ?>"><?php echo esc_html($registration->contributor_email); ?></a>
            </td>
            <td><?php echo esc_html($registration->contributor_affiliation); ?></td>
            <td><a href="https://github.com/<?php echo esc_attr($registration->contributor_github); ?>"><?php echo esc_html($registration->contributor_github); ?></a>
            </td>
            <td><?php echo esc_html($registration->contributor_slack); ?></td>
            <td><?php echo apply_filters('the_content', esc_html($registration->contributor_interests)); ?></td>
            <td><?php if ($registration->contributor_optingame === "on"):?><div class="checked-box"></div><?php endif; ?></td>
            <td><?php if ($registration->contributor_claim === "on"):?><div class="checked-box"></div><?php endif; ?></td>
        </tr>
        <?php endforeach; ?>
        </tbody>
    </table>
    <div class="tablenav bottom">
        <div class="alignleft actions">
            <input type="submit" id="delete-button" name="delete_button" value="Delete" class="button button-secondary">
        </div>
        <div class="alignright actions">
                <input type="hidden" name="page" value="<?php echo esc_attr(WebKit_Meeting_Registration::ADMIN_PAGE_SLUG); ?>">
            <span id="new-meeting-form">
                <label for="meeting-title-input">Meeting Title: </label>
                <input type="text" name="meeting_title" id="meeting-title-input" value="" size="26">
                <input id="new-meeting-save" type="submit" name="" value="Save" class="button button-primary">
                <input id="new-meeting-cancel-button" type="button" name="" value="Cancel" class="button cancel">
            </span>
            <input id="new-meeting-button" type="button" name="" value="New Meeting" class="button">
            <?php if (WebKit_Meeting_Registration::$registration_state === "open"): ?>
                <input type="submit" name="toggle_registration" value="Close Registration" class="button">
            <?php else: ?>
                <input type="submit" name="toggle_registration" value="Open Registration" class="button">
            <?php endif; ?>
        </div>
    </div>
    </form>
    <script>
    let new_meeting_form = document.getElementById('new-meeting-form');
    let new_meeting_button = document.getElementById('new-meeting-button');
    new_meeting_button.addEventListener('click', function() {
        new_meeting_form.classList.add('show');
        this.classList.add('hidden');
    });
    document.getElementById('new-meeting-cancel-button').addEventListener('click', function() {
        new_meeting_form.classList.remove('show');
        new_meeting_button.classList.remove('hidden');
    });
    document.getElementById('delete-button').addEventListener('click', function(e) {
        if (!confirm("Are you sure you want to delete the selected registrations?")) {
            e.preventDefault();
        }
    });
    </script>
</div>