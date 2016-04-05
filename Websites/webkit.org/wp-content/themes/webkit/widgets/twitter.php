<?php
/**
 * WebKitTwitterTileWidget class
 * A WordPress widget to show a Twitter tile on the home page
 **/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

if ( ! class_exists('WP_Widget') ) return;

class WebKitTwitterTileWidget extends WebKitPostTileWidget {

    const CACHEKEY = 'webkit_twitter_feed';

    function __construct() {
        parent::WP_Widget(false,
            __('Twitter Tile'),
            array('description' => __('Twitter tile for the home page'))
        );
    }

    function widget( array $args, array $options ) {

        if ( ! empty($args) )
            extract($args, EXTR_SKIP);

        $Tweet = $this->tweet();

        // Show "Follow @webkit" instead of tweet for empty text
        if ( empty($Tweet) || empty($Tweet->text) )
            return $this->follow_markup($options);

        // Show "Follow @webkit" instead of tweet for blog URLs
        if ( ! empty($Tweet->entities)
                && ! empty($Tweet->entities->urls)
                && count($Tweet->entities->urls) == 1
                && preg_match('!webkit.org/blog/\d+/!', $Tweet->entities->urls[0]->expanded_url) == 1)
            return $this->follow_markup($options);

        $classes = array('tile', 'third-tile', 'twitter-tile');

        $text = (string)$options['text'];

        if ( ! empty($Tweet->text) )
            $text = $Tweet->text;

        if ( ! empty($options['url']) )
            $url = (string)$options['url'];

        // Expand URLs
        foreach ( $Tweet->entities->urls as $entry ) {
            $expanded = '<a href="' . esc_url($entry->expanded_url) . '">'
                                 . $entry->display_url . '</a>';
            $text = str_replace($entry->url, $expanded, $text);
        }

        $text = preg_replace('/RT @[^:]+:\s+/', '', $text, 1);

        if ( empty($Tweet->entities) || empty($Tweet->entities->media) ) {

            $Image = $Tweet->entities->media[0];
            if ( empty($Image) ) $classes[] = 'text-only';

            // Strip media links
            foreach ( $Tweet->entities->media as $entry ) {
                $text = str_replace($entry->url, '', $text);
            }
        }

        ?>
        <div class="<?php echo esc_attr(join(' ', $classes)); ?>">
            <a href="http://twitter.com/webkit/status/<?php echo esc_attr($Tweet->id); ?>" class="tile-link">Clickable link to tweet</a>
            <div class="tile-content">
            <?php if ( ! empty($Image) ): ?>
            <img src="<?php echo esc_url($Image->media_url_https); ?>">
            <?php else: ?>
            <?php endif;?>
                <p><?php echo $text; ?></p>

            </div>
            <ul class="twitter-controls">
                <li><a href="https://twitter.com/webkit" target="twitter-modal"><span class="twitter-icon">Twitter</span> @webkit</a></li>
                <li><a href="https://twitter.com/intent/tweet?in-reply-to=<?php echo esc_attr($Tweet->id); ?>" class="twitter-icon reply-icon" target="twitter-modal">Reply</a></li>
                <li><a href="https://twitter.com/intent/retweet?tweet_id=<?php echo esc_attr($Tweet->id); ?>" class="twitter-icon retweet-icon" target="twitter-modal">Retweet</a></li>
                <li><a href="https://twitter.com/intent/favorite?tweet_id=<?php echo esc_attr($Tweet->id); ?>" class="twitter-icon favorite-icon" target="twitter-modal">Favorite</a></li>
            </ul>
        </div>
        <?php
    }

    function follow_markup ($options) {

        $url = 'https://twitter.com/intent/follow?screen_name=webkit';
        $classes = array('tile', 'third-tile', 'twitter-tile');

        ?>
        <div class="<?php echo esc_attr(join(' ', $classes)); ?>">
            <a class="tile-link" href="<?php echo esc_url($url); ?>">Clickable link</a>
            <div class="icon twitter-icon"></div>
            <h2><?php echo nl2br(esc_html($options['text'])); ?></h2>
        </div>
        <?php
    }

    function form( array $options ) {
        ?>
        <p><label for="<?php echo $this->get_field_id('text'); ?>"><?php _e('Text'); ?></label>
        <textarea type="text" name="<?php echo $this->get_field_name('text'); ?>" id="<?php echo $this->get_field_id('text'); ?>" class="widefat"><?php echo $options['text']; ?></textarea></p>
        <?php
    }

    function tweet () {

        // Prefer pushed tweet
        if ( class_exists('TweetListener') ) {
            $TweetListener = TweetListener::object();
            return $TweetListener->tweet();
        }

        // Poll for tweet
        if ( false !== ( $cached = get_transient(self::CACHEKEY) ) )
            return json_decode($cached);

        $endpoint = 'https://api.twitter.com/1.1/statuses/user_timeline.json';
        $parameters = array();
        $options = array(
            'method' => 'GET',
        );

        $oauth_consumer_key = 'ypSvRp37vmyt3ldMPhs0e62c9';
        $oauth_consumer_key_secret = 'mLuYD3AjUehUZKOgQIICA5Na69te45aSJTkdIDTGSg4cfHd6Lz';
        $oauth_token = '14315023-7pHbcI5bk2QZhNiHR9uFudaksBzMPubEuOEmYj7YQ';
        $oauth_token_secret = '0K9T9znxG4S9PUGunYZ5LwyKL9AR6v3eAXp6WKY2oi7Bg';
        $oauth_timestamp = time();
        $oauth_nonce = sha1(rand() . $oauth_timestamp);

        $fields = array(
            'oauth_consumer_key' => $oauth_consumer_key,
            'oauth_nonce' => sha1(rand() . time()),
            'oauth_signature_method' => 'HMAC-SHA1',
            'oauth_timestamp' => time(),
            'oauth_token' => $oauth_token,
            'oauth_version' => '1.0',
        );
        $fields = array_merge($parameters, $fields);

        $base = array(
            'GET', $endpoint, http_build_query($fields, '', '&')
        );
        $base = join('&', array_map('rawurlencode', $base));


        $key = array(
            $oauth_consumer_key_secret, $oauth_token_secret
        );
        $key = join('&', array_map('rawurlencode', $key));

        $signature = base64_encode( hash_hmac('sha1', $base, $key, true) );

        $oauth = array(
            'oauth_consumer_key' => $oauth_consumer_key,
            'oauth_nonce' => $fields['oauth_nonce'],
            'oauth_signature' => $signature,
            'oauth_signature_method' => 'HMAC-SHA1',
            'oauth_timestamp' => $fields['oauth_timestamp'],
            'oauth_token' => $oauth_token,
            'oauth_version' => '1.0',
        );
        $oauth = array_map(create_function('$h','return "\"$h\"";'), $oauth);

        $oauth = http_build_query($oauth, '', ', ');
        $oauth = str_replace('%22', '"', $oauth);

        $headers = array(
            'Authorization' => "OAuth $oauth"
        );

        $options['headers'] = $headers;

        $response = wp_remote_get($endpoint, $options);

        if ( is_a($response, 'WP_Error') )
            return false;

        if ( 200 == $response['response']['code'] && ! empty($response['body']) ) {
            $body = json_decode($response['body']);
            $data = $body[0];
            set_transient(self::CACHEKEY, json_encode($data), DAY_IN_SECONDS/2);
            return $data;
        }

        return false;
    }

} // END class WebKitTwitterTileWidget

register_widget('WebKitTwitterTileWidget');