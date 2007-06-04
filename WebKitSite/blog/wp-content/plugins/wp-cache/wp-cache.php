<?php
/*
Plugin Name: wp-cache
Plugin URI: http://mnm.uib.es/gallir/wp-cache-2/
Description: Very fast cache module. It's composed of several modules, this plugin can configure and manage the whole system. Once enabled, go to "Options" and select "WP-Cache".
Version: 2.1.1
Author: Ricardo Galli Granada
Author URI: http://mnm.uib.es/gallir/
*/
/*  Copyright 2005-2006  Ricardo Galli Granada  (email : gallir@uib.es)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Changelog
	2007-03-23
		- Version 2.1.1: Patch from Alex Concha: add control in admin pages to avoid 
		                possible XSS derived from CSRF attacks, if the users store
						the form with the "injected" bad values.
	2007-01-31
		- Version 2.1: modified and tested with WP 2.1, WP 2.0, WP 1.5 and PHP 4.3 and PHP 5.2.

	2007-01-14: 2.0.22
		- Corrected bug with meta object not marked as dynamic (introduce in 2.0.20 by http://dev.wp-plugins.org/ticket/517

	2006-12-31: 2.0.21
		- Added global definitien missing from http://dev.wp-plugins.org/ticket/517

	2006-12-31: 2.0.20
		- See http://mnm.uib.es/gallir/posts/2006/12/31/930/

	2006-11-06: 2.0.19
		- Added control of blog_id to delete only those cache files belonging to the same
		  virtual blog. $blog_id is tricky business, because the variable is not assigned yet
		  when wp-cache-phase1.php is called, so it cannot be used as part of the key.

	2006-11-04: 2.0.18 (beta)
		- Changed the use of REQUEST_URI to SCRIPT_URI for key generation. This
		  would solve problems in WP-MU.
		- Clean URI string in MetaCache object to avoid XSS attacks in the admin page.
		- Do not cache 404 pages.
	2005-10-23: 2.0.17
		- Commented out Content-Lenght negotiation, some site have strange problems with
		  ob_content_lenght and buffer length at OB shutdown. WP does not send it anyway.

	2005-10-20: 2.0.16
		- strlen($buffer) is a bug the that function, it's really not defined.
		
	2005-10-16: 2.0.15
		- Changed "Content-Size" to "Content-Length". Obvious bug.

	2005-09-12: 2.0.14
		- Add array() to headers to avoid PHP warnings

	2005-09-08: 2.0.13
			- Move request for Apache response headers to the shutdown callback
			  It seems some plugins do dirty things with headers... or php config?

	2005-07-26: 2.0.12
			- Patch from James (http://shreddies.org/) to delete individual cache files

	2005-07-21: 2.0.11
			- Check also for Last-Modified headers. Last WP version (1.5.3) does not 
			  it.
			- Move the previous check to the ob_callback, so the aditional headers 
			  can be sent also when cache still does not exist.

	2005-07-19: 2.0.10
			- Check also for feeds' closing tags
			  (patch from Daniel Westermann-Clark <dwc at ufl dot edu>)

	2005-07-19: 2.0.9
			- Better control of post_id and comment_id by refactoring the code
			  (inspired by a Brian Dupuis patch).
			- Avoid cleaning cache twice due to WP bugs that wrongly calls two actions.
			
	2005-07-12: 2.0.8
			- Add paranoic control to make sure it only caches files with
			  closing "body" and "html" tags.

	2005-06-23: 2.0.7
			- Add an extra control to make sure meta_mtime >= content_mtime
			  (it could serves incomplete html because other process is re-generating
			   content file and the meta file is the previous one).

	2005-06-23: 2.0.6
			- Delect cache _selectively_. If post_id is known
			  it deletes only that cache and the general (indexes) ones.
			  See: http://mnm.uib.es/gallir/wp-cache-2/#comment-4194
			- Delete cache files (all) also after moderation.

	2005-06-19: 2.0.5
			- Added "#anchors" to refresh cache files' list
			  (http://mnm.uib.es/gallir/wp-cache-2/#comment-4116)

	2005-06-09: 2.0.4
			- Avoid "fwrite() thrashing" at the begining of a connections storm
			- Send Content-Size header when generated dynamically too
			- Clean stats cache before deleting expired files
			- Optimized phase1, EVEN MORE! :-): 
				removed random and extrachecks that were not useful in the context 
				move checking for .meta at the begining

	2005-05-27: 2.0.3
			- Check for zero length of user agent and uri strings

	2005-05-24: 2.0.2a
			- As a workaround for buggy apache installations, create
			  Content-Type header if is not retrieved from Apache headers.
	2005-05-23: 2.0.2
			- Added mfunc sintax as in Staticize Reloaded 2.5,
			  also keep tags but take out function calls
			- Check of numbers are really numbers in web forms.
			- Remove header_list function verification, its result are not
			  the same.
			- wp-cache now verifies if gzipcompression is enabled
			
	2005-05-08: 2.0.1
			sanitize function names to aovid namespace collisions

	2005-05-08: 2.0-beta6
			ignore URI after #, it's sent by buggy clients
			print in red expired files's cache time
			if phase2 was compiled, reuse its function to remove files,
				it avoids race-conditions
			check html _and_ meta exist when listing/accesing files in cache

	2005-05-06: 2.0-beta5
			remove all expired files when one has expired
			POSTs are completely ignored
			only wordpress specific cookies are used for the md5 key

	2005-05-06: 2.0-beta4
			move wp_cache_microtime_diff to phase 2
			normalize functions' name in phase2
			workaround for nasty bug un PHP4(.3) that hits when cache fallbacks to flock()

	2005-05-06:	2.0-beta3
			include sample configuration file if the final one does not exist
			more verbose in errors
			change order of initial configuration to give better information
			stop if global cache is not enabled
			wp-cache-phase1 returns silently if no wp-cache-config.php is found
				
	2005-05-06:	2.0-beta2
			removed paranoic chmod's
			check for cache file consistency in Phase1 several times
			addded option to prevent cache based on user-agents
			added size in KB to every listed file in "cache listing"

*/

