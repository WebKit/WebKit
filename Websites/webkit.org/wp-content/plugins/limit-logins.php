<?php
/*
Plugin Name: Limit Logins
Description: Limit brute force login attempts
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

if (!defined('FAILED_LOGIN_LIMIT'))
    define('FAILED_LOGIN_LIMIT', 3);

function get_limit_logins_transient_key($username) {
    return 'login_attempts_' . md5($username);
}

add_action('wp_login_failed', function ($username) {
    $transient_key = get_limit_logins_transient_key($username);
    $login_attempts = intval(get_transient($transient_key));
    if ($login_attempts++ <= FAILED_LOGIN_LIMIT) 
        set_transient($transient_key, $login_attempts, 300);
});

add_filter('authenticate', function ($user, $username, $password) {
    $transient_key = get_limit_logins_transient_key($username);
    $login_attempts = intval(get_transient($transient_key));
    if ($login_attempts >= FAILED_LOGIN_LIMIT) {
        $wait_time = human_time_diff(time(), get_option('_transient_timeout_' . $transient_key));
        return new WP_Error('failed_login_limit', sprintf(__('Login attempt limit reached. Wait %1$s before trying again.'), $wait_time));
    }
    return $user;
}, 100, 3);