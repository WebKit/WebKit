<?php
/*
Plugin Name: Akismet
Plugin URI: http://akismet.com/
Description: Akismet checks your comments against the Akismet web service to see if they look like spam or not. You need a <a href="http://wordpress.com/api-keys/">WordPress.com API key</a> to use it. You can review the spam it catches under "Comments." To show off your Akismet stats just put <code>&lt;?php akismet_counter(); ?&gt;</code> in your template. See also: <a href="http://wordpress.org/extend/plugins/stats/">WP Stats plugin</a>.
Version: 2.2.6
Author: Matt Mullenweg
Author URI: http://ma.tt/
*/

define('AKISMET_VERSION', '2.2.6');

// If you hardcode a WP.com API key here, all key config screens will be hidden
if ( defined('WPCOM_API_KEY') )
	$wpcom_api_key = constant('WPCOM_API_KEY');
else
	$wpcom_api_key = '';

function akismet_init() {
	global $wpcom_api_key, $akismet_api_host, $akismet_api_port;

	if ( $wpcom_api_key )
		$akismet_api_host = $wpcom_api_key . '.rest.akismet.com';
	else
		$akismet_api_host = get_option('wordpress_api_key') . '.rest.akismet.com';

	$akismet_api_port = 80;
	add_action('admin_menu', 'akismet_config_page');
	add_action('admin_menu', 'akismet_stats_page');
	akismet_admin_warnings();
}
add_action('init', 'akismet_init');

function akismet_admin_init() {
	if ( function_exists( 'get_plugin_page_hook' ) )
		$hook = get_plugin_page_hook( 'akismet-stats-display', 'index.php' );
	else
		$hook = 'dashboard_page_akismet-stats-display';
	add_action('admin_head-'.$hook, 'akismet_stats_script');
}
add_action('admin_init', 'akismet_admin_init');

if ( !function_exists('wp_nonce_field') ) {
	function akismet_nonce_field($action = -1) { return; }
	$akismet_nonce = -1;
} else {
	function akismet_nonce_field($action = -1) { return wp_nonce_field($action); }
	$akismet_nonce = 'akismet-update-key';
}

if ( !function_exists('number_format_i18n') ) {
	function number_format_i18n( $number, $decimals = null ) { return number_format( $number, $decimals ); }
}

function akismet_config_page() {
	if ( function_exists('add_submenu_page') )
		add_submenu_page('plugins.php', __('Akismet Configuration'), __('Akismet Configuration'), 'manage_options', 'akismet-key-config', 'akismet_conf');

}