$wp_cache_config_file = ABSPATH . 'wp-content/wp-cache-config.php';
$wp_cache_config_file_sample = ABSPATH . 'wp-content/plugins/wp-cache/wp-cache-config-sample.php';
$wp_cache_link = ABSPATH . 'wp-content/advanced-cache.php';
$wp_cache_file = ABSPATH . 'wp-content/plugins/wp-cache/wp-cache-phase1.php';


if( !@include($wp_cache_config_file) ) {
	@include($wp_cache_config_file_sample);
}

function wp_cache_add_pages() {
	add_options_page('WP-Cache Manager', 'WP-Cache', 5, __FILE__, 'wp_cache_manager');
}

function wp_cache_manager() {
	global $wp_cache_config_file, $valid_nonce;

	$valid_nonce = wp_verify_nonce($_REQUEST['_wpnonce'], 'wp-cache');
	
 	echo '<div class="wrap">';
	echo "<h2>WP-Cache Manager</h2>\n";
	if(isset($_REQUEST['wp_restore_config']) && $valid_nonce) {
		unlink($wp_cache_config_file);
		echo '<strong>Configuration file changed, some values might be wrong. Load the page again from the "Options" menu to reset them.</strong>';
	}

	echo '<a name="main"></a><fieldset class="options"><legend>Main options</legend>';
	if ( !wp_cache_check_link() ||
		!wp_cache_verify_config_file() ||
		!wp_cache_verify_cache_dir() ) {
		echo "<br>Cannot continue... fix previous problems and retry.<br />";
		echo "</fieldset></div>\n";
		return;
	}

	if (!wp_cache_check_global_config()) {
		echo "</fieldset></div>\n";
		return;
	}

	if ( $valid_nonce ) {
		if(isset($_REQUEST['wp_enable'])) {
			wp_cache_enable();
		} elseif (isset($_REQUEST['wp_disable'])) {
			wp_cache_disable();
		}
	}

	echo '<form name="wp_manager" action="'. $_SERVER["REQUEST_URI"] . '" method="post">';
 	if (wp_cache_is_enabled()) {
		echo '<strong>WP-Cache is Enabled</strong>';
		echo '<input type="hidden" name="wp_disable" />';
		echo '<div class="submit"><input type="submit"value="Disable it" /></div>';
	} else {
		echo '<strong>WP-Cache is Disabled</strong>';
		echo '<input type="hidden" name="wp_enable" />';
		echo '<div class="submit"><input type="submit" value="Enable it" /></div>';
	}
	wp_nonce_field('wp-cache');
	echo "</form>\n";

	wp_cache_edit_max_time();
	echo '</fieldset>';

	echo '<a name="files"></a><fieldset class="options"><legend>Accepted filenames, rejected URIs</legend>';
	wp_cache_edit_rejected();
	echo "<br />\n";
	wp_cache_edit_accepted();
	echo '</fieldset>';

	wp_cache_edit_rejected_ua();

	wp_cache_files();

	wp_cache_restore();

	echo "</div>\n";

}

