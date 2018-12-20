<?php

// Declare theme features
add_theme_support( 'post-thumbnails' );

add_action( 'init', function () {
    register_nav_menu('site-nav', __( 'Site Navigation' ));
    register_nav_menu('footer-nav', __( 'Footer Navigation' ));
    register_nav_menu('sitemap', __( 'Site Map Page' ));
    register_nav_menu('feature-subnav', __( 'Feature Page Buttons' ));
} );

//add_action( 'wp_header', 'include_invert_lightness_filter');

add_action( 'wp_dashboard_setup', function () {
    $SurveyWidget = new WebKit_Nightly_Survey();
    $SurveyWidget->add_widget();
});

function modify_contact_methods($profile_fields) {

    // Add new fields
    $profile_fields['twitter'] = 'Twitter Handle';
    unset($profile_fields['aim']);
    unset($profile_fields['yim']);
    unset($profile_fields['jabber']);

    return $profile_fields;
}

function get_nightly_build ($type = 'builds') {
    if (!class_exists('SyncWebKitNightlyBuilds'))
        return false;

    $WebKitBuilds = SyncWebKitNightlyBuilds::object();
    $build = $WebKitBuilds->latest($type);
    return $build;
}

function get_nightly_source () {
    return get_nightly_build('source');
}

function get_nightly_archives ($limit) {
    if (!class_exists('SyncWebKitNightlyBuilds'))
        return array();

    $WebKitBuilds = SyncWebKitNightlyBuilds::object();
    $builds = $WebKitBuilds->records('builds', $limit);
    return (array)$builds;
}

function get_nightly_builds_json () {
    if (!class_exists('SyncWebKitNightlyBuilds'))
        return '';

    $WebKitBuilds = SyncWebKitNightlyBuilds::object();
    $records = $WebKitBuilds->records('builds', 100000);
    $builds = array();
    foreach ( $records as $build ) {
        $builds[] = $build[0];
    }
    $json = json_encode($builds);
    return empty($json) ? "''" : $json;
}

add_filter('user_contactmethods', function ($fields) {
    // Add Twitter field to user profiles
    $fields['twitter'] = 'Twitter Handle';

    // Remove unused social networks
    unset($fields['aim']);
    unset($fields['yim']);
    unset($fields['jabber']);

    return $fields;
});

add_action('init', function () {
    register_sidebar(array(
        'name'=> 'Home Tiles',
        'id' => 'tiles',
        'before_widget' => '',
        'after_widget' => '',
        'before_title' => '',
        'after_title' => '',
    ));
} );

// Start Page internal rewrite handling
add_action('after_setup_theme', function () {
    add_rewrite_rule(
        'nightly/start/([^/]+)/([0-9]+)/?$',
        'index.php?pagename=nightly/start&nightly_branch=$matches[1]&nightly_build=$matches[2]',
        'top'
    );
});

add_filter('query_vars', function( $query_vars ) {
    $query_vars[] = 'nightly_build';
    $query_vars[] = 'nightly_branch';
    return $query_vars;
});

add_filter('the_title', function( $title ) {
    if ( is_admin() ) return $title;
    if ( is_feed() ) return $title;

    $title = str_replace(": ", ": <br>", $title);

    $nowrap_strings = array();
    if ($nowrap_setting = get_option("webkit_org_nowrap_strings")) {
        $nowrap_strings = explode("\n", $nowrap_setting);
    } else add_option("webkit_org_nowrap_strings", "\n");

    foreach ($nowrap_strings as $token) {
        $nobreak = str_replace(" ", " ", trim($token));
        $title = str_replace(trim($token), $nobreak, $title);
    }

    return $title;
});

// For RSS feeds, convert relative URIs to absolute
add_filter('the_content', function($content) {
    if (!is_feed()) return $content;
    $base = trailingslashit(get_site_url());
    $content = preg_replace('/<a([^>]*) href="\/([^"]*)"/', '<a$1 href="' . $base . '$2"', $content);
    $content = preg_replace('/<img([^>]*) src="\/([^"]*)"/', '<img$1 src="' . $base . '$2"', $content);
    return $content;
});