function akismet_conf() {
	global $akismet_nonce, $wpcom_api_key;

	if ( isset($_POST['submit']) ) {
		if ( function_exists('current_user_can') && !current_user_can('manage_options') )
			die(__('Cheatin&#8217; uh?'));

		check_admin_referer( $akismet_nonce );
		$key = preg_replace( '/[^a-h0-9]/i', '', $_POST['key'] );

		if ( empty($key) ) {
			$key_status = 'empty';
			$ms[] = 'new_key_empty';
			delete_option('wordpress_api_key');
		} else {
			$key_status = akismet_verify_key( $key );
		}

		if ( $key_status == 'valid' ) {
			update_option('wordpress_api_key', $key);
			$ms[] = 'new_key_valid';
		} else if ( $key_status == 'invalid' ) {
			$ms[] = 'new_key_invalid';
		} else if ( $key_status == 'failed' ) {
			$ms[] = 'new_key_failed';
		}

		if ( isset( $_POST['akismet_discard_month'] ) )
			update_option( 'akismet_discard_month', 'true' );
		else
			update_option( 'akismet_discard_month', 'false' );
	} elseif ( isset($_POST['check']) ) {
		akismet_get_server_connectivity(0);
	}

	if ( $key_status != 'valid' ) {
		$key = get_option('wordpress_api_key');
		if ( empty( $key ) ) {
			if ( $key_status != 'failed' ) {
				if ( akismet_verify_key( '1234567890ab' ) == 'failed' )
					$ms[] = 'no_connection';
				else
					$ms[] = 'key_empty';
			}
			$key_status = 'empty';
		} else {
			$key_status = akismet_verify_key( $key );
		}
		if ( $key_status == 'valid' ) {
			$ms[] = 'key_valid';
		} else if ( $key_status == 'invalid' ) {
			delete_option('wordpress_api_key');
			$ms[] = 'key_empty';
		} else if ( !empty($key) && $key_status == 'failed' ) {
			$ms[] = 'key_failed';
		}
	}

	$messages = array(
		'new_key_empty' => array('color' => 'aa0', 'text' => __('Your key has been cleared.')),
		'new_key_valid' => array('color' => '2d2', 'text' => __('Your key has been verified. Happy blogging!')),
		'new_key_invalid' => array('color' => 'd22', 'text' => __('The key you entered is invalid. Please double-check it.')),
		'new_key_failed' => array('color' => 'd22', 'text' => __('The key you entered could not be verified because a connection to akismet.com could not be established. Please check your server configuration.')),
		'no_connection' => array('color' => 'd22', 'text' => __('There was a problem connecting to the Akismet server. Please check your server configuration.')),
		'key_empty' => array('color' => 'aa0', 'text' => sprintf(__('Please enter an API key. (<a href="%s" style="color:#fff">Get your key.</a>)'), 'http://wordpress.com/profile/')),
		'key_valid' => array('color' => '2d2', 'text' => __('This key is valid.')),
		'key_failed' => array('color' => 'aa0', 'text' => __('The key below was previously validated but a connection to akismet.com can not be established at this time. Please check your server configuration.')));
?>
<?php if ( !empty($_POST['submit'] ) ) : ?>
<div id="message" class="updated fade"><p><strong><?php _e('Options saved.') ?></strong></p></div>
<?php endif; ?>
<div class="wrap">
<h2><?php _e('Akismet Configuration'); ?></h2>
<div class="narrow">
<form action="" method="post" id="akismet-conf" style="margin: auto; width: 400px; ">
<?php if ( !$wpcom_api_key ) { ?>
	<p><?php printf(__('For many people, <a href="%1$s">Akismet</a> will greatly reduce or even completely eliminate the comment and trackback spam you get on your site. If one does happen to get through, simply mark it as "spam" on the moderation screen and Akismet will learn from the mistakes. If you don\'t have a WordPress.com account yet, you can get one at <a href="%2$s">WordPress.com</a>.'), 'http://akismet.com/', 'http://wordpress.com/api-keys/'); ?></p>

<?php akismet_nonce_field($akismet_nonce) ?>
<h3><label for="key"><?php _e('WordPress.com API Key'); ?></label></h3>
<?php foreach ( $ms as $m ) : ?>
	<p style="padding: .5em; background-color: #<?php echo $messages[$m]['color']; ?>; color: #fff; font-weight: bold;"><?php echo $messages[$m]['text']; ?></p>
<?php endforeach; ?>
<p><input id="key" name="key" type="text" size="15" maxlength="12" value="<?php echo get_option('wordpress_api_key'); ?>" style="font-family: 'Courier New', Courier, mono; font-size: 1.5em;" /> (<?php _e('<a href="http://faq.wordpress.com/2005/10/19/api-key/">What is this?</a>'); ?>)</p>
<?php if ( $invalid_key ) { ?>
<h3><?php _e('Why might my key be invalid?'); ?></h3>
<p><?php _e('This can mean one of two things, either you copied the key wrong or that the plugin is unable to reach the Akismet servers, which is most often caused by an issue with your web host around firewalls or similar.'); ?></p>
<?php } ?>
<?php } ?>
<p><label><input name="akismet_discard_month" id="akismet_discard_month" value="true" type="checkbox" <?php if ( get_option('akismet_discard_month') == 'true' ) echo ' checked="checked" '; ?> /> <?php _e('Automatically discard spam comments on posts older than a month.'); ?></label></p>
	<p class="submit"><input type="submit" name="submit" value="<?php _e('Update options &raquo;'); ?>" /></p>
</form>

<form action="" method="post" id="akismet-connectivity" style="margin: auto; width: 400px; ">

<h3><?php _e('Server Connectivity'); ?></h3>
<?php
	$servers = akismet_get_server_connectivity();
	$fail_count = count($servers) - count( array_filter($servers) );
	if ( is_array($servers) && count($servers) > 0 ) {
		// some connections work, some fail
		if ( $fail_count > 0 && $fail_count < count($servers) ) { ?>
			<p style="padding: .5em; background-color: #aa0; color: #fff; font-weight:bold;"><?php _e('Unable to reach some Akismet servers.'); ?></p>
			<p><?php echo sprintf( __('A network problem or firewall is blocking some connections from your web server to Akismet.com.  Akismet is working but this may cause problems during times of network congestion.  Please contact your web host or firewall administrator and give them <a href="%s" target="_blank">this information about Akismet and firewalls</a>.'), 'http://blog.akismet.com/akismet-hosting-faq/'); ?></p>
		<?php
		// all connections fail
		} elseif ( $fail_count > 0 ) { ?>
			<p style="padding: .5em; background-color: #d22; color: #fff; font-weight:bold;"><?php _e('Unable to reach any Akismet servers.'); ?></p>
			<p><?php echo sprintf( __('A network problem or firewall is blocking all connections from your web server to Akismet.com.  <strong>Akismet cannot work correctly until this is fixed.</strong>  Please contact your web host or firewall administrator and give them <a href="%s" target="_blank">this information about Akismet and firewalls</a>.'), 'http://blog.akismet.com/akismet-hosting-faq/'); ?></p>
		<?php
		// all connections work
		} else { ?>
			<p style="padding: .5em; background-color: #2d2; color: #fff; font-weight:bold;"><?php  _e('All Akismet servers are available.'); ?></p>
			<p><?php _e('Akismet is working correctly.  All servers are accessible.'); ?></p>
		<?php
		}
	} elseif ( !is_callable('fsockopen') ) {
		?>
			<p style="padding: .5em; background-color: #d22; color: #fff; font-weight:bold;"><?php _e('Network functions are disabled.'); ?></p>
			<p><?php echo sprintf( __('Your web host or server administrator has disabled PHP\'s <code>fsockopen</code> function.  <strong>Akismet cannot work correctly until this is fixed.</strong>  Please contact your web host or firewall administrator and give them <a href="%s" target="_blank">this information about Akismet\'s system requirements</a>.'), 'http://blog.akismet.com/akismet-hosting-faq/'); ?></p>
		<?php
	} else {
		?>
			<p style="padding: .5em; background-color: #d22; color: #fff; font-weight:bold;"><?php _e('Unable to find Akismet servers.'); ?></p>
			<p><?php echo sprintf( __('A DNS problem or firewall is preventing all access from your web server to Akismet.com.  <strong>Akismet cannot work correctly until this is fixed.</strong>  Please contact your web host or firewall administrator and give them <a href="%s" target="_blank">this information about Akismet and firewalls</a>.'), 'http://blog.akismet.com/akismet-hosting-faq/'); ?></p>
		<?php
	}
	
	if ( !empty($servers) ) {
?>
<table style="width: 100%;">
<thead><th><?php _e('Akismet server'); ?></th><th><?php _e('Network Status'); ?></th></thead>
<tbody>
<?php
		asort($servers);
		foreach ( $servers as $ip => $status ) {
			$color = ( $status ? '#2d2' : '#d22');
	?>
		<tr>
		<td><?php echo htmlspecialchars($ip); ?></td>
		<td style="padding: 0 .5em; font-weight:bold; color: #fff; background-color: <?php echo $color; ?>"><?php echo ($status ? __('No problems') : __('Obstructed') ); ?></td>
		
	<?php
		}
	}
?>
</tbody>
</table>
	<p><?php if ( get_option('akismet_connectivity_time') ) echo sprintf( __('Last checked %s ago.'), human_time_diff( get_option('akismet_connectivity_time') ) ); ?></p>
	<p class="submit"><input type="submit" name="check" value="<?php _e('Check network status &raquo;'); ?>" /></p>
</form>

</div>
</div>
<?php
}

function akismet_stats_page() {
	if ( function_exists('add_submenu_page') )
		add_submenu_page('index.php', __('Akismet Stats'), __('Akismet Stats'), 'manage_options', 'akismet-stats-display', 'akismet_stats_display');

}

function akismet_stats_script() {
	?>
<script type="text/javascript">
function resizeIframe() {
    var height = document.documentElement.clientHeight;
    height -= document.getElementById('akismet-stats-frame').offsetTop;
    height += 100; // magic padding
    
    document.getElementById('akismet-stats-frame').style.height = height +"px";
    
};
function resizeIframeInit() {
	document.getElementById('akismet-stats-frame').onload = resizeIframe;
	window.onresize = resizeIframe;
}
addLoadEvent(resizeIframeInit);
</script><?php
}