function wp_cache_restore() {
	echo '<fieldset class="options"><legend>Configuration messed up?</legend>';
	echo '<form name="wp_restore" action="'. $_SERVER["REQUEST_URI"] . '" method="post">';
	echo '<input type="hidden" name="wp_restore_config" />';
	echo '<div class="submit"><input type="submit" id="deletepost" value="Restore default configuration" /></div>';
	wp_nonce_field('wp-cache');
	echo "</form>\n";
	echo '</fieldset>';

}

function wp_cache_edit_max_time () {
	global $cache_max_time, $wp_cache_config_file, $valid_nonce;

	if(isset($_REQUEST['wp_max_time']) && $valid_nonce) {
		$max_time = (int)$_REQUEST['wp_max_time'];
		if ($max_time > 0) {
			$cache_max_time = $max_time;
			wp_cache_replace_line('^ *\$cache_max_time', "\$cache_max_time = $cache_max_time;", $wp_cache_config_file);
		}
	}
	echo '<form name="wp_edit_max_time" action="'. $_SERVER["REQUEST_URI"] . '" method="post">';
	echo '<label for="wp_max_time">Expire time (in seconds)</label>';
	echo "<input type=\"text\" name=\"wp_max_time\" value=\"$cache_max_time\" />";
	echo '<div class="submit"><input type="submit" value="Change expiration" /></div>';
	wp_nonce_field('wp-cache');
	echo "</form>\n";


}

function wp_cache_sanitize_value($text, & $array) {
	$text = wp_specialchars(strip_tags($text));
	$array = preg_split("/[\s,]+/", chop($text));
	$text = var_export($array, true);
	$text = preg_replace('/[\s]+/', ' ', $text);
	return $text;
}

function wp_cache_edit_rejected_ua() {
	global $cache_rejected_user_agent, $wp_cache_config_file, $valid_nonce;

	if (!function_exists('apache_request_headers')) return;

	if(isset($_REQUEST['wp_rejected_user_agent']) && $valid_nonce) {
		$text = wp_cache_sanitize_value($_REQUEST['wp_rejected_user_agent'], $cache_rejected_user_agent);
		wp_cache_replace_line('^ *\$cache_rejected_user_agent', "\$cache_rejected_user_agent = $text;", $wp_cache_config_file);
	}


	echo '<a name="user-agents"></a><fieldset class="options"><legend>Rejected User Agents</legend>';
	echo "<p>Strings in the HTTP 'User Agent' header that prevent WP-Cache from 
		caching bot, spiders, and crawlers' requests.
		Note that cached files are still sent to these request if they already exists.</p>\n";
	echo '<form name="wp_edit_rejected_user_agent" action="'. $_SERVER["REQUEST_URI"] . '" method="post">';
	echo '<label for="wp_rejected_user_agent">Rejected UA strings</label>';
	echo '<textarea name="wp_rejected_user_agent" cols="40" rows="4" style="width: 70%; font-size: 12px;" class="code">';
	foreach ($cache_rejected_user_agent as $ua) {
		echo wp_specialchars($ua) . "\n";
	}
	echo '</textarea> ';
	echo '<div class="submit"><input type="submit" value="Save UA strings" /></div>';
	wp_nonce_field('wp-cache');
	echo '</form>';
	echo "</fieldset>\n";
}


