<?php
/*
Plugin Name: Hyperlight
Plugin URI: http://code.google.com/p/hyperlight
Description: Static, CSS-based syntax highlighter for a variety of languages.
Author: Jonathan Davis
Version: 1.0
*/

require_once('hyperlight/hyperlight.php');

$hyperlight_codes = array();
$hyperlight_replace_token = uniqid('hyperlight', true) . ':';
$hyperlight_code_index = 0;

//
// Wire plugin into WordPress
//

add_filter('the_content', 'hyperlight_before_filter', 7);
add_filter('the_content', 'hyperlight_after_filter', 99);

add_filter('get_comment_text', 'hyperlight_before_filter', 7);
add_filter('get_comment_text', 'hyperlight_after_filter', 99);

add_filter('post_text', 'hyperlight_before_filter', 7);
add_filter('post_text', 'hyperlight_after_filter', 99);

// Replace code blocks which specify a `class` argument with a replace token. The
// token is a key in a dictionary and will be replaced by the after-filter with
// the corresponding highlighted source code.
// TODO Activate similar mechanism for <code>!
function hyperlight_before_filter( $content ) {
    return preg_replace_callback(
        '#<code class="(.+?)">(.+?)</code>#is',
        'hyperlight_highlight_block',
        $content
    );
}

// Replace the hyperlight replace tokens with the corresponding highlighted
// source code.
function hyperlight_after_filter($content) {
    global $hyperlight_replace_token;
    return preg_replace_callback(
        "/$hyperlight_replace_token\d+/",
        'hyperlight_insert_block',
        $content
    );
}

function hyperlight_highlight_block($match) {
    global $hyperlight_codes, $hyperlight_replace_token, $hyperlight_code_index;

    list(, $lang, $code) = $match;
    
    $code = html_entity_decode( $code, ENT_QUOTES );
    
    if ( ! isset($lang) ) 
        return $match[0]; // No language given: don't highlight.

	// Auto-detect implicit PHP code
	if ('php' == $lang && !preg_match('/(<\?php|\?>)/',$code))
		$lang = 'iphp';

    $hyperlight = new Hyperlight($lang);
    $code = $hyperlight->render($code);

    // $code =  htmlspecialchars_decode($code, ENT_NOQUOTES);

    $index = "$hyperlight_replace_token$hyperlight_code_index";
    ++$hyperlight_code_index;
    $hyperlight_codes[ $index ] = '<code class="' . esc_attr($lang) . '">' . $code . '</code>';
    
    return $index;
}

function hyperlight_insert_block($match) {
    global $hyperlight_codes;
    return $hyperlight_codes[ $match[0] ];
}