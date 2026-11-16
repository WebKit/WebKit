<?php
/*
Plugin Name: Disable Public APIs
Description: Disable all public APIs
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

// Disable XML-RPC
add_filter('xmlrpc_enabled', '__return_false', 100);

// Disable REST API links in wp_head
remove_action('wp_head', 'rest_output_link_wp_head', 10);
remove_action('xmlrpc_rsd_apis', 'rest_output_rsd');

// Disable REST API Link HTTP header
remove_action('template_redirect', 'rest_output_link_header', 100);

add_filter('rest_authentication_errors', function ($access) {
    if (is_user_logged_in())
        return $access;
    return new WP_Error('rest_login_required', __('Access denied.'), array('status' => rest_authorization_required_code()));
});