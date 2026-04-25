<?php

defined( 'WPINC' ) || header( 'HTTP/1.1 403' ) & exit; // Prevent direct access

class GitHubAPI {

    const API_ENDPOINT = 'https://api.github.com';

    private $token;

    public function __construct() {
        $this->token = GitHubOAuthPlugin::get_access_token();
    }

    public function __set($property, $value) {
        if (property_exists($this, $property))
            $this->$property = $value;
        return $this;
    }

    protected function call ($endpoint, $params = array()) {
        $endpoint = add_query_arg($params, self::API_ENDPOINT . $endpoint);

        $API = wp_remote_get($endpoint, array(
            'headers' => array(
                'Authorization' => 'token ' . $this->token,
                'Accept' => 'application/json'
            )
        ));
        $response = wp_remote_retrieve_body($API);

        return json_decode($response);
    }

    public function get_user() {
        return $this->call('/user');
    }

    public function get_repo() {
        return $this->call('/repos/WebKit/WebKit');
    }

    public function get_collab($username) {
        echo "/repos/WebKit/WebKit/collaborators/$username";
        return $this->call("/repos/WebKit/WebKit/collaborators/$username");
    }

    public function get_hovercard($username) {
        return $this->call("/users/$username/hovercard", [
            'subject_type' => 'repository',
            'subject_id' => '320636477'
        ]);
    }

    public function get_teams() {
        return $this->call('/user/teams');
    }

    protected function is_bot_request() {
        if (preg_match(
            '/(bot|Google-Apps-Script|WordPress)/i', $_SERVER['HTTP_USER_AGENT']))
            return true;
        if ("/xmlrpc.php" == $_SERVER['REQUEST_URI'])
            return true;
        return false;
    }

}