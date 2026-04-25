<?php
/*
Plugin Name: Tweet Listener
Description: Listens for Tweets from @webkit
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

TweetListener::object();

class TweetListener {

    private static $object = null;

    const AUTH_TOKEN = TWITTER_LISTENER_AUTH;
    const DATA_KEY = 'recent_tweet';

    public static function object() {
        if (self::$object === null)
            self::$object = new self();
        return self::$object;
    }

    private function __construct() {
        add_action('wp_ajax_nopriv_tweet', array($this, 'listen'));
        add_action('wp_ajax_tweet', array($this, 'listen'));
    }

    public function listen() {

        if (!isset($_POST['token']) || $_POST['token'] != TWITTER_LISTENER_AUTH)
            wp_die();

        $defaults = array(
            'text' => '',
            'username' => '@webkit',
            'link' => '',
            'created' => '',
            'embed' => ''
        );
        $data = array_merge($defaults, $_POST);

        update_option(self::DATA_KEY, $data);
        wp_die();
    }

    public function tweet() {
        $data = (object)get_option(self::DATA_KEY);

        // Setup compatible Tweet structure

        $Tweet = new StdClass();
        $Tweet->id = substr($data->link, strrpos($data->link, '/') + 1);
        $Tweet->text = stripslashes($data->text);

        $Tweet->entities = new StdClass();
        $Tweet->entities->urls = array();
        $Tweet->entities->media = array();

        // Search for embedded images
        if ( preg_match_all('/<a href="([^"]+)">(.*?)<\/a>/', stripslashes($data->embed), $matches, PREG_SET_ORDER) ) {
            foreach ($matches as $embedlink) {
                $URL = new StdClass();
                $URL->expanded_url = $URL->display_url = $URL->url =$embedlink[1];
                $URL->display_url = str_replace(array('https://','http://'), '', $URL->display_url);
                $Tweet->entities->urls[] = $URL;
            }
        }

        return $Tweet;
    }

} // end class TweetListener