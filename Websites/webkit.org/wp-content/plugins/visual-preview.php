<?php
/*
Plugin Name: Visual Editor Read-Only
Description: Changes the visual editor into a visual preview only
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/
add_filter( 'tiny_mce_before_init', function( $args ) {
    $args['readonly'] = 1;
    return $args;
} );