function akismet_stats_display() {
	global $akismet_api_host, $akismet_api_port, $wpcom_api_key;
	$blog = urlencode( get_option('home') );
	$url = "http://".akismet_get_key().".web.akismet.com/1.0/user-stats.php?blog={$blog}";
	?>
	<div class="wrap">
	<iframe src="<?php echo $url; ?>" width="100%" height="100%" frameborder="0" id="akismet-stats-frame"></iframe>
	</div>
	<?php
}

function akismet_get_key() {
	global $wpcom_api_key;
	if ( !empty($wpcom_api_key) )
		return $wpcom_api_key;
	return get_option('wordpress_api_key');
}

function akismet_verify_key( $key, $ip = null ) {
	global $akismet_api_host, $akismet_api_port, $wpcom_api_key;
	$blog = urlencode( get_option('home') );
	if ( $wpcom_api_key )
		$key = $wpcom_api_key;
	$response = akismet_http_post("key=$key&blog=$blog", 'rest.akismet.com', '/1.1/verify-key', $akismet_api_port, $ip);
	if ( !is_array($response) || !isset($response[1]) || $response[1] != 'valid' && $response[1] != 'invalid' )
		return 'failed';
	return $response[1];
}

// Check connectivity between the WordPress blog and Akismet's servers.
// Returns an associative array of server IP addresses, where the key is the IP address, and value is true (available) or false (unable to connect).
function akismet_check_server_connectivity() {
	global $akismet_api_host, $akismet_api_port, $wpcom_api_key;
	
	$test_host = 'rest.akismet.com';
	
	// Some web hosts may disable one or both functions
	if ( !is_callable('fsockopen') || !is_callable('gethostbynamel') )
		return array();
	
	$ips = gethostbynamel($test_host);
	if ( !$ips || !is_array($ips) || !count($ips) )
		return array();
		
	$servers = array();
	foreach ( $ips as $ip ) {
		$response = akismet_verify_key( akismet_get_key(), $ip );
		// even if the key is invalid, at least we know we have connectivity
		if ( $response == 'valid' || $response == 'invalid' )
			$servers[$ip] = true;
		else
			$servers[$ip] = false;
	}

	return $servers;
}

// Check the server connectivity and store the results in an option.
// Cached results will be used if not older than the specified timeout in seconds; use $cache_timeout = 0 to force an update.
// Returns the same associative array as akismet_check_server_connectivity()
function akismet_get_server_connectivity( $cache_timeout = 86400 ) {
	$servers = get_option('akismet_available_servers');
	if ( (time() - get_option('akismet_connectivity_time') < $cache_timeout) && $servers !== false )
		return $servers;
	
	// There's a race condition here but the effect is harmless.
	$servers = akismet_check_server_connectivity();
	update_option('akismet_available_servers', $servers);
	update_option('akismet_connectivity_time', time());
	return $servers;
}

// Returns true if server connectivity was OK at the last check, false if there was a problem that needs to be fixed.
function akismet_server_connectivity_ok() {
	$servers = akismet_get_server_connectivity();
	return !( empty($servers) || !count($servers) || count( array_filter($servers) ) < count($servers) );
}

function akismet_admin_warnings() {
	global $wpcom_api_key;
	if ( !get_option('wordpress_api_key') && !$wpcom_api_key && !isset($_POST['submit']) ) {
		function akismet_warning() {
			echo "
			<div id='akismet-warning' class='updated fade'><p><strong>".__('Akismet is almost ready.')."</strong> ".sprintf(__('You must <a href="%1$s">enter your WordPress.com API key</a> for it to work.'), "plugins.php?page=akismet-key-config")."</p></div>
			";
		}
		add_action('admin_notices', 'akismet_warning');
		return;
	} elseif ( get_option('akismet_connectivity_time') && empty($_POST) && is_admin() && !akismet_server_connectivity_ok() ) {
		function akismet_warning() {
			echo "
			<div id='akismet-warning' class='updated fade'><p><strong>".__('Akismet has detected a problem.')."</strong> ".sprintf(__('A server or network problem is preventing Akismet from working correctly.  <a href="%1$s">Click here for more information</a> about how to fix the problem.'), "plugins.php?page=akismet-key-config")."</p></div>
			";
		}
		add_action('admin_notices', 'akismet_warning');
		return;
	}
}

function akismet_get_host($host) {
	// if all servers are accessible, just return the host name.
	// if not, return an IP that was known to be accessible at the last check.
	if ( akismet_server_connectivity_ok() ) {
		return $host;
	} else {
		$ips = akismet_get_server_connectivity();
		// a firewall may be blocking access to some Akismet IPs
		if ( count($ips) > 0 && count(array_filter($ips)) < count($ips) ) {
			// use DNS to get current IPs, but exclude any known to be unreachable
			$dns = (array)gethostbynamel( rtrim($host, '.') . '.' );
			$dns = array_filter($dns);
			foreach ( $dns as $ip ) {
				if ( array_key_exists( $ip, $ips ) && empty( $ips[$ip] ) )
					unset($dns[$ip]);
			}
			// return a random IP from those available
			if ( count($dns) )
				return $dns[ array_rand($dns) ];
			
		}
	}
	// if all else fails try the host name
	return $host;
}

// Returns array with headers in $response[0] and body in $response[1]
function akismet_http_post($request, $host, $path, $port = 80, $ip=null) {
	global $wp_version;
	
	$akismet_version = constant('AKISMET_VERSION');

	$http_request  = "POST $path HTTP/1.0\r\n";
	$http_request .= "Host: $host\r\n";
	$http_request .= "Content-Type: application/x-www-form-urlencoded; charset=" . get_option('blog_charset') . "\r\n";
	$http_request .= "Content-Length: " . strlen($request) . "\r\n";
	$http_request .= "User-Agent: WordPress/$wp_version | Akismet/$akismet_version\r\n";
	$http_request .= "\r\n";
	$http_request .= $request;
	
	$http_host = $host;
	// use a specific IP if provided - needed by akismet_check_server_connectivity()
	if ( $ip && long2ip(ip2long($ip)) ) {
		$http_host = $ip;
	} else {
		$http_host = akismet_get_host($host);
	}

	$response = '';
	if( false != ( $fs = @fsockopen($http_host, $port, $errno, $errstr, 10) ) ) {
		fwrite($fs, $http_request);

		while ( !feof($fs) )
			$response .= fgets($fs, 1160); // One TCP-IP packet
		fclose($fs);
		$response = explode("\r\n\r\n", $response, 2);
	}
	return $response;
}

