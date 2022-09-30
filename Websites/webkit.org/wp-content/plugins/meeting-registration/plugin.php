<?php
/*
Plugin Name: Meeting Registration
Description: Enforce strong passwords
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

class WebKit_Meeting_Registration {

    const COOKIE_PREFIX = 'wkmr_';
    const ADMIN_PAGE_SLUG = 'meeting-registration';
    const CUSTOM_POST_TYPE = 'meeting_registration';
    const MEETING_TAXONOMY = 'webkit_meeting';
    const CURRENT_MEETING_OPTION = 'current_webkit_meeting';
    const MEETING_REGISTRATION_STATE_SETTING = 'meeting-registration-state';
    const CONTRIBUTORS_JSON = 'https://github.com/WebKit/WebKit/raw/main/metadata/contributors.json';
    const EXTRA_FIELDS = [
        'slack'       => FILTER_SANITIZE_STRING,
        'affiliation' => FILTER_SANITIZE_STRING,
        'interests'   => FILTER_SANITIZE_STRING,
        'claim'       => FILTER_SANITIZE_STRING,
        'optingame'   => FILTER_SANITIZE_STRING
    ];

    static $refresh = false;
    static $current_meeting = '';
    static $registration_state = 'closed';

    public static function init() {
        add_action('init', ['WebKit_Meeting_Registration', 'register_post_type']);
        add_action('init', ['WebKit_Meeting_Registration', 'register_shortcodes']);
        add_action('admin_menu', ['WebKit_Meeting_Registration', 'register_admin_page']);
        self::$current_meeting = get_option(self::CURRENT_MEETING_OPTION);
        self::$registration_state = get_option(self::MEETING_REGISTRATION_STATE_SETTING);
    }

    public static function register_post_type() {
        register_taxonomy(self::MEETING_TAXONOMY, [self::CUSTOM_POST_TYPE], [
            'label' => __('WebKit Meeting'),
            'hierarchical' => false,
            'rewrite' => ['slug' => 'webkit-meeting'],
            'show_admin_column' => false,
            'show_in_rest' => false,
            'labels' => [
                'singular_name' => __('WebKit Meeting'),
                'all_items' => __('All WebKit Meetings'),
                'edit_item' => __('Edit WebKit Meeting'),
                'view_item' => __('View WebKit Meeting'),
                'update_item' => __('Update WebKit Meeting'),
                'add_new_item' => __('Add New WebKit Meeting'),
                'new_item_name' => __('New WebKit Meeting Name'),
                'search_items' => __('Search WebKit Meetings'),
                'popular_items' => __('Popular WebKit Meetings'),
                'separate_items_with_commas' => __('Separate WebKit Meeting names with a comma'),
                'choose_from_most_used' => __('Choose from most used WebKit Meetings'),
                'not_found' => __('No WebKit Meetings found'),
            ]
        ]);

        register_post_type(self::CUSTOM_POST_TYPE, [
            'label'                 => 'Registration',
            'description'           => 'WebKit Contributors Meeting registrations.',
            'taxonomies'            => ['webkit_meeting'],
            'hierarchical'          => false,
            'public'                => false,
            'show_ui'               => false,
            'show_in_menu'          => false,
            'menu_position'         => 5,
            'show_in_admin_bar'     => false,
            'show_in_nav_menus'     => false,
            'can_export'            => true,
            'has_archive'           => true,
            'exclude_from_search'   => false,
            'publicly_queryable'    => false,
            'rewrite'               => false,
            'capability_type'       => 'post',
            'labels'                => [
                'name'                  => 'Registrations',
                'singular_name'         => 'Registration',
                'menu_name'             => 'Contributors Meeting',
                'name_admin_bar'        => 'Meeting Registration',
                'archives'              => 'Registrations',
                'attributes'            => 'Registration Attributes',
                'parent_item_colon'     => 'Parent Registration:',
                'all_items'             => 'All Registrations',
                'add_new_item'          => 'Add New Registration',
                'add_new'               => 'Add New',
                'new_item'              => 'New Registration',
                'edit_item'             => 'Edit Registration',
                'update_item'           => 'Update Registration',
                'view_item'             => 'View Registration',
                'view_items'            => 'View Registrations',
                'search_items'          => 'Search Registrations',
                'not_found'             => 'Not found',
                'not_found_in_trash'    => 'Not found in Trash',
                'featured_image'        => 'Featured Image',
                'set_featured_image'    => 'Set featured image',
                'remove_featured_image' => 'Remove featured image',
                'use_featured_image'    => 'Use as featured image',
                'insert_into_item'      => 'Insert into Registration',
                'uploaded_to_this_item' => 'Uploaded to this registration',
                'items_list'            => 'Registrations list',
                'items_list_navigation' => 'Registrations list navigation',
                'filter_items_list'     => 'Filter registrations list'
            ]
        ]);

        register_taxonomy_for_object_type('webkit_meeting', self::CUSTOM_POST_TYPE);

    }

    public static function register_shortcodes() {
        add_shortcode('meeting-registration', ['WebKit_Meeting_Registration', 'registration_shortcode']);
        add_shortcode('contributors-meeting-registration-form', ['WebKit_Meeting_Registration', 'form_shortcode']);
    }

    public function registration_shortcode($atts, $content) {
        $registration = self::registration();

        if (!empty($registration))
            $content = $registered = $atts['registered'];

        if (self::$registration_state == 'closed')
            $content = $atts['closed'] . (!empty($registered) ? " $registered" : "");

        if (!empty($content))
            return '<p>' . $content . '</p>';
    }

    public function form_shortcode($atts, $content) {
        if (!class_exists('GitHubOAuthPlugin'))
            return '';

        if (!GitHubOAuthPlugin::$Auth->has_access())
            return $content;

        $posted_data = filter_input_array(INPUT_POST, self::EXTRA_FIELDS);
        if (!empty($posted_data) && $posted_data['claim'] !== 'on')
            return $content;

        ob_start();
        include(dirname(__FILE__) . '/form.php');
        return ob_get_clean();
    }

    public static function is_contributor() {
        if (!class_exists('GitHubOAuthPlugin'))
            return false;

        return GitHubOAuthPlugin::is_contributor();
    }

    public static function process() {
        $posted_data = filter_input_array(INPUT_POST, self::EXTRA_FIELDS);

        if (empty($posted_data))
            return true;

        if (!wp_verify_nonce(filter_input(INPUT_POST, '_nonce'), self::$current_meeting))
            wp_die('Invalid WebKit Meeting Registration submission.');

        $User = self::get_github_user();
        if (!$User)
            return 'invalid-user';

        if ($posted_data['claim'] !== 'on')
            return 'invalid-claim';

        $post_id = wp_insert_post([
            'post_status' => 'publish',
            'post_type' => 'meeting_registration',
            'post_title' => self::$current_meeting . '-' . $User->id,
            'post_content' => wp_hash($User->email, 'logged_in'),
            'post_author' => $User->id
        ]);
        $post = get_post($post_id);

        // Associate registration with the current meeting
        wp_set_object_terms($post_id, self::$current_meeting, self::MEETING_TAXONOMY, false );

        if (!empty($User->email))
            $posted_data['github_email'] = $User->email;

        if (!empty($User->name))
            $posted_data['github_name'] = $User->name;

        if (!empty($User->login))
            $posted_data['github_login'] = $User->login;

        foreach ($posted_data as $name => $value) {
            add_post_meta($post_id, $name, $value);
        }

        return true;
    }

    public static function responded() {
        $registration = self::registration();
        return !empty($registration);
    }

    public static function registration() {
        $User = self::get_github_user();

        if (!isset($User->id))
            return false;

        $posts_array = get_posts([
            self::MEETING_TAXONOMY => self::$current_meeting,
            'post_type'            => self::CUSTOM_POST_TYPE,
            'author'               => $User->id,
            'posts_per_page'       => -1
        ]);

        if (isset($posts_array[0]))
            return self::registration_data_from_post($posts_array[0]);

        return false;
    }

    public static function registration_data_from_post($post) {
        if (empty($post->ID) || empty($post->post_content))
            return false;

        $registration = new StdClass();
        $registration->id = $post->ID;
        $registration->github_id = $post->post_author;
        $registration->hash = $post->post_content;

        return $registration;
    }

    public static function full_registration($entry = false) {
        $index = WebKit_Meeting_Registration::get_indexed_contributors();

        $registration = !empty($entry) ? self::registration_data_from_post($entry) : self::registration();
        if (!$registration)
            return false;

        $custom = get_post_custom($registration->id);

        if (isset($index[$registration->hash]))
            $contributor = $index[$registration->hash];

        $registration->contributor_name = $contributor->name;
        $registration->contributor_email = is_array($contributor->emails) ? reset($contributor->emails) : $contributor->emails;
        $registration->contributor_github = isset($contributor->github) ? $contributor->github : '';
        $registration->contributor_status = isset($contributor->status) ? $contributor->status : '';

        if (empty($registration->contributor_name))
            $registration->contributor_name = $custom['github_name'][0];

        if (empty($registration->contributor_email))
            $registration->contributor_email = $custom['github_email'][0];

        if (empty($registration->contributor_github))
            $registration->contributor_github = $custom['github_login'][0];

        foreach (self::EXTRA_FIELDS as $key => $entry) {
            $property = "contributor_$key";
            if (isset($custom[$key][0]))
                $registration->$property = $custom[$key][0];
        }

        return $registration;
    }

    public static function get_indexed_contributors() {
        $cachekey = 'wk_contributors_' . crc32(WP_HOST);
        if ( false !== ( $cached = get_transient($cachekey) ) && ! self::$refresh )
            return unserialize($cached);

        $contributors_file = file_get_contents(self::CONTRIBUTORS_JSON);
        $contributors_data = json_decode($contributors_file);
        $indexed_contributors = [];
        foreach ((array)$contributors_data as $id => $entry) {
            foreach($entry->emails as $email) {
                $hash = wp_hash(trim($email), 'logged_in');
                $indexed_contributors[$hash] = $entry;
            }
        }

        set_transient($cachekey, serialize($indexed_contributors), 300);
        return $indexed_contributors;
    }

    public static function get_github_user() {
        if (!class_exists('GitHubOAuthPlugin'))
            return false;

        $API = GitHubOAuthPlugin::$API;
        $User = $API->get_user();

        return $User;
    }

    public static function reset_access() {
        if (!class_exists('GitHubOAuthPlugin'))
            return false;

        $Auth = GitHubOAuthPlugin::$Auth;
        $Auth->reset_access();
    }

    public static function form_nonce() {
        return '<input type="hidden" name="_nonce" value="' . wp_create_nonce(self::$current_meeting) . '">';
    }

    public static function register_admin_page() {
        $pageslug = filter_input(INPUT_GET, 'page');
        $download = filter_input(INPUT_GET, 'download');
        if ($pageslug == self::ADMIN_PAGE_SLUG && $download == "csv")
            self::download() && exit;

        add_dashboard_page(__('Contributors Meeting'), __('Contributors Meeting'), 'read', self::ADMIN_PAGE_SLUG, ['WebKit_Meeting_Registration', 'admin_page']);
    }

    public static function process_admin() {
        // Process form updates
        $posted_data = filter_input_array(INPUT_POST,  [
            'meeting_title' => FILTER_SANITIZE_STRING,
            'toggle_registration' => FILTER_SANITIZE_STRING,
            'delete_button' => FILTER_SANITIZE_STRING,
            'delete' => [
                'filter' => FILTER_VALIDATE_INT,
                'flags'  => FILTER_REQUIRE_ARRAY
            ]
        ]);

        // Create a new meeting taxonomy
        if (!empty($posted_data['meeting_title'])) {
            $current_meeting_setting = get_option(self::CURRENT_MEETING_OPTION);

            if ($current_meeting_setting !== false)
                $term = get_term_by('slug', $current_meeting_setting, self::MEETING_TAXONOMY);

            $meeting_slug = sanitize_title_with_dashes($posted_data['meeting_title']);
            if (!$term || (isset($term->slug) && $term->slug !== $meeting_slug)) {

                $new_term = get_term_by('slug', $meeting_slug, self::MEETING_TAXONOMY);

                if (!isset($new_term->slug)) {
                    // Create a new term
                    $inserted_term = wp_insert_term(
                        $posted_data['meeting_title'],
                        self::MEETING_TAXONOMY,
                        ['slug' => $meeting_slug]
                    );
                }

                // Set the new current meeting option
                if (!$current_meeting_setting) {
                    add_option(self::CURRENT_MEETING_OPTION, $meeting_slug, 'deprecated', false);
                } else {
                    update_option(self::CURRENT_MEETING_OPTION, $meeting_slug);
                }

                self::$current_meeting = $meeting_slug;
            }
        }

        // Open and close registration
        if (!empty($posted_data['toggle_registration'])) {
            if (!self::$registration_state) {
                self::$registration_state = 'closed';
                add_option(self::MEETING_REGISTRATION_STATE_SETTING, 'closed', 'deprecated', false);
            } else {
                self::$registration_state = self::$registration_state === 'closed' ? 'open' : 'closed';
                update_option(self::MEETING_REGISTRATION_STATE_SETTING, self::$registration_state);
            }
        }

        // Delete registration(s)
        if (!empty($posted_data['delete_button'])) {
            foreach ($posted_data['delete'] as $id) {
                wp_delete_post($id, true);
            }
        }

    }

    public static function admin_page() {
        self::process_admin();

        // Load records for display
        $posts_array = get_posts([
            self::MEETING_TAXONOMY => self::$current_meeting,
            'post_type'      => self::CUSTOM_POST_TYPE,
            'posts_per_page' => -1
        ]);
        $total = count($posts_array);
        $registration_label = _n(
            'registration',
            'registrations',
            $total
        );

        $current_meeting_term = get_term_by('slug', self::$current_meeting, self::MEETING_TAXONOMY);

        include(dirname(__FILE__) . '/admin.php');
    }

    private static function download() {
        $data = get_posts([
            self::MEETING_TAXONOMY => self::$current_meeting,
            'post_type'      => self::CUSTOM_POST_TYPE,
            'author'         => $User->id,
            'posts_per_page' => -1
        ]);

        header('Content-type: text/csv; charset=UTF-8');
        header('Content-Disposition: attachment; filename="' . self::ADMIN_PAGE_SLUG . '.csv"');
        header('Content-Description: Delivered by webkit.org');
        header('Cache-Control: maxage=1');
        header('Pragma: public');

        $out = fopen('php://output', 'w');
        $first_entry = reset($data);
        $registration = (array)self::full_registration($first_entry);
        fputcsv($out, array_keys($registration));

        foreach ($data as $entry) {
            $registration = (array)self::full_registration($entry);
            fputcsv($out, $registration);
        }
        fclose($out);
        exit;
    }

}

WebKit_Meeting_Registration::init();