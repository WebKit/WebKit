<?php
if( !@include(ABSPATH . 'wp-content/wp-cache-config.php') ) {
	return;
}

$mutex_filename = 'wp_cache_mutex.lock';
$new_cache = false;
	

// Don't change variables behind this point

if (!$cache_enabled || $_SERVER["REQUEST_METHOD"] == 'POST') 
	return;

$file_expired = false;


$cache_filename = '';
$meta_file = '';


$key = md5($_SERVER['SERVER_NAME'].preg_replace('/#.*$/', '', $_SERVER['REQUEST_URI']).wp_cache_get_cookies_values());
$cache_filename = $file_prefix . $key . '.html';
$meta_file = $file_prefix . $key . '.meta';
$cache_file = $cache_path . $cache_filename;
$meta_pathname = $cache_path . $meta_file;


$wp_start_time = microtime();
if( ($mtime = @filemtime($meta_pathname)) ) {
	if ($mtime + $cache_max_time > time() ) {
		$meta = new CacheMeta;
		if (! ($meta = unserialize(@file_get_contents($meta_pathname))) ) 
			return;
		foreach ($meta->headers as $header) {
			header($header);
		}
		$log = "<!-- Cached page served by WP-Cache -->\n";
		if ( !($content_size = @filesize($cache_file)) > 0 || $mtime < @filemtime($cache_file))
			return;
		if ($meta->dynamic) {
			include($cache_file);
		} else {
			/* No used to avoid problems with some PHP installations
			$content_size += strlen($log);
			header("Content-Length: $content_size");
			*/
			if(!@readfile ($cache_file)) 
				return;
		}
		echo $log;
		die;
	}
	$file_expired = true; // To signal this file was expired
}

function wp_cache_postload() {
	global $cache_enabled;

	if (!$cache_enabled) 
		return;
	require(ABSPATH . 'wp-content/plugins/wp-cache/wp-cache-phase2.php');
	wp_cache_phase2();
}

function wp_cache_get_cookies_values() {
	$string = '';
	while ($key = key($_COOKIE)) {
		if (preg_match("/^wordpress|^comment_author_email_/", $key)) {
			$string .= $_COOKIE[$key] . ",";
		}
		next($_COOKIE);
	}
	reset($_COOKIE);
	return $string;
}

?>