function akismet_auto_check_comment( $comment ) {
	global $akismet_api_host, $akismet_api_port;

	$comment['user_ip']    = preg_replace( '/[^0-9., ]/', '', $_SERVER['REMOTE_ADDR'] );
	$comment['user_agent'] = $_SERVER['HTTP_USER_AGENT'];
	$comment['referrer']   = $_SERVER['HTTP_REFERER'];
	$comment['blog']       = get_option('home');
	$comment['blog_lang']  = get_locale();
	$comment['blog_charset'] = get_option('blog_charset');
	$comment['permalink']  = get_permalink($comment['comment_post_ID']);

	$ignore = array( 'HTTP_COOKIE' );

	foreach ( $_SERVER as $key => $value )
		if ( !in_array( $key, $ignore ) && is_string($value) )
			$comment["$key"] = $value;

	$query_string = '';
	foreach ( $comment as $key => $data )
		$query_string .= $key . '=' . urlencode( stripslashes($data) ) . '&';

	$response = akismet_http_post($query_string, $akismet_api_host, '/1.1/comment-check', $akismet_api_port);
	if ( 'true' == $response[1] ) {
		add_filter('pre_comment_approved', create_function('$a', 'return \'spam\';'));
		update_option( 'akismet_spam_count', get_option('akismet_spam_count') + 1 );

		do_action( 'akismet_spam_caught' );

		$post = get_post( $comment['comment_post_ID'] );
		$last_updated = strtotime( $post->post_modified_gmt );
		$diff = time() - $last_updated;
		$diff = $diff / 86400;

		if ( $post->post_type == 'post' && $diff > 30 && get_option( 'akismet_discard_month' ) == 'true' )
			die;
	}
	akismet_delete_old();
	return $comment;
}

function akismet_delete_old() {
	global $wpdb;
	$now_gmt = current_time('mysql', 1);
	$wpdb->query("DELETE FROM $wpdb->comments WHERE DATE_SUB('$now_gmt', INTERVAL 15 DAY) > comment_date_gmt AND comment_approved = 'spam'");
	$n = mt_rand(1, 5000);
	if ( $n == 11 ) // lucky number
		$wpdb->query("OPTIMIZE TABLE $wpdb->comments");
}

function akismet_submit_nonspam_comment ( $comment_id ) {
	global $wpdb, $akismet_api_host, $akismet_api_port, $current_user, $current_site;
	$comment_id = (int) $comment_id;
	
	$comment = $wpdb->get_row("SELECT * FROM $wpdb->comments WHERE comment_ID = '$comment_id'");
	if ( !$comment ) // it was deleted
		return;
	$comment->blog = get_option('home');
	$comment->blog_lang = get_locale();
	$comment->blog_charset = get_option('blog_charset');
	$comment->permalink = get_permalink($comment->comment_post_ID);
	if ( is_object($current_user) ) {
	    $comment->reporter = $current_user->user_login;
	}
	if ( is_object($current_site) ) {
		$comment->site_domain = $current_site->domain;
	}
	$query_string = '';
	foreach ( $comment as $key => $data )
		$query_string .= $key . '=' . urlencode( stripslashes($data) ) . '&';

	$response = akismet_http_post($query_string, $akismet_api_host, "/1.1/submit-ham", $akismet_api_port);
}

function akismet_submit_spam_comment ( $comment_id ) {
	global $wpdb, $akismet_api_host, $akismet_api_port, $current_user, $current_site;
	$comment_id = (int) $comment_id;

	$comment = $wpdb->get_row("SELECT * FROM $wpdb->comments WHERE comment_ID = '$comment_id'");
	if ( !$comment ) // it was deleted
		return;
	if ( 'spam' != $comment->comment_approved )
		return;
	$comment->blog = get_option('home');
	$comment->blog_lang = get_locale();
	$comment->blog_charset = get_option('blog_charset');
	$comment->permalink = get_permalink($comment->comment_post_ID);
	if ( is_object($current_user) ) {
	    $comment->reporter = $current_user->user_login;
	}
	if ( is_object($current_site) ) {
		$comment->site_domain = $current_site->domain;
	}
	$query_string = '';
	foreach ( $comment as $key => $data )
		$query_string .= $key . '=' . urlencode( stripslashes($data) ) . '&';

	$response = akismet_http_post($query_string, $akismet_api_host, "/1.1/submit-spam", $akismet_api_port);
}

add_action('wp_set_comment_status', 'akismet_submit_spam_comment');
add_action('edit_comment', 'akismet_submit_spam_comment');
add_action('preprocess_comment', 'akismet_auto_check_comment', 1);

function akismet_spamtoham( $comment ) { akismet_submit_nonspam_comment( $comment->comment_ID ); }
add_filter( 'comment_spam_to_approved', 'akismet_spamtoham' );

// Total spam in queue
// get_option( 'akismet_spam_count' ) is the total caught ever
function akismet_spam_count( $type = false ) {
	global $wpdb;

	if ( !$type ) { // total
		$count = wp_cache_get( 'akismet_spam_count', 'widget' );
		if ( false === $count ) {
			if ( function_exists('wp_count_comments') ) {
				$count = wp_count_comments();
				$count = $count->spam;
			} else {
				$count = (int) $wpdb->get_var("SELECT COUNT(comment_ID) FROM $wpdb->comments WHERE comment_approved = 'spam'");
			}
			wp_cache_set( 'akismet_spam_count', $count, 'widget', 3600 );
		}
		return $count;
	} elseif ( 'comments' == $type || 'comment' == $type ) { // comments
		$type = '';
	} else { // pingback, trackback, ...
		$type  = $wpdb->escape( $type );
	}

	return (int) $wpdb->get_var("SELECT COUNT(comment_ID) FROM $wpdb->comments WHERE comment_approved = 'spam' AND comment_type='$type'");
}

function akismet_spam_comments( $type = false, $page = 1, $per_page = 50 ) {
	global $wpdb;

	$page = (int) $page;
	if ( $page < 2 )
		$page = 1;

	$per_page = (int) $per_page;
	if ( $per_page < 1 )
		$per_page = 50;

	$start = ( $page - 1 ) * $per_page;
	$end = $start + $per_page;

	if ( $type ) {
		if ( 'comments' == $type || 'comment' == $type )
			$type = '';
		else
			$type = $wpdb->escape( $type );
		return $wpdb->get_results( "SELECT * FROM $wpdb->comments WHERE comment_approved = 'spam' AND comment_type='$type' ORDER BY comment_date DESC LIMIT $start, $end");
	}

	// All
	return $wpdb->get_results( "SELECT * FROM $wpdb->comments WHERE comment_approved = 'spam' ORDER BY comment_date DESC LIMIT $start, $end");
}

