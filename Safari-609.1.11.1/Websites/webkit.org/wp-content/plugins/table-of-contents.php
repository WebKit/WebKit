<?php
/*
Plugin Name: WebKit Table of Contents
Plugin URI: http://webkit.org
Description: Builds a table of contents with navigation anchors from headings
Version: 1.0
Author: Jonathan Davis
*/

WebKitTableOfContents::init();

class WebKitTableOfContents {

    private static $editing = false;
    private static $toc = array();
    private static $attr_regex = '\{((?:[ ]*[#.][-_:a-zA-Z0-9]+){1,})[ ]*\}';

    public function init() {
        add_filter( 'wp_insert_post_data', array( 'WebKitTableOfContents', 'wp_insert_post_data' ), 20, 2 );
        add_action( 'wp_insert_post', array( 'WebKitTableOfContents', 'wp_insert_post' ) );
    }

    public static function hasIndex() {
        $toc = get_post_meta( get_the_ID(), 'toc', true );
        array_walk($toc, array('WebKitTableOfContents', 'filterIndex'));
        return ( ! empty(self::$toc) && count(self::$toc) > 2 );
    }

    public static function filterIndex($value, $key) {
        list($level, $anchor) = explode('::', $key);
        if ( $level < 3 ) self::$toc[ $key ] = $value;
    }

    public static function renderMarkup() {
        if ( ! is_page() ) return;

        if ( empty(self::$toc) || ! self::hasIndex() )
            return;

        $depth = 0;
        $parent = 0;
        $markup = '<input type="checkbox" id="table-of-contents-toggle" class="menu-toggle"><menu class="table-of-contents"><menuitem class="list"><h6><label for="table-of-contents-toggle">Contents</label></h6>';

        foreach ( self::$toc as $name => $heading ) {
            list($level, $anchor) = explode('::', $name);
            if ( empty($level) || $level > 3 ) continue;

            if ( $level < $depth ) {
                $markup .= str_repeat('</ul></li>', $depth - $level);
                $depth = $level;
            } elseif ( $level == $depth )
                $markup .= '</li>';



            if ( $level > $depth ) {
                $markup .= '<ul class="level-' . $level . '">';
                $depth = $level;
                $parent = $depth;
            }

            $markup .= sprintf('<li><a href="#%s">%s</a>', $anchor, $heading);

        }
        $markup .= '</ul></menuitem></menu>';

        return $markup;
    }

    public static function markup() {
        echo self::renderMarkup();
    }

    public function wp_insert_post_data( $post_data, $record ) {

        if ( ! in_array($post_data['post_type'], array('page')) )
            return $post_data;

        $post_data['post_content'] = self::parse($post_data['post_content']);

        self::$editing = isset( $record['ID'] ) ? $record['ID'] : false;

        return $post_data;
    }

    public static function wp_insert_post( $post_id ) {
        if ( empty(self::$toc) ) return;
        if ( self::$editing && self::$editing != $post_id ) return;
        update_post_meta($post_id, 'toc', self::$toc);
    }

    public static function parse( $content ) {
        $markup = preg_replace_callback('{
                ^<h([1-6])[^>]*>(.+?)<\/h[1-6]>* # HTML tags
            }xm',
            array('WebKitTableOfContents', 'index'),
            $content
        );

        return $markup;
    }

    public static function index( $matches ) {
        list($marked, $level, $heading) = $matches;
        $anchor = sanitize_title_with_dashes($heading);
        self::$toc[ "$level::$anchor" ] = $heading;
        return sprintf('<h%2$d><a name="%1$s"></a>%3$s</h%2$d>', $anchor, $level, $heading);
    }
}