function wp_cache_edit_rejected() {
	global $cache_acceptable_files, $cache_rejected_uri, $wp_cache_config_file, $valid_nonce;

	if(isset($_REQUEST['wp_rejected_uri']) && $valid_nonce) {
		$text = wp_cache_sanitize_value($_REQUEST['wp_rejected_uri'], $cache_rejected_uri);
		wp_cache_replace_line('^ *\$cache_rejected_uri', "\$cache_rejected_uri = $text;", $wp_cache_config_file);
	}


	echo "<p>Add here strings (not a filename) that forces a page not to be cached. For example, if your URLs include year and you dont want to cache last year posts, it's enough to specify the year, i.e. '/2004/'. WP-Cache will search if that string is part of the URI and if so, it will no cache that page.</p>\n";
	echo '<form name="wp_edit_rejected" action="'. $_SERVER["REQUEST_URI"] . '" method="post">';
	echo '<label for="wp_rejected_uri">Rejected URIs</label>';
	echo '<textarea name="wp_rejected_uri" cols="40" rows="4" style="width: 70%; font-size: 12px;" class="code">';
	foreach ($cache_rejected_uri as $file) {
		echo wp_specialchars($file) . "\n";
	}
	echo '</textarea> ';
	echo '<div class="submit"><input type="submit" value="Save strings" /></div>';
	wp_nonce_field('wp-cache');
	echo "</form>\n";
}

function wp_cache_edit_accepted() {
	global $cache_acceptable_files, $cache_rejected_uri, $wp_cache_config_file, $valid_nonce;

	if(isset($_REQUEST['wp_accepted_files']) && $valid_nonce) {
		$text = wp_cache_sanitize_value($_REQUEST['wp_accepted_files'], $cache_acceptable_files);
		wp_cache_replace_line('^ *\$cache_acceptable_files', "\$cache_acceptable_files = $text;", $wp_cache_config_file);
	}


	echo "<p>Add here those filenames that can be cached, even if they match one of the rejected substring specified above.</p>\n";
	echo '<form name="wp_edit_accepted" action="'. $_SERVER["REQUEST_URI"] . '" method="post">';
	echo '<label for="wp_accepted_files">Accepted files</label>';
	echo '<textarea name="wp_accepted_files" cols="40" rows="8" style="width: 70%; font-size: 12px;" class="code">';
	foreach ($cache_acceptable_files as $file) {
		echo wp_specialchars($file) . "\n";
	}
	echo '</textarea> ';
	echo '<div class="submit"><input type="submit" value="Save files" /></div>';
	wp_nonce_field('wp-cache');
	echo "</form>\n";
}

function wp_cache_enable() {
	global $wp_cache_config_file, $cache_enabled;

	if(get_settings('gzipcompression')) {
		echo "<b>Error: GZIP compression is enabled, disable it if you want to enable wp-cache.</b><br /><br />";
		return false;
	}
	if( wp_cache_replace_line('^ *\$cache_enabled', '$cache_enabled = true;', $wp_cache_config_file) ) {
		$cache_enabled = true;
	}
}

function wp_cache_disable() {
	global $wp_cache_config_file, $cache_enabled;

	if (wp_cache_replace_line('^ *\$cache_enabled', 
			'$cache_enabled = false;', $wp_cache_config_file)) {
		$cache_enabled = false;
	}
}

function wp_cache_is_enabled() {
	global $wp_cache_config_file;

	if(get_settings('gzipcompression')) {
		echo "<b>Warning</b>: GZIP compression is enabled in Wordpress, wp-cache will be bypassed until you disable gzip compression.<br />";
		return false;
	}
	$lines = file($wp_cache_config_file);
	foreach($lines as $line) {
	 	if (preg_match('/^ *\$cache_enabled *= *true *;/', $line))
			return true;
	}
	return false;
}


function wp_cache_replace_line($old, $new, $my_file) {
	if (!is_writable($my_file)) {
		echo "Error: file $my_file is not writeable.<br />\n";
		return false;
	}
	$found = false;
	$lines = file($my_file);
	foreach($lines as $line) {
	 	if ( preg_match("/$old/", $line)) {
			$found = true;
			break;
		}
	}
	if ($found) {
		$fd = fopen($my_file, 'w');
		foreach($lines as $line) {
			if ( !preg_match("/$old/", $line))
				fputs($fd, $line);
			else {
				fputs($fd, "$new //Added by WP-Cache Manager\n");
			}
		}
		fclose($fd);
		return true;
	}
	$fd = fopen($my_file, 'w');
	$done = false;
	foreach($lines as $line) {
		if ( $done || !preg_match('/^define|\$|\?>/', $line))
			fputs($fd, $line);
		else {
			fputs($fd, "$new //Added by WP-Cache Manager\n");
			fputs($fd, $line);
			$done = true;
		}
	}
	fclose($fd);
	return true;
/*
	copy($my_file, $my_file . "-prev");
	rename($my_file . '-new', $my_file);
*/
}