// Totals for each comment type
// returns array( type => count, ... )
function akismet_spam_totals() {
	global $wpdb;
	$totals = $wpdb->get_results( "SELECT comment_type, COUNT(*) AS cc FROM $wpdb->comments WHERE comment_approved = 'spam' GROUP BY comment_type" );
	$return = array();
	foreach ( $totals as $total )
		$return[$total->comment_type ? $total->comment_type : 'comment'] = $total->cc;
	return $return;
}

function akismet_manage_page() {
	global $wpdb, $submenu, $wp_db_version;

	// WP 2.7 has its own spam management page
	if ( 8645 <= $wp_db_version )
		return;

	$count = sprintf(__('Akismet Spam (%s)'), akismet_spam_count());
	if ( isset( $submenu['edit-comments.php'] ) )
		add_submenu_page('edit-comments.php', __('Akismet Spam'), $count, 'moderate_comments', 'akismet-admin', 'akismet_caught' );
	elseif ( function_exists('add_management_page') )
		add_management_page(__('Akismet Spam'), $count, 'moderate_comments', 'akismet-admin', 'akismet_caught');
}

function akismet_caught() {
	global $wpdb, $comment, $akismet_caught, $akismet_nonce;

	akismet_recheck_queue();
	if (isset($_POST['submit']) && 'recover' == $_POST['action'] && ! empty($_POST['not_spam'])) {
		check_admin_referer( $akismet_nonce );
		if ( function_exists('current_user_can') && !current_user_can('moderate_comments') )
			die(__('You do not have sufficient permission to moderate comments.'));

		$i = 0;
		foreach ($_POST['not_spam'] as $comment):
			$comment = (int) $comment;
			if ( function_exists('wp_set_comment_status') )
				wp_set_comment_status($comment, 'approve');
			else
				$wpdb->query("UPDATE $wpdb->comments SET comment_approved = '1' WHERE comment_ID = '$comment'");
			akismet_submit_nonspam_comment($comment);
			++$i;
		endforeach;
		$to = add_query_arg( 'recovered', $i, $_SERVER['HTTP_REFERER'] );
		wp_redirect( $to );
		exit;
	}
	if ('delete' == $_POST['action']) {
		check_admin_referer( $akismet_nonce );
		if ( function_exists('current_user_can') && !current_user_can('moderate_comments') )
			die(__('You do not have sufficient permission to moderate comments.'));

		$delete_time = $wpdb->escape( $_POST['display_time'] );
		$nuked = $wpdb->query( "DELETE FROM $wpdb->comments WHERE comment_approved = 'spam' AND '$delete_time' > comment_date_gmt" );
		wp_cache_delete( 'akismet_spam_count', 'widget' );
		$to = add_query_arg( 'deleted', 'all', $_SERVER['HTTP_REFERER'] );
		wp_redirect( $to );
		exit;
	}

if ( isset( $_GET['recovered'] ) ) {
	$i = (int) $_GET['recovered'];
	echo '<div class="updated"><p>' . sprintf(__('%1$s comments recovered.'), $i) . "</p></div>";
}

if (isset( $_GET['deleted'] ) )
	echo '<div class="updated"><p>' . __('All spam deleted.') . '</p></div>';

if ( isset( $GLOBALS['submenu']['edit-comments.php'] ) )
	$link = 'edit-comments.php';
else
	$link = 'edit.php';
?>
<style type="text/css">
.akismet-tabs {
	list-style: none;
	margin: 0;
	padding: 0;
	clear: both;
	border-bottom: 1px solid #ccc;
	height: 31px;
	margin-bottom: 20px;
	background: #ddd;
	border-top: 1px solid #bdbdbd;
}
.akismet-tabs li {
	float: left;
	margin: 5px 0 0 20px;
}
.akismet-tabs a {
	display: block;
	padding: 4px .5em 3px;
	border-bottom: none;
	color: #036;
}
.akismet-tabs .active a {
	background: #fff;
	border: 1px solid #ccc;
	border-bottom: none;
	color: #000;
	font-weight: bold;
	padding-bottom: 4px;
}
#akismetsearch {
	float: right;
	margin-top: -.5em;
}

#akismetsearch p {
	margin: 0;
	padding: 0;
}
</style>
<div class="wrap">
<h2><?php _e('Caught Spam') ?></h2>
<?php
$count = get_option( 'akismet_spam_count' );
if ( $count ) {
?>
<p><?php printf(__('Akismet has caught <strong>%1$s spam</strong> for you since you first installed it.'), number_format_i18n($count) ); ?></p>
<?php
}

$spam_count = akismet_spam_count();