add_action('wp_head', function () {
    if (!is_single()) return;

    $style = get_post_meta(get_the_ID(), 'style', true);
    if (!empty($style))
        echo '<style type="text/css">' . $style . '</style>';

    $script = get_post_meta(get_the_ID(), 'script', true);
    if (!empty($script))
        echo '<script type="text/javascript">' . $script . '</script>';
});

add_action('the_post', function($post) {
    global $pages;

    if (!(is_single() || is_page())) return;

    $content = $post->post_content;
    if (strpos($content, 'abovetitle') === false) return;
    if (strpos($content, '<img') !== 0) return;

    $post->post_title_img = substr($content, 0, strpos($content, ">\n") + 3);
    $post->post_content = str_replace($post->post_title_img, '', $content);
    $pages = array($post->post_content);
});

add_action('the_post', function($post) {
    global $pages;

    if (!(is_single() || is_page())) return;

    $foreword = get_post_meta(get_the_ID(), 'foreword', true);
    if ( ! $foreword ) return;

    $content = $post->post_content;
    // Transform Markdown
    $Markdown = WPCom_Markdown::get_instance();
    $foreword = wp_unslash( $Markdown->transform($foreword) );

    $post->post_content = '<div class="foreword">' . $foreword . '</div>' . $content;
    $pages = array($post->post_content);
});

add_filter('the_author', function($display_name) {
    $post = get_post();
    if (!(is_single() || is_page())) return;
    $byline = get_post_meta(get_the_ID(), 'byline', true);
    return empty($byline) ? $display_name : $byline;
});

function before_the_title() {
    $post = get_post();

    if ( isset($post->post_title_img) )
        echo wp_make_content_images_responsive($post->post_title_img);
}

// Hide category 41: Legacy from archives
add_filter('pre_get_posts', function ($query) {
    if ( $query->is_home() )
        $query->set('cat', '-41');
    return $query;
});

add_filter( 'get_the_excerpt', function( $excerpt ) {
    $sentences = preg_split( '/(\.|!|\?)\s/', $excerpt, 2, PREG_SPLIT_DELIM_CAPTURE );

    // if ( empty($sentences[1]) )
    //     $sentences[1] = '&hellip;';

    return $sentences[0] . $sentences[1];

});

function show_404_page() {
    status_header(404);
    return include( get_query_template( '404' ) );
}

include('widgets/post.php');
include('widgets/icon.php');
include('widgets/twitter.php');
include('widgets/page.php');

function table_of_contents() {
    if ( class_exists('WebKitTableOfContents') )
        WebKitTableOfContents::markup();
}

function has_table_of_contents() {
    if ( class_exists('WebKitTableOfContents') )
        return WebKitTableOfContents::hasIndex();
}

function table_of_contents_index( $content, $post_id ) {
    if ( ! class_exists('WebKitTableOfContents') )
        return $content;
    $content = WebKitTableOfContents::parse($content);
    WebKitTableOfContents::wp_insert_post($post_id);
    return $content;
}

function is_super_cache_enabled() {
    global $super_cache_enabled;
    return (isset($super_cache_enabled) && true === $super_cache_enabled);
}

function include_post_icons() {
    echo WebKit_Post_Icons::parse_icons();
}

function include_invert_lightness_filter() {
    include('images/invert-lightness.svg');
}

function get_post_icon() {

    $categories = get_the_category();
    if (isset($categories[0]))
        $slug = $categories[0]->slug;

    if ('web-inspector' == $slug) {
        $tags = get_the_tags();
        if (isset($tags[0]))
            $slug = $tags[0]->slug;
    }

    if (!WebKit_Post_Icons::has_icon($slug))
        return 'default';

    return $slug;
}

function tag_post_image_luminance($post_id) {
    $threshold = 128;
    $tags = array();

    // Get the image data
    $image_src = wp_get_attachment_image_src( get_post_thumbnail_id($post_id), 'small' );
    $image_url = $image_src[0];

    if ( empty($image_url) ) return $post_id;

    // detect luminence value
    $luminance = calculate_image_luminance($image_url);
    $tags = wp_get_post_tags($post_id, array('fields' => 'names'));

    if ( $luminance < $threshold )
        $tags[] = 'dark';
    elseif ( false !== ( $key = array_search('dark', $messages) ) )
        unset($tags[ $key ]);

    // Set a tag class
    if ( ! empty($tags) )
        wp_set_post_tags( $post_id, $tags, true );

    return $post_id;
}