function wp_cache_verify_cache_dir() {
	global $cache_path;

	$dir = dirname($cache_path);
	if ( !file_exists($cache_path) ) {
		if ( !is_writable( $dir ) || !($dir = mkdir( $cache_path, 0777) ) ) {
				echo "<b>Error:</b> Your cache directory (<b>$cache_path</b>) did not exist and couldn't be created by the web server. <br /> Check  $dir permissions.";
				return false;
		}
	}
	if ( !is_writable($cache_path)) {
		echo "<b>Error:</b> Your cache directory (<b>$cache_path</b>) or <b>$dir</b> need to be writable for this plugin to work. <br /> Double-check it.";
		return false;
	}

	if ( '/' != substr($cache_path, -1)) {
		$cache_path .= '/';
	}
	return true;
}

function wp_cache_verify_config_file() {
	global $wp_cache_config_file, $wp_cache_config_file_sample;

	$new = false;
	$dir = dirname($wp_cache_config_file);

	if ( !is_writable($dir)) {
			echo "<b>Error:</b> wp-content directory (<b>$dir</b>) is not writable by the Web server.<br />Check its permissions.";
			return false;
	}
	if ( !file_exists($wp_cache_config_file) ) {
		if ( !file_exists($wp_cache_config_file_sample) ) {
			echo "<b>Error:</b> Sample WP-Cache config file (<b>$wp_cache_config_file_sample</b>) does not exist.<br />Verify you installation.";
			return false;
		}
		copy($wp_cache_config_file_sample, $wp_cache_config_file);
		$new = true;
	}
	if ( !is_writable($wp_cache_config_file)) {
		echo "<b>Error:</b> Your WP-Cache config file (<b>$wp_cache_config_file</b>) is not writable by the Web server.<br />Check its permissions.";
		return false;
	}
	require($wp_cache_config_file);
	return true;
}

function wp_cache_check_link() {
	global $wp_cache_link, $wp_cache_file;

	if ( basename(@readlink($wp_cache_link)) != basename($wp_cache_file)) {
		@unlink($wp_cache_link);
		if (!@symlink ($wp_cache_file, $wp_cache_link)) {
			echo "<code>advanced-cache.php</code> link does not exist<br />";
			echo "Create it by executing: <br /><code>ln -s $wp_cache_file $wp_cache_link</code><br /> in your server<br />";
			return false;
		}
	}
	return true;
}

function wp_cache_check_global_config() {

	$global = ABSPATH . 'wp-config.php';

	$lines = file($global);
	foreach($lines as $line) {
	 	if (preg_match('/^ *define *\( *\'WP_CACHE\' *, *true *\) *;/', $line)) {
			return true;
		}
	}
	$line = 'define(\'WP_CACHE\', true);';
	if (!is_writable($global) || !wp_cache_replace_line('define *\( *\'WP_CACHE\'', $line, $global) ) {
			echo "<b>Error: WP_CACHE is not enabled</b> in your <code>wp-config.php</code> file and I couldn't modified it.<br />";
			echo "Edit <code>$global</code> and add the following line: <br /><code>define('WP_CACHE', true);</code><br />Otherwise, <b>WP-Cache will not be executed</b> by Wordpress core. <br />";
			return false;
	} 
	return true;
}