if ( 0 == $spam_count ) {
	echo '<p>'.__('You have no spam currently in the queue. Must be your lucky day. :)').'</p>';
	echo '</div>';
} else {
	echo '<p>'.__('You can delete all of the spam from your database with a single click. This operation cannot be undone, so you may wish to check to ensure that no legitimate comments got through first. Spam is automatically deleted after 15 days, so don&#8217;t sweat it.').'</p>';
?>
<?php if ( !isset( $_POST['s'] ) ) { ?>
<form method="post" action="<?php echo attribute_escape( add_query_arg( 'noheader', 'true' ) ); ?>">
<?php akismet_nonce_field($akismet_nonce) ?>
<input type="hidden" name="action" value="delete" />
<?php printf(__('There are currently %1$s comments identified as spam.'), $spam_count); ?>&nbsp; &nbsp; <input type="submit" class="button delete" name="Submit" value="<?php _e('Delete all'); ?>" />
<input type="hidden" name="display_time" value="<?php echo current_time('mysql', 1); ?>" />
</form>
<?php } ?>
</div>
<div class="wrap">
<?php if ( isset( $_POST['s'] ) ) { ?>
<h2><?php _e('Search'); ?></h2>
<?php } else { ?>
<?php echo '<p>'.__('These are the latest comments identified as spam by Akismet. If you see any mistakes, simply mark the comment as "not spam" and Akismet will learn from the submission. If you wish to recover a comment from spam, simply select the comment, and click Not Spam. After 15 days we clean out the junk for you.').'</p>'; ?>
<?php } ?>
<?php
if ( isset( $_POST['s'] ) ) {
	$s = $wpdb->escape($_POST['s']);
	$comments = $wpdb->get_results("SELECT * FROM $wpdb->comments  WHERE
		(comment_author LIKE '%$s%' OR
		comment_author_email LIKE '%$s%' OR
		comment_author_url LIKE ('%$s%') OR
		comment_author_IP LIKE ('%$s%') OR
		comment_content LIKE ('%$s%') ) AND
		comment_approved = 'spam'
		ORDER BY comment_date DESC");
} else {
	if ( isset( $_GET['apage'] ) )
		$page = (int) $_GET['apage'];
	else
		$page = 1;

	if ( $page < 2 )
		$page = 1;

	$current_type = false;
	if ( isset( $_GET['ctype'] ) )
		$current_type = preg_replace( '|[^a-z]|', '', $_GET['ctype'] );

	$comments = akismet_spam_comments( $current_type, $page );
	$total = akismet_spam_count( $current_type );
	$totals = akismet_spam_totals();
?>
<ul class="akismet-tabs">
<li <?php if ( !isset( $_GET['ctype'] ) ) echo ' class="active"'; ?>><a href="edit-comments.php?page=akismet-admin"><?php _e('All'); ?></a></li>
<?php
foreach ( $totals as $type => $type_count ) {
	if ( 'comment' == $type ) {
		$type = 'comments';
		$show = __('Comments');
	} else {
		$show = ucwords( $type );
	}
	$type_count = number_format_i18n( $type_count );
	$extra = $current_type === $type ? ' class="active"' : '';
	echo "<li $extra><a href='edit-comments.php?page=akismet-admin&amp;ctype=$type'>$show ($type_count)</a></li>";
}
do_action( 'akismet_tabs' ); // so plugins can add more tabs easily
?>
</ul>
<?php
}

if ($comments) {
?>
<form method="post" action="<?php echo attribute_escape("$link?page=akismet-admin"); ?>" id="akismetsearch">
<p>  <input type="text" name="s" value="<?php if (isset($_POST['s'])) echo attribute_escape($_POST['s']); ?>" size="17" />
  <input type="submit" class="button" name="submit" value="<?php echo attribute_escape(__('Search Spam &raquo;')) ?>"  />  </p>
</form>
<?php if ( $total > 50 ) {
$total_pages = ceil( $total / 50 );
$r = '';
if ( 1 < $page ) {
	$args['apage'] = ( 1 == $page - 1 ) ? '' : $page - 1;
	$r .=  '<a class="prev" href="' . clean_url(add_query_arg( $args )) . '">'. __('&laquo; Previous Page') .'</a>' . "\n";
}
if ( ( $total_pages = ceil( $total / 50 ) ) > 1 ) {
	for ( $page_num = 1; $page_num <= $total_pages; $page_num++ ) :
		if ( $page == $page_num ) :
			$r .=  "<strong>$page_num</strong>\n";
		else :
			$p = false;
			if ( $page_num < 3 || ( $page_num >= $page - 3 && $page_num <= $page + 3 ) || $page_num > $total_pages - 3 ) :
				$args['apage'] = ( 1 == $page_num ) ? '' : $page_num;
				$r .= '<a class="page-numbers" href="' . clean_url(add_query_arg($args)) . '">' . ( $page_num ) . "</a>\n";
				$in = true;
			elseif ( $in == true ) :
				$r .= "...\n";
				$in = false;
			endif;
		endif;
	endfor;
}
if ( ( $page ) * 50 < $total || -1 == $total ) {
	$args['apage'] = $page + 1;
	$r .=  '<a class="next" href="' . clean_url(add_query_arg($args)) . '">'. __('Next Page &raquo;') .'</a>' . "\n";
}
echo "<p>$r</p>";
?>

<?php } ?>
<form style="clear: both;" method="post" action="<?php echo attribute_escape( add_query_arg( 'noheader', 'true' ) ); ?>">
<?php akismet_nonce_field($akismet_nonce) ?>
<input type="hidden" name="action" value="recover" />
<ul id="spam-list" class="commentlist" style="list-style: none; margin: 0; padding: 0;">
<?php
$i = 0;
foreach($comments as $comment) {
	$i++;
	$comment_date = mysql2date(get_option("date_format") . " @ " . get_option("time_format"), $comment->comment_date);
	$post = get_post($comment->comment_post_ID);
	$post_title = $post->post_title;
	if ($i % 2) $class = 'class="alternate"';
	else $class = '';
	echo "\n\t<li id='comment-$comment->comment_ID' $class>";
	?>

<p><strong><?php comment_author() ?></strong> <?php if ($comment->comment_author_email) { ?>| <?php comment_author_email_link() ?> <?php } if ($comment->comment_author_url && 'http://' != $comment->comment_author_url) { ?> | <?php comment_author_url_link() ?> <?php } ?>| <?php _e('IP:') ?> <a href="http://ws.arin.net/cgi-bin/whois.pl?queryinput=<?php comment_author_IP() ?>"><?php comment_author_IP() ?></a></p>

<?php comment_text() ?>

<p><label for="spam-<?php echo $comment->comment_ID; ?>">
<input type="checkbox" id="spam-<?php echo $comment->comment_ID; ?>" name="not_spam[]" value="<?php echo $comment->comment_ID; ?>" />
<?php _e('Not Spam') ?></label> &#8212; <?php comment_date('M j, g:i A');  ?> &#8212; [
<?php
$post = get_post($comment->comment_post_ID);
$post_title = wp_specialchars( $post->post_title, 'double' );
$post_title = ('' == $post_title) ? "# $comment->comment_post_ID" : $post_title;
?>
 <a href="<?php echo get_permalink($comment->comment_post_ID); ?>" title="<?php echo $post_title; ?>"><?php _e('View Post') ?></a> ] </p>


<?php
}
?>
</ul>
<?php if ( $total > 50 ) {
$total_pages = ceil( $total / 50 );
$r = '';
if ( 1 < $page ) {
	$args['apage'] = ( 1 == $page - 1 ) ? '' : $page - 1;
	$r .=  '<a class="prev" href="' . clean_url(add_query_arg( $args )) . '">'. __('&laquo; Previous Page') .'</a>' . "\n";
}
if ( ( $total_pages = ceil( $total / 50 ) ) > 1 ) {
	for ( $page_num = 1; $page_num <= $total_pages; $page_num++ ) :
		if ( $page == $page_num ) :
			$r .=  "<strong>$page_num</strong>\n";
		else :
			$p = false;
			if ( $page_num < 3 || ( $page_num >= $page - 3 && $page_num <= $page + 3 ) || $page_num > $total_pages - 3 ) :
				$args['apage'] = ( 1 == $page_num ) ? '' : $page_num;
				$r .= '<a class="page-numbers" href="' . clean_url(add_query_arg($args)) . '">' . ( $page_num ) . "</a>\n";
				$in = true;
			elseif ( $in == true ) :
				$r .= "...\n";
				$in = false;
			endif;
		endif;
	endfor;
}
if ( ( $page ) * 50 < $total || -1 == $total ) {
	$args['apage'] = $page + 1;
	$r .=  '<a class="next" href="' . clean_url(add_query_arg($args)) . '">'. __('Next Page &raquo;') .'</a>' . "\n";
}
echo "<p>$r</p>";
}
?>
<p class="submit">
<input type="submit" name="submit" value="<?php echo attribute_escape(__('De-spam marked comments &raquo;')); ?>" />
</p>
<p><?php _e('Comments you de-spam will be submitted to Akismet as mistakes so it can learn and get better.'); ?></p>
</form>
<?php
} else {
?>
<p><?php _e('No results found.'); ?></p>
<?php } ?>

<?php if ( !isset( $_POST['s'] ) ) { ?>
<form method="post" action="<?php echo attribute_escape( add_query_arg( 'noheader', 'true' ) ); ?>">
<?php akismet_nonce_field($akismet_nonce) ?>
<p><input type="hidden" name="action" value="delete" />
<?php printf(__('There are currently %1$s comments identified as spam.'), $spam_count); ?>&nbsp; &nbsp; <input type="submit" name="Submit" class="button" value="<?php echo attribute_escape(__('Delete all')); ?>" />
<input type="hidden" name="display_time" value="<?php echo current_time('mysql', 1); ?>" /></p>
</form>
<?php } ?>
</div>
<?php
	}
}

