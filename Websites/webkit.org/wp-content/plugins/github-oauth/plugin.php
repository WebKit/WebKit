<?php
/*
Plugin Name: GitHub OAuth Sign-in
Description: Provides federated authentication via Discord
Version:     1.0
Author:      Apple, Inc.
Author URI:  https://webkit.org
*/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

if (!defined('WK_GITHUB_CLIENTID'))
    define('WK_GITHUB_CLIENTID','');

if (!defined('WK_GITHUB_SECRET'))
    define('WK_GITHUB_SECRET','');

include('api.php');
include('oauth.php');

$GitHubOAuthPlugin = new GitHubOAuthPlugin();

class GitHubOAuthPlugin {
    const ACCESS_TOKEN_COOKIE_NAME = 'gwkt';
    const REFERRER_KEY = 'auth_referrer';
    const REDIRECT_REQUEST = 'auth-gateway';
    const DEBUG = false;

    static $Auth;
    static $API;

    function __construct() {
        // Initialize API
        self::$API = new GitHubAPI();

        // Initialize OAuth2
        self::$Auth = new GitHubOAuth2(WK_GITHUB_CLIENTID, WK_GITHUB_SECRET, home_url(self::REDIRECT_REQUEST));
        self::$Auth->set_scopes(['read:user', 'user:email']);

        add_action('parse_request', array($this, 'request'));
    }

    function request ($wp) {
        if ($wp->request == self::REDIRECT_REQUEST) {
            $referrer = filter_input(INPUT_COOKIE, self::REFERRER_KEY);
            $auth_referrer = !empty($referrer) ? $referrer : '/';
            $redirect =  home_url(self::$Auth->auth() ? add_query_arg([], $auth_referrer) : null);
            header("Location: $redirect");
            exit;
        }

        if ($wp->request == 'login') {
            self::save_cookie(self::REFERRER_KEY, filter_input(INPUT_SERVER, 'HTTP_REFERER'));
            self::$Auth->auth();
            header("Location: /");
            exit;
        }

        if ($wp->request == 'logout') {
            self::$Auth->reset_access();
            header("Location: /");
            exit;
        }
    }

    static function request_auth() {
        global $wp;
        self::save_cookie(self::REFERRER_KEY, $wp->request);
        return self::$Auth->auth();
    }

    static function get_access_token() {
        return filter_input(INPUT_COOKIE, self::ACCESS_TOKEN_COOKIE_NAME, FILTER_SANITIZE_STRING);
    }

    static function save_cookie($name, $value) {
        if (empty($name))
            $name = self::ACCESS_TOKEN_COOKIE_NAME;

        $expire = time() + DAY_IN_SECONDS;
        $path = "/";
        $domain = COOKIE_DOMAIN;
        $secure = true;
        $httponly = true;

        setcookie($name, $value, $expire, $path, $domain, $secure, $httponly);
    }

    static function is_contributor() {
        $API = self::$API;
        $Teams = $API->get_teams();

        $contributor = false;
        foreach ($Teams as $Team) {
            if ($Team->name == "Contributors" && $Team->organization->login == "WebKit") {
                $contributor = true;
                break;
            }
        }

        return $contributor;
    }

    static function log($string) {
        if (!GitHubOAuthPlugin::DEBUG) return;
        error_log($string);
    }

}