function calculate_image_luminance($image_url) {
    if (!function_exists('ImageCreateFromString'))
        return 1;

    // Get original image dimensions
    $size = getimagesize($image_url);

    // Create image resource from source image
    $image = ImageCreateFromString(file_get_contents($image_url));

    // Allocate image resource
    $sample = ImageCreateTrueColor(1, 1);

    // Flood fill with a white background (to properly calculate luminance of PNGs with alpha)
    ImageFill($sample , 0, 0, ImageColorAllocate($sample, 255, 255, 255));

    // Downsample to 1x1 image
    ImageCopyResampled($sample, $image, 0, 0, 0, 0, 1, 1, $size[0], $size[1]);

    // Get the RGB value of the pixel
    $rgb   = ImageColorAt($sample, 0, 0);
    $red   = ($rgb >> 16) & 0xFF;
    $green = ($rgb >> 8) & 0xFF;
    $blue  = $rgb & 0xFF;

    // Calculate relative luminance value (https://en.wikipedia.org/wiki/Relative_luminance)
    return ( 0.2126 * $red + 0.7152 * $green + 0.0722 * $blue);
}

function html_select_options(array $list, $selected = null, $values = false, $append = false) {
        if ( ! is_array($list) ) return '';

        $_ = '';

        // Append the options if the selected value doesn't exist
        if ( ( ! in_array($selected, $list) && ! isset($list[ $selected ])) && $append )
            $_ .= '<option value="' . esc_attr($selected) . '">' .esc_html($selected) . '</option>';

        foreach ( $list as $value => $text ) {

            $value_attr = $selected_attr = '';

            if ( $values ) $value_attr = ' value="' . esc_attr($value) . '"';
            if ( ( $values && (string)$value === (string)$selected)
                || ( ! $values && (string)$text === (string)$selected ) )
                    $selected_attr = ' selected="selected"';

            if ( is_array($text) ) {
                $label = $value;
                $_ .= '<optgroup label="' . esc_attr($label) . '">';
                $_ .= html_select_options($text, $selected, $values);
                $_ .= '</optgroup>';
                continue;
            } else $_ .= "<option$value_attr$selected_attr>$text</option>";

        }
        return $_;
    }

add_filter('save_post', 'tag_post_image_luminance');

add_filter('next_post_link', function ( $format ) {
    return str_replace('href=', 'class="page-numbers next-post" href=', $format);
});
add_filter('previous_post_link', function ( $format ) {
    return str_replace('href=', 'class="page-numbers prev-post" href=', $format);
});

// Queue global scripts
add_action( 'wp_enqueue_scripts', function () {
    wp_register_script(
        'theme-global',
        get_stylesheet_directory_uri() . '/scripts/global.js',
        false,
        '1.0',
        true
    );

    wp_enqueue_script( 'theme-global' );

} );

class Responsive_Toggle_Walker_Nav_Menu extends Walker_Nav_Menu {

    private $toggleid = null;

    public function start_lvl( &$output, $depth = 0, $args = array() ) {
        $output .= "\n" . str_repeat("\t", $depth);
        $classes = array("sub-menu");
        if ( 0 == $depth ) {
            $classes[] = "sub-menu-layer";
        }
        $id = ( 0 == $depth ) ? " id=\"sub-menu-for-{$this->toggleid}\"" : '';
        $class_names = esc_attr(join( ' ', $classes ));
        $output .= "<ul class=\"$class_names\" role=\"menu\"$id>\n";
    }

    public function end_lvl( &$output, $depth = 0, $args = array() ) {
        $indent = str_repeat("\t", $depth);
        $output .= "$indent</ul>\n";
    }

