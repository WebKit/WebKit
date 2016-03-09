<?php

// Declare theme features
add_theme_support( 'post-thumbnails' );

function register_menus() {
}
add_action( 'init', function () {
    register_nav_menu('site-nav', __( 'Site Navigation' ));
    register_nav_menu('footer-nav', __( 'Footer Navigation' ));
    register_nav_menu('sitemap', __( 'Site Map Page' ));
} );

function modify_contact_methods($profile_fields) {

    // Add new fields
    $profile_fields['twitter'] = 'Twitter Handle';
    unset($profile_fields['aim']);
    unset($profile_fields['yim']);
    unset($profile_fields['jabber']);

    return $profile_fields;
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

add_filter('the_title', function( $title ) {
    if ( is_admin() ) return $title;
    $title = str_replace(": ", ":<br>", $title);
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

// Hide category 41: Legacy from archives
add_filter('pre_get_posts', function ($query) {
    if ( $query->is_home() )
        $query->set('cat', '-41');
    return $query;
});

include('widgets/post.php');
include('widgets/icon.php');
include('widgets/twitter.php');
include('widgets/page.php');

add_filter( 'get_the_excerpt', function( $excerpt ) {
    $sentences = preg_split( '/(\.|!|\?)\s/', $excerpt, 2, PREG_SPLIT_DELIM_CAPTURE );

    // if ( empty($sentences[1]) )
    //     $sentences[1] = '&hellip;';

    return $sentences[0] . $sentences[1];

});

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

function tag_post_image_luminance( $post_id ) {
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
                $_ .= self::menuoptions($text, $selected, $values);
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

class Front_Page_Posts {

    private static $object;     // Singleton instance

    private static $wp_query;   // WP_Query instance

    public static function &object () {
        if ( ! self::$object instanceof self )
            self::$object = new self;

        if ( empty(self::$wp_query) )
            self::$wp_query = new WP_Query(array('post_type' => 'post'));

        return self::$object;
    }

    public static function WP_Query() {
        return self::$wp_query;
    }

}