add_action('admin_menu', 'akismet_manage_page');

// WP < 2.5
function akismet_stats() {
	if ( !function_exists('did_action') || did_action( 'rightnow_end' ) ) // We already displayed this info in the "Right Now" section
		return;
	if ( !$count = get_option('akismet_spam_count') )
		return;
	$path = plugin_basename(__FILE__);
	echo '<h3>'.__('Spam').'</h3>';
	global $submenu;
	if ( isset( $submenu['edit-comments.php'] ) )
		$link = 'edit-comments.php';
	else
		$link = 'edit.php';
	echo '<p>'.sprintf(__('<a href="%1$s">Akismet</a> has protected your site from <a href="%2$s">%3$s spam comments</a>.'), 'http://akismet.com/', clean_url("$link?page=akismet-admin"), number_format_i18n($count) ).'</p>';
}

add_action('activity_box_end', 'akismet_stats');

// WP 2.5+
function akismet_rightnow() {
	global $submenu, $wp_db_version;

	if ( 8645 < $wp_db_version  ) // 2.7
		$link = 'edit-comments.php?comment_status=spam';
	elseif ( isset( $submenu['edit-comments.php'] ) )
		$link = 'edit-comments.php?page=akismet-admin';
	else
		$link = 'edit.php?page=akismet-admin';

	if ( $count = get_option('akismet_spam_count') ) {
		$intro = sprintf( __ngettext(
			'<a href="%1$s">Akismet</a> has protected your site from %2$s spam comment already,',
			'<a href="%1$s">Akismet</a> has protected your site from %2$s spam comments already,',
			$count
		), 'http://akismet.com/', number_format_i18n( $count ) );
	} else {
		$intro = sprintf( __('<a href="%1$s">Akismet</a> blocks spam from getting to your blog,'), 'http://akismet.com/' );
	}

	if ( $queue_count = akismet_spam_count() ) {
		$queue_text = sprintf( __ngettext(
			'and there\'s <a href="%2$s">%1$s comment</a> in your spam queue right now.',
			'and there are <a href="%2$s">%1$s comments</a> in your spam queue right now.',
			$queue_count
		), number_format_i18n( $queue_count ), clean_url($link) );
	} else {
		$queue_text = sprintf( __( "but there's nothing in your <a href='%1\$s'>spam queue</a> at the moment." ), clean_url($link) );
	}

	$text = sprintf( _c( '%1$s %2$s|akismet_rightnow' ), $intro, $queue_text );

	echo "<p class='akismet-right-now'>$text</p>\n";
}
	
add_action('rightnow_end', 'akismet_rightnow');

// For WP <= 2.3.x
if ( 'moderation.php' == $pagenow ) {
	function akismet_recheck_button( $page ) {
		global $submenu;
		if ( isset( $submenu['edit-comments.php'] ) )
			$link = 'edit-comments.php';
		else
			$link = 'edit.php';
		$button = "<a href='$link?page=akismet-admin&amp;recheckqueue=true&amp;noheader=true' style='display: block; width: 100px; position: absolute; right: 7%; padding: 5px; font-size: 14px; text-decoration: underline; background: #fff; border: 1px solid #ccc;'>" . __('Recheck Queue for Spam') . "</a>";
		$page = str_replace( '<div class="wrap">', '<div class="wrap">' . $button, $page );
		return $page;
	}

	if ( $wpdb->get_var( "SELECT COUNT(*) FROM $wpdb->comments WHERE comment_approved = '0'" ) )
		ob_start( 'akismet_recheck_button' );
}

// For WP >= 2.5
function akismet_check_for_spam_button($comment_status) {
	if ( 'approved' == $comment_status )
		return;
	if ( function_exists('plugins_url') )
		$link = 'admin.php?action=akismet_recheck_queue';
	else
		$link = 'edit-comments.php?page=akismet-admin&amp;recheckqueue=true&amp;noheader=true';
	echo "</div><div class='alignleft'><a class='button-secondary checkforspam' href='$link'>" . __('Check for Spam') . "</a>";
}
add_action('manage_comments_nav', 'akismet_check_for_spam_button');

function akismet_recheck_queue() {
	global $wpdb, $akismet_api_host, $akismet_api_port;

	if ( ! ( isset( $_GET['recheckqueue'] ) || ( isset( $_REQUEST['action'] ) && 'akismet_recheck_queue' == $_REQUEST['action'] ) ) )
		return;

	$moderation = $wpdb->get_results( "SELECT * FROM $wpdb->comments WHERE comment_approved = '0'", ARRAY_A );
	foreach ( (array) $moderation as $c ) {
		$c['user_ip']    = $c['comment_author_IP'];
		$c['user_agent'] = $c['comment_agent'];
		$c['referrer']   = '';
		$c['blog']       = get_option('home');
		$c['blog_lang']  = get_locale();
		$c['blog_charset'] = get_option('blog_charset');
		$c['permalink']  = get_permalink($c['comment_post_ID']);
		$id = (int) $c['comment_ID'];

		$query_string = '';
		foreach ( $c as $key => $data )
		$query_string .= $key . '=' . urlencode( stripslashes($data) ) . '&';

		$response = akismet_http_post($query_string, $akismet_api_host, '/1.1/comment-check', $akismet_api_port);
		if ( 'true' == $response[1] ) {
			$wpdb->query( "UPDATE $wpdb->comments SET comment_approved = 'spam' WHERE comment_ID = $id" );
		}
	}
	wp_redirect( $_SERVER['HTTP_REFERER'] );
	exit;
}

