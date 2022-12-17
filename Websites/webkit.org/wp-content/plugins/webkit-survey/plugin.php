<?php
/*
Plugin Name: WebKit Surveys
Description: Create surveys to include on pages or posts
Version:     1.0
Author:      Jonathan Davis
Author URI:  http://webkit.org
*/

defined('WPINC') || header('HTTP/1.1 403') & exit; // Prevent direct access

class WebKit_Survey {
    
    static $responded = false;   // Track when a user has responded
    static $post_update = false; // Track when the host post/page is updated
    
    public static function init() {
        add_action('init', [__CLASS__, 'register_shortcodes']);
        add_action('post_updated', [__CLASS__, 'process_survey'], 10, 3);
        add_action('wp', [__CLASS__, 'process_vote']);
        
        add_action( 'add_meta_boxes', [__CLASS__, 'add_survey_boxes'] );
    }

    public static function register_shortcodes() {
        add_shortcode('survey', ['WebKit_Survey', 'survey_shortcode']);
    }
    
    public function process_survey($post_id, $post_after, $post_before) {
        self::$post_update = true;
        do_shortcode($post_after->post_content);
    }
    
    public function survey_shortcode($atts, $content) {
        $json_string = str_replace(array('“','”'), '"', html_entity_decode(strip_tags($content)));
        $Survey = json_decode($json_string);

        // Capture survey settings data when updating the post
        if (self::$post_update) {
            $post_id = get_the_ID();
            $Survey->status = $atts['closed'] == 'true' ? 'closed' : 'open';
            $Survey->results = $atts['visible-results'] == 'true' ? 'visible' : 'hidden';
            if (!add_post_meta($post_id, 'webkit_survey_settings', $Survey, true))
                update_post_meta($post_id, 'webkit_survey_settings', $Survey);
            delete_transient(sha1(json_encode($Survey->survey)));
        }

        $survey_template = dirname(__FILE__) . '/survey.php';
        $results_template = dirname(__FILE__) . '/results.php';
        
        ob_start();
        echo '<div class="webkit-survey">';
        include($results_template);
        if ($atts['closed'] == "false" && !self::responded())
            include($survey_template);
        echo '</div';
        return ob_get_clean();
    }

    public function add_survey_boxes() {
        $post_id = get_the_ID();
        $settings = get_post_meta($post_id, 'webkit_survey_settings');
        if (!$settings)
            return;

        $screens = [ 'post', 'page' ];
        foreach ( $screens as $screen ) {
            add_meta_box(
                'webkit_survey',
                'WebKit Survey',
                [__CLASS__, 'admin_box'],
                $screen
            );
        }
    }
    
    public function admin_box() {
        $post_id = get_the_ID();
        $settings = get_post_meta($post_id, 'webkit_survey_settings');
        ?>
        <style>
            :root {
                --background-color: hsl(203.6, 100%, 12%);
                --text-color-coolgray: hsl(240, 2.3%, 56.7%);
            }
        </style>
        <?php
        include(dirname(__FILE__) . '/results.php');
        echo "$total Total Votes";
    }

    public function process_vote() {
        if (self::responded()) {
            self::$responded = true;
            return;
        }

        $post_id = get_the_ID();
        $settings = get_post_meta($post_id, 'webkit_survey_settings');
        $Survey = $settings[0];
        $key = sha1(json_encode($Survey->survey));

        if (!wp_verify_nonce(filter_input(INPUT_POST, '_nonce'), "webkit_survey-$post_id"))
            return;

        $posted_input = filter_input_array(INPUT_POST, [
            'questions' => [
                'flags'     => FILTER_REQUIRE_ARRAY,
                'filter'    => FILTER_VALIDATE_INT
            ],
            'other' => [
                'flags'     => FILTER_REQUIRE_ARRAY,
                'filter'    => FILTER_VALIDATE_SCALAR
            ]
        ]);

        $data = $posted_input['questions'];
        foreach ($Survey->survey as $id => $SurveyQuestion) {
            $response = $posted_input['questions'][$id];
            $answer = empty($SurveyQuestion->responses[$response]) ? $response : $SurveyQuestion->responses[$response];
            if ($answer == 'Other:')
                $data[$id] .= "::" . $posted_input['other'][$id];
        }

        if (is_null($data))
            return;

        add_post_meta($post_id, $key, $data);

        // Ensure vote calculations get updated
        delete_transient($key);
        
        self::$responded = true;

        $post_url_parts = parse_url(get_permalink());
        setcookie(
            self::cookie_name($post_id),     // name
            1,                               // value
            time() + YEAR_IN_SECONDS,        // expires
            $post_url_parts['path'],         // path
            $post_url_parts['host'],         // domain
            false,                           // secure
            true                             // httponly
        );
        
        // Redirect for reload POST protection
        header('Location: ' . get_permalink());
    }
    
    public static function calculate_results() {
        $post_id = get_the_ID();
        $settings = get_post_meta($post_id, 'webkit_survey_settings');
        $Survey = $settings[0];
        $key = sha1(json_encode($Survey->survey));

        // Use cached tabulated responses if they exist
        if (false !== ($cached = get_transient($key)))
            return unserialize($cached);

        $responses = get_post_custom_values($key);
        $max = 0;

        // Add the tabulated responses to the survey data
        foreach ($responses as $response) {
            $response = unserialize($response);
            foreach ($response as $question_index => $option) {
                $SurveyQuestion = $Survey->survey[$question_index];
                
                if (!isset($SurveyQuestion->scores))
                    $SurveyQuestion->scores = [];
        
                if (strpos($option, "::")!== false) {
                    list($option, $other_value) = explode("::", $option);
                    
                    if (!isset($SurveyQuestion->other))                
                        $SurveyQuestion->other = [];
                    
                    $SurveyQuestion->other[] = $other_value;
                }
                
                $option = intval($option);
                
                if (!isset($SurveyQuestion->scores[$option]))
                    $SurveyQuestion->scores[$option] = 0;
                
                $SurveyQuestion->scores[$option]++;
                if ($SurveyQuestion->scores[$option] > $max)
                   $Survey->winner = $option;
                
                if (!isset($SurveyQuestion->scores['total']))
                    $SurveyQuestion->scores['total'] = 0;
                    
                $SurveyQuestion->scores['total']++;

            }
        }
        
        // Cache the tabulated responses
        set_transient($key, serialize($Survey), 300);
        return $Survey;
    }

    public static function responded() {
        $voted_cookie = filter_input(INPUT_COOKIE, self::cookie_name(get_the_ID()));
        return self::$responded || $voted_cookie;
    }

    private static function cookie_name($key) {
        return $key . COOKIEHASH;
    }
}

WebKit_Survey::init();