function wp_cache_files() {
	global $cache_path, $file_prefix, $cache_max_time, $valid_nonce;

	if ( '/' != substr($cache_path, -1)) {
		$cache_path .= '/';
	}

	if ( $valid_nonce ) {
		if(isset($_REQUEST['wp_delete_cache'])) {
			wp_cache_clean_cache($file_prefix);
		}
		if(isset($_REQUEST['wp_delete_cache_file'])) {
			wp_cache_clean_cache($_REQUEST['wp_delete_cache_file']);
		}
		if(isset($_REQUEST['wp_delete_expired'])) {
			wp_cache_clean_expired($file_prefix);
		}
	}
	if(isset($_REQUEST['wp_list_cache'])) {
		$list_files = true;
		$list_mess = "Update list";
	} else 
		$list_mess = "List files";

	echo '<a name="list"></a><fieldset class="options"><legend>Cache contents</legend>';
	echo '<form name="wp_cache_content_list" action="'. $_SERVER["REQUEST_URI"] . '#list" method="post">';
	echo '<input type="hidden" name="wp_list_cache" />';
	echo '<div class="submit"><input type="submit" value="'.$list_mess.'" /></div>';
	echo "</form>\n";

	$count = 0;
	$expired = 0;
	$now = time();
	if ( ($handle = opendir( $cache_path )) ) { 
		if ($list_files) echo "<table cellspacing=\"0\" cellpadding=\"5\">";
		while ( false !== ($file = readdir($handle))) {
			if ( preg_match("/^$file_prefix.*\.meta/", $file) ) {
				$this_expired = false;
				$content_file = preg_replace("/meta$/", "html", $file);
				$mtime = filemtime($cache_path.$file);
				if ( ! ($fsize = @filesize($cache_path.$content_file)) ) 
					continue; // .meta does not exists
				$fsize = intval($fsize/1024);
				$age = $now - $mtime;
				if ( $age > $cache_max_time) {
					$expired++;
					$this_expired = true;
				}
				$count++;
				if ($list_files) {
					$meta = new CacheMeta;
					$meta = unserialize(file_get_contents($cache_path . $file));
					echo $flip ? '<tr style="background: #EAEAEA;">' : '<tr>';
					$flip = !$flip;
					echo '<td><a href="http://' . $meta->uri . '" target="_blank" >';
					echo $meta->uri . "</a></td>";
					if ($this_expired) echo "<td><span style='color:red'>$age secs</span></td>";
					else echo "<td>$age secs</td>";
					echo "<td>$fsize KB</td>";
					echo '<td><form name="wp_delete_cache_file" action="'. $_SERVER["REQUEST_URI"] . '#list" method="post">';
					echo '<input type="hidden" name="wp_list_cache" />';
					echo '<input type="hidden" name="wp_delete_cache_file" value="'.preg_replace("/^(.*)\.meta$/", "$1", $file).'" />';
					echo '<div class="submit"><input id="deletepost" type="submit" value="Remove" /></div>';
					wp_nonce_field('wp-cache');
					echo "</form></td></tr>\n";
				}
			}
		}
		closedir($handle);
		if ($list_files) echo "</table>";
	}
	echo "<P><b>$count cached pages</b></p>";
	echo "<P><b>$expired expired pages</b></p>";

	echo '<form name="wp_cache_content_expired" action="'. $_SERVER["REQUEST_URI"] . '#list" method="post">';
	echo '<input type="hidden" name="wp_delete_expired" />';
	echo '<input type="hidden" name="wp_list_cache" />';
	echo '<div class="submit"><input type="submit" value="Delete expired" /></div>';
	wp_nonce_field('wp-cache');
	echo "</form>\n";


	echo '<form name="wp_cache_content_delete" action="'. $_SERVER["REQUEST_URI"] . '#list" method="post">';
	echo '<input type="hidden" name="wp_delete_cache" />';
	echo '<div class="submit"><input id="deletepost" type="submit" value="Delete cache" /></div>';
	wp_nonce_field('wp-cache');
	echo "</form>\n";

	echo '</fieldset>';
}

function wp_cache_clean_cache($file_prefix) {
	global $cache_path;

	// If phase2 was compiled, use its function to avoid race-conditions
	if(function_exists('wp_cache_phase2_clean_cache'))
		return wp_cache_phase2_clean_cache($file_prefix);

	$expr = "/^$file_prefix/";
	if ( ($handle = opendir( $cache_path )) ) { 
		while ( false !== ($file = readdir($handle))) {
			if ( preg_match($expr, $file) ) {
				unlink($cache_path . $file);
			}
		}
		closedir($handle);
	}
}

function wp_cache_clean_expired($file_prefix) {
	global $cache_path, $cache_max_time;

	// If phase2 was compiled, use its function to avoid race-conditions
	if(function_exists('wp_cache_phase2_clean_expired'))
		return wp_cache_phase2_clean_expired($file_prefix);

	$expr = "/^$file_prefix/";
	$now = time();
	if ( ($handle = opendir( $cache_path )) ) { 
		while ( false !== ($file = readdir($handle))) {
			if ( preg_match($expr, $file)  &&
				(filemtime($cache_path . $file) + $cache_max_time) <= $now) {
				unlink($cache_path . $file);
			}
		}
		closedir($handle);
	}
}

add_action('admin_menu', 'wp_cache_add_pages');
?>