    public function start_el( &$output, $item, $depth = 0, $args = array(), $id = 0 ) {

        $before = $args->link_before;
        $after = $args->link_after;

        if ( in_array('menu-item-has-children', $item->classes) && 0 == $depth ) {
            $this->toggleid = $item->ID;
            $args->before .= "<input type=\"checkbox\" id=\"toggle-{$item->ID}\" class=\"menu-toggle\" />";
            $args->link_before = "<label for=\"toggle-{$item->ID}\" class=\"label-toggle\">" . $args->link_before;
            $args->link_after .= "</label>";
            $item->url = '#nav-sub-menu';
        } elseif ( in_array('menu-item-has-children', $item->classes) && 1 == $depth ) {
            // $item->role = "presentation";
        } else $toggleid = null;

        $indent = ( $depth ) ? str_repeat( "\t", $depth ) : '';

        $classes = empty( $item->classes ) ? array() : (array) $item->classes;
        $classes[] = 'menu-item-' . $item->ID;

        $class_names = join( ' ', apply_filters( 'nav_menu_css_class', array_filter( $classes ), $item, $args, $depth ) );
        $class_names = $class_names ? ' class="' . esc_attr( $class_names ) . '"' : '';

        $id = apply_filters( 'nav_menu_item_id', 'menu-item-'. $item->ID, $item, $args, $depth );
        $id = $id ? ' id="' . esc_attr( $id ) . '"' : '';

        $output .= $indent . '<li' . $id . $class_names . '>';

        $atts = array();
        $atts['title']  = ! empty( $item->attr_title ) ? $item->attr_title : '';
        $atts['target'] = ! empty( $item->target )     ? $item->target     : '';
        $atts['rel']    = ! empty( $item->xfn )        ? $item->xfn        : '';
        $atts['href']   = ! empty( $item->url )        ? $item->url        : '';
        $atts['role']   = ! empty( $item->role )       ? $item->role       : '';

        if ( in_array('menu-item-has-children', $item->classes) && 0 == $depth ) {
            $atts['aria-haspopup'] = "true";
            $atts['aria-owns'] = 'sub-menu-for-' . $item->ID;
            $atts['aria-controls'] = 'sub-menu-for-' . $item->ID;
            $atts['aria-expanded'] = 'true';
        }

        $atts = apply_filters( 'nav_menu_link_attributes', $atts, $item, $args, $depth );

        $attributes = '';
        foreach ( $atts as $attr => $value ) {
            if ( ! empty( $value ) ) {
                $value = ( 'href' === $attr ) ? esc_url( $value ) : esc_attr( $value );
                $attributes .= ' ' . $attr . '="' . $value . '"';
            }
        }

        $item_output = $args->before;
        $item_output .= '<a'. $attributes .'>';
        $item_output .= $args->link_before . apply_filters( 'the_title', $item->title, $item->ID ) . $args->link_after;
        $item_output .= '</a>';
        $item_output .= $args->after;

        $output .= apply_filters( 'walker_nav_menu_start_el', $item_output, $item, $depth, $args );

        $args->link_before = $before;
        $args->link_after = $after;
    }

}

class WebKit_Post_Icons {

    private static $registry = array();

    public static function parse_icons() {
        if (!empty(self::$registry))
            return '';

        $svg_string = file_get_contents(get_stylesheet_directory() . '/images/icons.svg');
        $svg = new SimpleXMLElement( $svg_string );
        $svg->registerXPathNamespace('svg', 'http://www.w3.org/2000/svg');

        $matches = $svg->xpath('//svg:symbol/@id');
        foreach ($matches as $symbol)
            self::$registry[(string)$symbol['id']] = true;

        return $svg_string;
    }

    public static function has_icon($id) {
        return isset(self::$registry[$id]);
    }

}

class Front_Page_Posts {

    private static $object;     // Singleton instance

    private static $wp_query;   // WP_Query instance

    public static function &object () {
        if ( ! self::$object instanceof self )
            self::$object = new self;

        if ( empty(self::$wp_query) )
            self::$wp_query = new WP_Query(array('post_type' => 'post', 'posts_per_page' => 16));

        return self::$object;
    }

    public static function WP_Query() {
        return self::$wp_query;
    }

}

class WebKit_Nightly_Survey {

    const COOKIE_PREFIX = 'webkitnightlysurvey_';
    const DATA_SETTING_NAME = 'webkit_nightly_survey_data';
    const SURVEY_FILENAME = 'survey.json';

    public function add_widget() {

        wp_add_dashboard_widget(
            'webkit_nightly_survey_results', // Widget slug
            'WebKit Nightly Survey Results', // Title
            array($this, 'display_widget')   // Display function
        );

        if (isset($_GET['wksurvey']) && $_GET['wksurvey'] == 'download')
            $this->download() && exit;

    }