add_action('admin_action_akismet_recheck_queue', 'akismet_recheck_queue');

function akismet_check_db_comment( $id ) {
	global $wpdb, $akismet_api_host, $akismet_api_port;

	$id = (int) $id;
	$c = $wpdb->get_row( "SELECT * FROM $wpdb->comments WHERE comment_ID = '$id'", ARRAY_A );
	if ( !$c )
		return;

	$c['user_ip']    = $c['comment_author_IP'];
	$c['user_agent'] = $c['comment_agent'];
	$c['referrer']   = '';
	$c['blog']       = get_option('home');
	$c['blog_lang']  = get_locale();
	$c['blog_charset'] = get_option('blog_charset');
	$c['permalink']  = get_permalink($c['comment_post_ID']);
	$id = $c['comment_ID'];

	$query_string = '';
	foreach ( $c as $key => $data )
	$query_string .= $key . '=' . urlencode( stripslashes($data) ) . '&';

	$response = akismet_http_post($query_string, $akismet_api_host, '/1.1/comment-check', $akismet_api_port);
	return $response[1];
}

// This option causes tons of FPs, was removed in 2.1
function akismet_kill_proxy_check( $option ) { return 0; }
add_filter('option_open_proxy_check', 'akismet_kill_proxy_check');

// Widget stuff
function widget_akismet_register() {
	if ( function_exists('register_sidebar_widget') ) :
	function widget_akismet($args) {
		extract($args);
		$options = get_option('widget_akismet');
		$count = number_format_i18n(get_option('akismet_spam_count'));
		?>
			<?php echo $before_widget; ?>
				<?php echo $before_title . $options['title'] . $after_title; ?>
				<div id="akismetwrap"><div id="akismetstats"><a id="aka" href="http://akismet.com" title=""><?php printf( __( '%1$s %2$sspam comments%3$s %4$sblocked by%5$s<br />%6$sAkismet%7$s' ), '<div id="akismet1"><span id="akismetcount">' . $count . '</span>', '<span id="akismetsc">', '</span></div>', '<div id="akismet2"><span id="akismetbb">', '</span>', '<span id="akismeta">', '</span></div>' ); ?></a></div></div>
			<?php echo $after_widget; ?>
	<?php
	}

	function widget_akismet_style() {
		?>
<style type="text/css">
#aka,#aka:link,#aka:hover,#aka:visited,#aka:active{color:#fff;text-decoration:none}
#aka:hover{border:none;text-decoration:none}
#aka:hover #akismet1{display:none}
#aka:hover #akismet2,#akismet1{display:block}
#akismet2{display:none;padding-top:2px}
#akismeta{font-size:16px;font-weight:bold;line-height:18px;text-decoration:none}
#akismetcount{display:block;font:15px Verdana,Arial,Sans-Serif;font-weight:bold;text-decoration:none}
#akismetwrap #akismetstats{background:url(<?php echo get_option('siteurl'); ?>/wp-content/plugins/akismet/akismet.gif) no-repeat top left;border:none;color:#fff;font:11px 'Trebuchet MS','Myriad Pro',sans-serif;height:40px;line-height:100%;overflow:hidden;padding:8px 0 0;text-align:center;width:120px}
</style>
		<?php
	}

	function widget_akismet_control() {
		$options = $newoptions = get_option('widget_akismet');
		if ( $_POST["akismet-submit"] ) {
			$newoptions['title'] = strip_tags(stripslashes($_POST["akismet-title"]));
			if ( empty($newoptions['title']) ) $newoptions['title'] = 'Spam Blocked';
		}
		if ( $options != $newoptions ) {
			$options = $newoptions;
			update_option('widget_akismet', $options);
		}
		$title = htmlspecialchars($options['title'], ENT_QUOTES);
	?>
				<p><label for="akismet-title"><?php _e('Title:'); ?> <input style="width: 250px;" id="akismet-title" name="akismet-title" type="text" value="<?php echo $title; ?>" /></label></p>
				<input type="hidden" id="akismet-submit" name="akismet-submit" value="1" />
	<?php
	}

	register_sidebar_widget('Akismet', 'widget_akismet', null, 'akismet');
	register_widget_control('Akismet', 'widget_akismet_control', null, 75, 'akismet');
	if ( is_active_widget('widget_akismet') )
		add_action('wp_head', 'widget_akismet_style');
	endif;
}

add_action('init', 'widget_akismet_register');

// Counter for non-widget users
function akismet_counter() {
?>
<style type="text/css">
#akismetwrap #aka,#aka:link,#aka:hover,#aka:visited,#aka:active{color:#fff;text-decoration:none}
#aka:hover{border:none;text-decoration:none}
#aka:hover #akismet1{display:none}
#aka:hover #akismet2,#akismet1{display:block}
#akismet2{display:none;padding-top:2px}
#akismeta{font-size:16px;font-weight:bold;line-height:18px;text-decoration:none}
#akismetcount{display:block;font:15px Verdana,Arial,Sans-Serif;font-weight:bold;text-decoration:none}
#akismetwrap #akismetstats{background:url(<?php echo get_option('siteurl'); ?>/wp-content/plugins/akismet/akismet.gif) no-repeat top left;border:none;color:#fff;font:11px 'Trebuchet MS','Myriad Pro',sans-serif;height:40px;line-height:100%;overflow:hidden;padding:8px 0 0;text-align:center;width:120px}
</style>
<?php
$count = number_format_i18n(get_option('akismet_spam_count'));
?>
<div id="akismetwrap"><div id="akismetstats"><a id="aka" href="http://akismet.com" title=""><div id="akismet1"><span id="akismetcount"><?php echo $count; ?></span> <span id="akismetsc"><?php _e('spam comments') ?></span></div> <div id="akismet2"><span id="akismetbb"><?php _e('blocked by') ?></span><br /><span id="akismeta">Akismet</span></div></a></div></div>
<?php
}

?>
