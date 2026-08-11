<?php

defined( 'WPINC' ) || header( 'HTTP/1.1 403' ) & exit; // Prevent direct access

class GitHubOAuth2 extends GitHubAPI {

    const AUTHORIZE_ENDPOINT = 'https://github.com/login/oauth/authorize';
    const TOKEN_ENDPOINT = 'https://github.com/login/oauth/access_token';

    const REFRESH_PERIOD = 300; // 5 minute grace period

    private $client_id = false;
    private $secret = false;
    private $redirect_url = false;
    private $access_token = false;
    private $expiry = false;
    private $scopes = array();

    public function __construct($client_id, $secret, $redirect_url) {
        $this->client_id = $client_id;
        $this->secret = $secret;
        $this->redirect_url = $redirect_url;

        if (!empty(filter_input(INPUT_GET, 'reset_access')))
            $this->reset_access();
        $this->access_token = GitHubOAuthPlugin::get_access_token();
    }

    public function auth() {
        // Check for access
        if ($this->has_access())
            return true;

        // Handle redirect or loading the token
        if (!$this->redirected()) {
            $this->redirect();
        } elseif ($this->load_token() === true) {
            return true;
        }

        return false;
    }

    public function has_access() {
        return !empty($this->access_token);
    }

    public function reset_access() {
        GitHubOAuthPlugin::save_cookie(GitHubOAuthPlugin::ACCESS_TOKEN_COOKIE_NAME, '');
        GitHubOAuthPlugin::save_cookie('oauth2state', '');
        GitHubOAuthPlugin::save_cookie('auth_referrer', '');

        $this->access_token = null;
    }

    public function redirected() {
        $code = filter_input(INPUT_GET, 'code');
        return !is_null($code);
    }

    public function set_scopes($scopes) {
        $this->scopes = $scopes;
    }

    private function redirect() {
        if ($this->is_bot_request())
            return false;

        $stateToken = self::generate_token();
        GitHubOAuthPlugin::save_cookie('oauth2state', $stateToken);

        $redirect = add_query_arg(array(
            'client_id' => $this->client_id,
            'redirect_uri' => urlencode($this->redirect_url),
            'response_type' => 'code',
            'scope' => join('%20', $this->scopes),
            'state' => $stateToken
        ), self::AUTHORIZE_ENDPOINT);

        header("Location: $redirect");
        exit;
    }

    private function load_token() {
        // Avoid authenticating bots
        if ($this->is_bot_request())
            return false;

        $saved_state = filter_input(INPUT_COOKIE, 'oauth2state');

        $state = filter_input(INPUT_GET, 'state');
        if (empty($saved_state) || empty($state) || $state !== $saved_state) {
            GitHubOAuthPlugin::save_cookie('oauth2state', '');
            return 'Invalid state';
        }

        $code = filter_input(INPUT_GET, 'code');
        $response = $this->post(self::TOKEN_ENDPOINT, [], [
            'client_id' => $this->client_id,
            'client_secret' => $this->secret,
            'code' => $code,
            'redirect_uri' => $this->redirect_url
        ]);

        if ($response === null)
            return 'Invalid state';
        
        if (isset($response->error_description))
            return $response->error_description;

        if (!empty($response->access_token)) {
            GitHubOAuthPlugin::save_cookie('', $response->access_token);
            $this->access_token = $response->access_token;
        }

        return true;
    }

    protected function post ($endpoint, $params = array(), $data = false) {
        $endpoint = add_query_arg($params, $endpoint);

        $API = wp_remote_post($endpoint, array(
            'headers' => array(
                'Accept' => 'application/json'
            ),
            'body' => $data
        ));

        $response = wp_remote_retrieve_body($API);
        return json_decode($response);
    }

    private static function generate_token() {
        $chars = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        $chars_length = strlen($chars);
        $string = "";
        for ($i = 0; $i < 20; $i++)
            $string .= $chars[rand(0, $chars_length - 1)];
        return $string;
    }

}