    public function display_widget() {
        $data = get_option(self::DATA_SETTING_NAME);
        $styles = "
            <style>
            .survey_table .question {
                text-align: left;
            }
            .survey_table .score {
                font-size: 15px;
                min-width: 30px;
                text-align: right;
                vertical-align: top;
                padding-right: 10px;
            }
            .survey_table .others {
                font-style: italic;
            }
            </style>
        ";

        $response_limit = 10;
        $table = '';
        foreach ($data as $question => $responses) {
            $question_row = '<tr><th colspan="2" class="question">' . $question . '</th></tr>';
            $response_rows = '';
            arsort($responses);
            $response_count = 0;
            $total_responses = count($responses);
            foreach ($responses as $response => $votes) {
                $response_rows .= '<tr><td class="score">' . intval($votes) . '</td><td class="response">' . stripslashes($response) . '</td></tr>';
                if ( ++$response_count >= $response_limit && $response_count < $total_responses) {
                    $response_rows .= '<tr><td class="score others">' . intval($total_responses - $response_limit) . '</td><td class="response others">more "other" responses</td></tr>';
                    break;
                }
            }
            $table .= $question_row . $response_rows;
        }
        echo $styles;
        echo '<table class="survey_table">' . $table . '</table>';
        echo '<p class="textright"><a class="button button-primary" href="' . add_query_arg('wksurvey','download',admin_url()) . '">Download Results as CSV</a></p>';
    }

    private function download() {
        $data = get_option(self::DATA_SETTING_NAME);
        $table = '';

        header('Content-type: text/csv; charset=UTF-8');
        header('Content-Disposition: attachment; filename="webkit_nightly_survey.csv"');
        header('Content-Description: Delivered by webkit.org');
        header('Cache-Control: maxage=1');
        header('Pragma: public');

        foreach ($data as $question => $responses) {
            $question_row = 'Score,' . $question . "\n";
            $response_rows = '';
            arsort($responses);
            foreach ($responses as $response => $votes) {
                $response_rows .= intval($votes) . ',' . stripslashes($response) . "\n";
            }
            $table .= $question_row . $response_rows . "\n";
        }
        echo $table;

        exit;
    }

    public function process() {
        if ( empty($_POST) ) return;

        if ( ! wp_verify_nonce($_POST['_nonce'], self::SURVEY_FILENAME) )
            wp_die('Invalid WebKit Nightly Survey submission.');

        $score = $data = get_option(self::DATA_SETTING_NAME);
        $Survey = self::survey();
        foreach ($Survey as $id => $SurveyQuestion) {
            if ( ! isset($score[ $SurveyQuestion->question ]) )
                $score[ $SurveyQuestion->question ] = array();
            $response = $_POST['questions'][ $id ];
            $answer = empty($SurveyQuestion->responses[ $response ]) ? $response : $SurveyQuestion->responses[ $response ];
            if ($answer == 'Other:')
                $answer = $_POST['other'][ $id ];
            $score[ $SurveyQuestion->question ][ $answer ]++;
        }

        if ( $data === false ) {
            $deprecated = null;
            $autoload = 'no';
            add_option(self::DATA_SETTING_NAME, $score, $deprecated, $autoload);
        } else {
            update_option(self::DATA_SETTING_NAME, $score);
        }

        $httponly = false;
        $secure = false;
        setcookie(self::cookie_name(), 1, time() + YEAR_IN_SECONDS, '/', WP_HOST, $secure, $httponly );
        $_COOKIE[ self::cookie_name() ] = 1;
    }

    public static function responded() {
        return isset($_COOKIE[ self::cookie_name() ]);
    }

    private static function cookie_name() {
        return self::COOKIE_PREFIX . COOKIEHASH;
    }

    public static function survey_file() {
        return dirname(__FILE__) . '/' . self::SURVEY_FILENAME;
    }

    public static function form_nonce() {
        return '<input type="hidden" name="_nonce" value="' . wp_create_nonce( self::SURVEY_FILENAME ) . '">';
    }

    public static function survey () {
        $survey_json = file_get_contents(self::survey_file());
        $Decoded = json_decode($survey_json);
        return isset($Decoded->survey) ? $Decoded->survey : array();
    }

}