<?php 

/*
Plugin Name: Extended Comment Options
Plugin URI: http://beingmrkenny.co.uk/wordpress/plugins/extended-comment-options/
Description: This plugin allows you to switch comments and/or pings on or off for batches of existing posts.
Version: 2.0
Author: Mark Kenny
Author URI: http://beingmrkenny.co.uk/
*/

/*
	What happens when a time-unit arrangement is changed to posts -- unecessary.
	
	Comment count returns both comments and pings, set a way of closing when 10   [  ] Comments    [  ] Pings.
	SELECT COUNT(comment_ID) FROM $wpdb->comments WHERE post_ID = $post_ID AND comment_status = 'comment'
	
	New features:
		close comments on posts with more than X comments
		automatic upkeep of this
	Behind the scenes:
		Tightened security (more difficult to forge CSF)
		Polished a few of the messages
		Made the advanced section easier to use
		Changed the way dates are managed on Posts in the last
*/

define ('DEBUGGING', false); // Change to TRUE if you want to display information about the query.

// -------------------------------- Functions --------------------------------

$auto_check = ctype_digit(wp_next_scheduled('eco_auto'));

if (false == $auto_check) {
	$auto_check = ('posts' == get_option('eco_auto_units') || 'comments' == get_option('eco_auto_units')) ? true : false;
}

if ($auto_check == true) {
	$options = array(
		'which' => get_option('eco_auto_which'),
		'status' => get_option('eco_auto_status'),
		'last' => get_option('eco_auto_last'),
		'units' => get_option('eco_auto_units'),
		'db_excluded_posts' => get_option('eco_excluded_posts'),
		'next_time' => wp_next_scheduled('eco_auto')
	);
	$options['message_which'] = ($options['which'] == 'both') ? 'Comments and pings' : ucwords($options['which']);
	$options['opposite_status'] = ($options['status'] == 'open') ? 'closed' : 'open';
	$options['status_pp'] = ($options['status'] == 'open') ? 'opened' : 'closed';
	$options['opp_pp'] = ($options['opposite_status'] == 'open') ? 'opened' : 'closed';
}

function eco_extended_comments_options() {
	if (function_exists('add_options_page') )
		add_submenu_page('edit-comments.php', 'Extended Comment Options', 'Comments Status', 8, basename(__FILE__), 'comment_conf');
		// The first argument does into the <title>, the second goes on the tab in options
}

function eco_error($err_message) {
	global $eco_error;
	echo '<div class="error">';
	echo '<p>An error has occurred:</p>';
	echo "<p><strong>$err_message</strong></p>";
	echo '<p>No settings have been changed.</p>';
	echo '</div>';
	if (DEBUGGING === TRUE) { eco_debugging(); }
	include('admin-footer.php');
	die();
}

function date_from_last($time, $units) {
	/*
	switch ($units) {
		case 'days':
			$unixtime = time() - ($time * 86400);
			break;
		case 'weeks':
			$unixtime = time() - ($time * 604800);
			break;
		case 'months':
			// if X months ago is in this year
			if ( date('m') >= $time )	{
				$month = date('m') - ($time - 1);
				$year = date('Y');
				$unixtime = strtotime("$year-$month-01");
			// if X months ago is a multiple of 12 (24, 48, etc) then 1 or more years ago
			} elseif ( is_int($time / 12) ) {
				$year = date('Y') - ($time / 12);
				$month = date('m') - ($time / 12);
				$unixtime = strtotime("$year-$month-01");
			// date lies beyond current year
			} else {
				$months = $time % 12;
				$new_year = date(Y) - ( floor($time / 12) ); // this year / number of years
				if ( date('m') > $months ) {
					$new_month = date(m) - $months;
				} else {
					$new_month = $months + 12;
				}
				$unixtime = strtotime("$new_year-$new_month-01");
			}
			break;
		case 'years':
			$year = date(Y) - ( $time - 1 );
			$unixtime = strtotime("$year-01-01");
	} */
	return date('Y-m-d', strtotime("$time $units ago"));
}

function default_comments_status() {
	global $eco_clean;
	
	if ('for-last' == $eco_clean['for'] || 'oneclick-last' == $eco_clean['for'] || 'for-comments' == $eco_clean['for']) {
		$eco_clean['status'] = $eco_clean['opposite_status'];
	}
	
	if ($eco_clean['future'] === true || $eco_clean['for'] == 'for-new') {
		switch($eco_clean['which']) {
			case 'both':
				update_option('default_comment_status', $eco_clean['status']);
				update_option('default_ping_status', $eco_clean['status']);
				break;
			case 'comments':
				update_option('default_comment_status', $eco_clean['status']);
				break;
			case 'pings':
				update_option('default_ping_status', $eco_clean['status']);
				break;
		}
	}
	
}

function eco_query($which, $status, $horizon='', $criterion='', $excluded='') {
	global $wpdb;
	
	// Escape the strings, just in case!
	$which = $wpdb->escape($which);
	$status = $wpdb->escape($status);
	$horizon = $wpdb->escape($horizon);
	$criterion = $wpdb->escape($criterion);
	$excluded = $wpdb->escape($excluded);
	
	$sql = "UPDATE $wpdb->posts ";
	
	switch ($which) {
		case 'comments':
			$sql .= " SET `comment_status` = '$status' ";
			break;
		case 'pings':
			$sql .= " SET `ping_status` = '$status' ";
			break;
		case 'both':
			$sql .= " SET `comment_status` = '$status', `ping_status` = '$status' ";
	}
	
	$sql .= " WHERE `post_status` = 'publish' AND `post_type` = 'post' ";
	
	if ( $excluded != '' ) {
		$sql .= " AND `ID` NOT IN ($excluded) ";
	}
	
	if ( $horizon != '' ) {
		switch($horizon) {
			case 'before' :
				$sql .= " AND `post_date` < '$criterion' ";
				break;
			case 'after' :
				$sql .= " AND `post_date` > '$criterion' ";
				break;
			case 'last' :
				$sql .= " ORDER BY `post_date` DESC LIMIT $criterion ";
				break;
			case 'comments' :
				$sql .= " AND `comment_count` >= $criterion ";
				break;
		}
	}
	
	$wpdb->query($sql);
	
	if (DEBUGGING === TRUE) {
		if ( defined('ECO_QUERYVARS') ) {
			define ('ECO_QUERYVARS2', wp_specialchars("[$which] [$status] [$horizon] [$criterion] [$excluded]"));
		} else {
			define ('ECO_QUERYVARS', wp_specialchars("[$which] [$status] [$horizon] [$criterion] [$excluded]"));
		}
		
		if ( defined('ECO_SQL') ) {
			define ('ECO_SQL2', wp_specialchars($sql));
		} else {
			define ('ECO_SQL', wp_specialchars($sql));
		}
	}
	
}

function eco_auto() {
	global $wpdb, $options;
	
	if ( $options['units'] == 'posts' ) {
		// close all, then open the right ones
		eco_query($options['which'], $options['opposite_status'], null, null, $options['db_excluded_posts']);
		eco_query($options['which'], $options['status'], 'last', $options['last'], $options['db_excluded_posts']);
	} elseif ($options['units'] == 'comments') {
		// close all, then open the right ones
		eco_query($options['which'], $options['opposite_status'], null, null, $options['db_excluded_posts']);
		eco_query($options['which'], $options['status'], 'comments', $options['last'], $options['db_excluded_posts']);
	} else {
		$date = date_from_last($options['last'], $options['units']);
		eco_query($options['which'], $options['status'], 'after', $options['date'], $options['db_excluded_posts']);
		eco_query($options['which'], $options['opposite_status'], 'before', $options['date'], $options['db_excluded_posts']);
	}
	
}

function eco_cancel_auto() {
	global $wpdb;
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_which'");
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_status'");
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_last'");
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_units'");
	wp_clear_scheduled_hook('eco_auto');
}

// Do comments thing every time a new post/comment is published, but only if this has been requested
switch($options['units']) {
	case 'posts' :
		add_action('publish_post', 'eco_auto');
		break;
	case 'comments' :
		add_action('comment_post', 'eco_auto');
		add_action('pingback_post', 'eco_auto');
		break;
}

// Initialise $eco_clean;
// Set $eco_clean['submitted'] to true if the form has been submitted;
$eco_clean = array('auto-last' => false);
$eco_clean['db_excluded_posts'] = get_option('eco_excluded_posts');
$eco_clean['submitted'] = false;
if ( $_POST['submit'] == 'Update' || $_POST['for'] == 'oneclick-all' || $_POST['for'] == 'oneclick-last' || $_POST['for'] == 'cancel-auto') {
	$eco_clean['submitted'] = true;
}

$eco_html = array();
$eco_html['db_excluded_posts'] = wp_specialchars($eco_clean['db_excluded_posts']);

// This inserts the page's content
function comment_conf() {
global $wpdb, $eco_clean, $eco_message, $options, $eco_html, $auto_check, $options;
?>

<?php if ($eco_clean['submitted'] === true) : ?>
	
	<div id="message" class="updated fade">
		<p>Comment options received.</p>
	</div>
	
<?php elseif (true == $auto_check) : ?>
	
	<div id="message" class="updated">
		<p>You have scheduled <?php echo strtolower($options['message_which']); ?> to be <?php echo $options['status_pp']; ?> automatically.</p>
	</div>
	
<?php endif; ?>

<?php if (true == DEBUGGING) : ?>
<div class="wrap">
	<pre><?php print_r($options); ?></pre>
</div>
<?php endif; ?>

<?php if ( empty($eco_clean['submitted']) ) :
/*
 *
 *
 * ------------------------------------------------ THE FORM ---------------------------------------------------------
 *
 *
 *
 */

?>

<?php if (true == $auto_check) : ?>
	
<div class="wrap">
	
<h2>Automatic Schedule</h2>

<p>
	<?php
		if ('posts' == $options['units']) {
			echo
				"{$options['message_which']} will be {$options['opposite_status']} on the {$options['last']} most recent posts. "
				. " {$options['message_which']} on older posts will be {$options['status_pp']} each time a new post is published.";
		} elseif ('comments' == $options['units']) {
			echo
				"{$options['message_which']} will be {$options['status']} on posts once {$options['last']} comments and/or pings have been left on it.";
		} else {
			if ('1' == $options['last']) {
				$options['units'] = rtrim($options['units'], 's'); // cut off the last S if there's only 1!
			}
			echo
			//Comments and pings on posts older than 6 days will be closed automatically.
				"{$options['message_which']} on posts older than"
				. " {$options['last']} {$options['units']}"
				. " will be {$options['status_pp']} automatically. The next scheduled check will be at ". date('G:i, j M Y', $options['next_time']) . '.';
		}
	?> Cancelling this arrangement does not change the status of comments and pings on any posts.
</p>
<form action="" method="post">
	<?php
		if ( function_exists('wp_nonce_field') ) {
			wp_nonce_field('eco-submitform');
		}
	?>
	<input type="hidden" name="for" value="cancel-auto" />
	<p class="submit"><input type="submit" name="cancel-arrangement" value="Cancel this arrangement &raquo;"></p>
</form>

</div>

<?php endif; ?>

<div class="wrap">
 
<h2>Simple Settings</h2>

<!-- fieldsets are used here so that the admin page retains good layout when Tiger Admin is used -->

<script type="text/javascript"><!--
// from http://www.somacon.com/p143.php
function setCheckedValue(radioObj, newValue) {
	if(!radioObj)
		return;
	var radioLength = radioObj.length;
	if(radioLength == undefined) {
		radioObj.checked = (radioObj.value == newValue.toString());
		return;
	}
	for(var i = 0; i < radioLength; i++) {
		radioObj[i].checked = false;
		if(radioObj[i].value == newValue.toString()) {
			radioObj[i].checked = true;
		}
	}
}
//--></script>

<form action="" method="post">
	
	<?php
		if ( function_exists('wp_nonce_field') ) {
			wp_nonce_field('eco-submitform');
		}
	?>
	
<fieldset>
	<h3>One-Click</h3>
	<p>This option applies to every single post, and cancels any automatic arrangment you have made.
		If you want to exclude certain posts, you should use the &#8220;Advanced Settings&#8221; form below.</p>
	<p>
		<strong>All</strong> comments and pings:
		<input name="for" value="oneclick-all" type="hidden" />
		<span class="submit">
			<input name="status" value="Open" type="submit" />
			<input name="status" value="Closed" type="submit" />
		</span>
	</p>
</fieldset>
</form>

<form action="" method="post">
	
	<?php
		if ( function_exists('wp_nonce_field') ) {
			wp_nonce_field('eco-submitform');
		}
	?>
	
<fieldset style="margin-top: 2em;">
	<h3>Posts in the last&hellip;</h3>
	<p>This option respects your excluded posts setting, if you have entered it below.</p>
	<p>
		<input name="for" value="oneclick-last" type="hidden" />
		<input name="posts" value="<?php echo $eco_html['db_excluded_posts']; ?>" type="hidden" />
		Open comments and pings for the last
		<input name="last" value="5" type="text" size="5" maxlength="5" />
		<select name="units">
			<option value="posts">Posts</option>
			<option value="days">Days</option>
			<option value="weeks">Weeks</option>
			<option value="months">Months</option>
			<option value="years">Years</option>
		</select>
		 , then close the rest.
	</p>
	<p>
		<input name="oneclick-auto" type="checkbox" id="oneclick-auto" value="auto" /> <label for="oneclick-auto">Apply this automatically from now on.</label>
	</p>
	<p>
		<span class="submit">
			<input name="oneclick" value="Update" type="submit" />
		</span>
	</p>
</fieldset>
</form>

</div>

<div class="wrap">

<form action="" method="post" name="advanced">
	
	<?php
		if ( function_exists('wp_nonce_field') ) {
			wp_nonce_field('eco-submitform');
		}
	?>

<h2>Advanced Settings</h2>

<fieldset class="options">
	
	<table width="100%" cellspacing="2" cellpadding="5" class="editform">
	
	<tr valign="top">
		<th width="33%" scope="row">Set the status to:</th>
		<td>
			<input name="status" value="open" id="on" type="radio" /> <label for="on">Open (on)</label><br />
			<input name="status" value="closed" id="off" type="radio" checked="checked" /> <label for="off">Closed (off)</label>
		</td>
	</tr>
	
	<tr valign="top">
		<th width="33%" scope="row">Apply this to:</th>
		<td>
			<input name="comments" value="comments" id="comments" type="checkbox" checked="checked" /> <label for="comments">Comments.</label><br />
			<input name="pings" value="pings" id="pings" type="checkbox" checked="checked" /> <label for="pings">Pings (and Trackbacks).</label>
		</td>
	</tr>
	
	<tr valign="top">
		<th width="33%" scope="row">Change the default status of new posts?</th>
		<td>
			<input name="future" id="future" value="future" type="checkbox" checked="checked" />
			<label for="future">Yes</label>
		</td>
	</tr>
	
	<tr valign="top">
		<th width="33%" scope="row">Which posts?</th>
		<td>
			<!-- Option 1 -->
			<p style="margin-top: 0;">
				<input name="for" value="for-new" id="for-new" type="radio" /> <label for="for-new">No existing posts, just change default status for new posts.</label>
			</p>
			<!-- Option 2 -->
			<p>
				<input name="for" value="for-all" id="for-all" type="radio" checked="checked" /> <label for="for-all">All existing posts</label>
			</p>
			<!-- Option 3 -->
			<p>
				<input name="for" value="for-bora" id="for-bora" type="radio" /> <label for="for-bora">All posts made</label>
				
				<span onclick="setCheckedValue(document.forms['advanced'].elements['for'], 'for-bora')">
				
					<select name="beforeafter">
						<option value="before" selected="selected">Before</option>
						<option value="after">After</option>
					</select> :
					
					<input name="day" value="01" type="text" maxlength="2" size="2" id="day" title="Enter the day (2 digits)" />
					<select name="month">
						<option value="01">January</option>
						<option value="02">February</option>
						<option value="03">March</option>
						<option value="04">April</option>
						<option value="05">May</option>
						<option value="06">June</option>
						<option value="07">July</option>
						<option value="08">August</option>
						<option value="09">September</option>
						<option value="10">October</option>
						<option value="11">November</option>
						<option value="12">December</option>
					</select>
					<input name="year" value="<?php echo date('Y') ?>" type="text" maxlength="4" size="4" id="year" title="Enter the year (4 digits)" />
					
				</span>
			</p>
			
			<!-- Option 4 -->
			<p>
				<input name="for" value="for-last" id="for-last" type="radio" /> <label for="for-last">All but the last</label>
				<span onclick="setCheckedValue(document.forms['advanced'].elements['for'], 'for-last')">
				<input name="last" value="5" id="last" type="text" maxlength="5" size="5" />
				<select name="units">
					<option value="posts">Posts</option>
					<option value="days">Days</option>
					<option value="weeks">Weeks</option>
					<option value="months">Months</option>
					<option value="years">Years</option>
				</select>
				</span><br />
				<input name="for-last-auto" type="checkbox" id="for-last-auto" value="auto" style="margin: 5px 0 0 20px;" /> 
				<label for="for-last-auto">Apply this automatically from now on.</label>
			</p>
			
			<!-- Option 5 -->
			<p>
				<input name="for" value="for-comments" id="for-comments" type="radio" /> <label for="for-comments">Posts with more than</label>
				<span onclick="setCheckedValue(document.forms['advanced'].elements['for'], 'for-comments')">
				<input name="number-comments" value="10" id="number-comments" type="text" maxlength="5" size="5" />
				</span>
				<label for="for-comments">comments and pings</label><br />
				<input name="for-comments-auto" type="checkbox" id="for-comments-auto" value="auto" style="margin-left: 20px;" />
				<label for="for-comments-auto">Apply this automatically from now on.</label>
			</p>
		</td>
	</tr>
	
	<tr valign="top">
		<th width="33%" scope="row">
			Exclude posts?
		</th>
		<td>
			<input name="posts" type="text" style="font-family:monospace;" value="<?php echo $eco_html['db_excluded_posts']; ?>" size="50" />
			<p>
				If you have a popular post that draws a lot of comments and you want to leave comments
				open on it, you can enter its ID here. If you have more than one, enter all the IDs separated by a
				<strong>comma</strong>. For example: <kbd>12,34,129</kbd>.
			</p>				
			
			<p>
				IDs are the numbers that WordPress assigns to each post. They can be found in the first column of
				the &#8216;Manage posts&#8217; page. The IDs you enter are saved and will reappear
				the next time you load this page. If you want to include a post again, just delete its ID.
				(You may have to update its comments status manually.)
			</p>
		</td>
	</tr>
	
	</table>
	
</fieldset>
		
	<p class="submit">
		<input name="submit" value="Update" type="submit" />
	</p>
	
</form>

</div>

<?php // OTHERWISE IF FORM HAS BEEN SUBMITTED :
elseif ( $eco_clean['submitted'] === true ) :

check_admin_referer('eco-submitform');

/*
 *
 *
 * ------------------------------------------------ FILTER DATA ---------------------------------------------------------
 *
 *
 *
 */

// Comments, pings, or both?
if ( $_POST['comments'] == 'comments' &&  $_POST['pings'] == 'pings') {
	$eco_clean['which'] = 'both';
	$eco_clean['message_which'] = 'Comments and pings';
} elseif ( $_POST['comments'] == 'comments' && empty($_POST['pings']) ) {
	$eco_clean['which'] = 'comments';
	$eco_clean['message_which'] = 'Comments';
} elseif ( $_POST['pings'] == 'pings' && empty($_POST['comments']) ) {
	$eco_clean['which'] = 'pings';
	$eco_clean['message_which'] = 'Pings';
}

// The one click options are for both comments and pings
if ($_POST['for'] == 'oneclick-all' || $_POST['for'] == 'oneclick-last') {
	$eco_clean['status'] = 'open';
	$eco_clean['message_which'] = 'Comments and pings';
}

// Include future posts?
if ( $_POST['future'] == 'future' ) {
	$eco_clean['future'] = true;
}

$eco_clean['for'] = null;

// Which posts?
switch ($_POST['for']) {
	case 'oneclick-all':
	case 'oneclick-last':
	case 'for-new':
	case 'for-all':
	case 'for-bora':
	case 'for-last':
	case 'for-comments' :
	case 'cancel-auto' :
		$eco_clean['for'] = $_POST['for'];
}

// Status = open or closed?
switch ($_POST['status']) {
	case 'open':
	case 'Open':
		$eco_clean['status'] = 'open';
		$eco_clean['opposite_status'] = 'closed';
		break;
	case 'closed' :
	case 'Closed' :
		$eco_clean['status'] = 'closed';
		$eco_clean['opposite_status'] = 'open';
		break;
}

if ('oneclick-last' == $eco_clean['for']) {
	$eco_clean['status'] = 'closed';
	$eco_clean['opposite_status'] = 'open';
}

// BEFOREAFTER SETTINGS
if ( $eco_clean['for'] == 'for-bora') {
	
	if ( $_POST['beforeafter'] == 'before' || $_POST['beforeafter'] == 'after' ) {
		$eco_clean['beforeafter'] = $_POST['beforeafter'];
	}
	
	// Could I use this alone to check for valid date information?
	if ( checkdate($_POST['day'], $_POST['month'], $_POST['year']) ) {
		$eco_clean['date'] = date('Y-m-d',  strtotime("{$_POST['year']}-{$_POST['month']}-{$_POST['day']}"));
	}
	
}

// LAST 'X' SETTINGS
elseif ( $eco_clean['for'] == 'for-last' || $eco_clean['for'] == 'oneclick-last' ) {
	
	// Has the auto setting been applied?
	if ( $_POST['oneclick-auto'] == 'auto' || $_POST['for-last-auto'] == 'auto' ) {
		$eco_clean['auto-last'] = true;
	}

	// Numeric, i.e. last X posts/months/etc.
	if ( is_numeric($_POST['last']) ) {
		$eco_clean['last'] = $_POST['last'];
	} else {
		$eco_clean['last'] = 'error';
	}
	
	switch($_POST['units']) {
		case 'posts':
		case 'days':
		case 'weeks':
		case 'months':
		case 'years':
			$eco_clean['units'] = $_POST['units'];
			break;
	}
	
	
} // end if last 'X' settings

elseif ( $eco_clean['for'] == 'for-comments' ) {
	
	if ($_POST['for-comments-auto'] == 'auto') {
		$eco_clean['auto-comments'] = true;
	}
	
	if (ctype_digit($_POST['number-comments'])) {
		$eco_clean['number-comments'] = $_POST['number-comments'];
	} else {
		$eco_clean['number-comments'] = 'error';
	}
	
}

// IF POSTS FOR EXCLUSION HAVE BEEN ENTERED ...

if ( preg_match('/^[0-9, ]*$/', $_POST['posts']) ) {
	$treatments = preg_replace('/ {2,}/', ' ', $_POST['posts']); // remove excessive spaces
	$treatments = trim($treatments, ' ,'); // get rid of commas and whitespace at the beginning and end
	
	if (strpos($treatments, ' ') && !strpos($treatments, ',')) {
		$treatments = str_replace(' ', ', ', $treatments);
	}
	
	$eco_clean['posts'] = $treatments;
} elseif ( $_POST['posts'] == '' ) {
	$eco_clean['posts'] = '';
} else {
	$eco_clean['posts'] = 'error';
}

foreach ($eco_clean as $key => $value) {
	$eco_html[$key] = wp_specialchars($value);
}

$eco_html['status_pp'] = ($eco_clean['status'] == 'open') ? 'opened' : 'closed';
$eco_html['opp_pp'] = ($eco_clean['opposite_status'] == 'open') ? 'opened' : 'closed';

/*
 *
 *
 * ------------------------------------------------ ERRORS AND DEBUGGING ---------------------------------------------------------
 *
 *
 *
 */
 
if (DEBUGGING === TRUE) {
	function eco_debugging() { ?>
		<h3>Debugging Information</h3>
		<pre style="border: 1px solid black; background: #fafafa; padding: 5px;"><?php
			global $eco_clean, $eco_message, $eco_html, $options;
			$eco_POST_html = array();
			foreach ($_POST as $key => $value) {
				$eco_POST_html[$key] = wp_specialchars($value);
			}
			echo '<strong>$_POST</strong>:'."\n"; print_r($eco_POST_html); echo "\n";
			
			foreach ($eco_clean as $key => $value) {
				$eco_clean_html[$key] = wp_specialchars($value);
			}
			echo '<strong>$eco_clean</strong>:'."\n"; print_r($eco_clean_html); echo "\n";
			
			echo '<strong>$eco_html</strong>' . "\n";
			print_r($eco_html);
			echo "\n";
			
			echo "These are the arguments supplied to eco_query():\n";
			echo '<strong>ECO_QUERYVARS</strong>:  ' . ECO_QUERYVARS . "\n";
			if ( defined('ECO_QUERYVARS2') ) {
				echo '<strong>ECO_QUERYVARS2</strong>: ' . ECO_QUERYVARS2 . "\n";
			}
			echo "\nSQL query(-ies) sent to the database\n";
			echo '<strong>ECO_SQL</strong>:  ' . ECO_SQL . "\n";
			if ( defined('ECO_SQL2') ) {
				echo '<strong>ECO_SQL2</strong>: ' . ECO_SQL2 . "\n";
			}
			echo "\n" . '<strong>$eco_message</strong>:' . "\n" . wp_specialchars($eco_message) . "\n\n";
			
			if (!empty($options)) {
				foreach($options as $key => $value) {
					$options[$key] = wp_specialchars($value);
				}
				echo "\n".'<strong>$options</strong>:'."\n"; print_r($options);
			}
			?></pre><?php
	}
}

// If 'LAST X' and the 'last' value is wrong
if (($eco_clean['for'] == 'oneclick-last' || $eco_clean['for'] == 'for-last')
		&&
	($eco_clean['last'] == 'error')) {
	eco_error('Please provide a number in the &ldquo;last&rdquo; box.');
}

// If it's 'BEFORE/AFTER' and the date is wrong
if ( ($eco_clean['for'] == 'for-bora') && ($eco_clean['date'] == 'error') ) {
	eco_error('The date you provided wasn&#8217;t a valid calendar date, please check it for errors.');
}

// If 'EXCLUDED POSTS' have been entered and aren't nice
if ( $eco_clean['posts'] == 'error' ) {
	eco_error('Please make sure you enter the IDs of posts as numbers separated by a comma and space. For example: <kbd>12, 34, 129</kbd>');
}

// If it's X comments then close, and it's not a number.
if ( ($eco_clean['for'] == 'for-comments') && ($eco_clean['number-comments'] == 'error') ) {
	eco_error('Please enter the number of comments as a number.');
}



/*
 *
 *
 * ----------------------------------------- UPDATE OPTIONS and SEND QUERY ---------------------------------------------------------
 *
 *
 *
 */

// -------------------------------- WORKHORSE: Where the changes are actually made --------------------------------

// If excluded posts have changed since they were last entered AND if the one-click options haven't been used
if ( $eco_clean['posts'] != get_option('eco_excluded_posts') && !in_array($eco_clean['for'], array('oneclick-all', 'oneclick-last')) ) {
	update_option('eco_excluded_posts', $eco_clean['posts']);
}

$eco_auto_cancel = false;

switch ($eco_clean['for']) {

// IF CANCEL ARRANGEMENT SELECTED
case 'cancel-auto' :
	eco_cancel_auto();
	$eco_message = 'Your automatic arrangment has been cancelled. No other settings have been changed.';
	break;

// IF ALL POSTS ONE CLICK SELECTED
case 'oneclick-all' :
	update_option('default_comment_status', $eco_clean['status']);
	update_option('default_ping_status', $eco_clean['status']);
	if (true == $auto_check) {
		$eco_auto_cancel = true;
		eco_cancel_auto();
	}
	eco_query('both', $eco_clean['status']);
	$eco_message =
		'<strong>Comments and pings</strong> for '
		. '<strong>all existing posts</strong> have been set to '
		. "<strong>{$eco_html['status']}</strong>. The default status for comments and pings on new posts is also "
		. "<strong>{$eco_html['status']}</strong>.";
	break;

// IF 'NO EXISTING POSTS' SELECTED
case 'for-new' :
	default_comments_status();
	$eco_message =
		'The default setting for <strong>' . strtolower($eco_html['message_which']) . '</strong> on new posts have been set to '
		. " <strong>{$eco_html['status']}</strong>. No existing posts have been changed.";
	break;


// IF 'ALL EXISTING POSTS' SELECTED
case 'for-all' :
	default_comments_status();
	eco_query($eco_clean['which'], $eco_clean['status'], null, null, $eco_clean['posts']);
	$eco_message =
		"<strong>{$eco_html['message_which']}</strong> for <strong>all existing posts</strong> have been set to "
		. " <strong>{$eco_html['status']}</strong>.";
	break;


// IF 'BEFORE / AFTER' SELECTED
case 'for-bora' :
	default_comments_status();
	eco_query($eco_clean['which'], $eco_clean['status'], $eco_clean['beforeafter'], $eco_clean['date'], $eco_clean['posts']);
	$eco_message =
		"<strong>{$eco_html['message_which']}</strong> for posts made "
		. "<strong>{$eco_html['beforeafter']} " . date('jS F, Y', strtotime($eco_clean['date'])) . "</strong>"
		. " are now <strong>{$eco_html['status']}</strong>.";
	break;

// IF LAST 'X' ONE CLICK or 'LAST X' SELECTED
case 'oneclick-last' :
case 'for-last' :
	
	if ($eco_clean['for'] == 'oneclick-last') {
		update_option('default_comment_status', 'open');
		update_option('default_ping_status', 'open');
		$eco_clean['posts'] = get_option('eco_excluded_posts');
		$eco_clean['which'] = 'both';
	} else {
		default_comments_status();
	}
	
	// Last X posts
	if ( $eco_clean['units'] == 'posts' ) {
		$count = $wpdb->get_var("SELECT COUNT(ID) FROM $wpdb->posts WHERE `post_status` = 'publish' AND `post_type` = 'post'");
		if ( $eco_clean['last'] > $count ) {
			if ( $count == '1' ) {
				eco_error("You can't change the last {$eco_html['last']} posts because there&rsquo; only 1 post!");
			} else {
				eco_error("You can't change the last {$eco_html['last']} posts because there are only $count posts!");
			}
		} else {
			// do the opposite to all posts, then do intended to the right ones
			eco_query($eco_clean['which'], $eco_clean['status'], null, null, $eco_clean['posts']);
			eco_query($eco_clean['which'], $eco_clean['opposite_status'], 'last', $eco_clean['last'], $eco_clean['posts']);
		}
	// Last X days/weeks/months/years
	} else {
		$eco_clean['date'] = date_from_last($eco_clean['last'], $eco_clean['units']);
		eco_query($eco_clean['which'], $eco_clean['status'], 'after', $eco_clean['date'], $eco_clean['posts']);
		eco_query($eco_clean['which'], $eco_clean['opposite_status'], 'before', $eco_clean['date'], $eco_clean['posts']);
	}
	
	if ('posts' == $eco_clean['units']) {
		$eco_message = '<strong>Comments and pings</strong> for the '
			. "<strong>last {$eco_html['last']} {$eco_html['units']}</strong> "
			. "are now <strong>{$eco_html['opposite_status']}</strong>. The rest have been <strong>{$eco_html['status_pp']}</strong>.";
	} else {
		if ('1' == $eco_clean['last']) {
			$eco_html['units'] = rtrim($eco_html['units'], 's'); // cut off the last S if there's only 1!
		}
		$eco_message = "<strong>Comments and pings</strong> for posts made "
			. "<strong>before " . date('jS F, Y', strtotime($eco_clean['date'])) . " ({$eco_html['last']} {$eco_html['units']} ago)</strong>"
			. " are now <strong>{$eco_html['status']}</strong>.";
	}
	
	/*
	Updating automatic settings
	If there is already a arrangement in place, update options
	If there is no arrangement, update options and schedule the arrangement if the units aren't posts
	*/
	
	// If the option for automatic hasn't been selected, or if there isn't already one in place, these won't be executed.
	if (true == $eco_clean['auto-last'] || true == $auto_check) {
		update_option('eco_auto_which', $eco_clean['which']);
		update_option('eco_auto_status', $eco_clean['status']);
		update_option('eco_auto_last', $eco_clean['last']);
		update_option('eco_auto_units', $eco_clean['units']);
		// Only schedule the auto thing if it's a unit of time
		if ('posts' == $eco_clean['units']) {
			$eco_message =
				"<strong>{$eco_html['message_which']}</strong> will be <strong>{$eco_html['opposite_status']}</strong> on the "
				. " <strong>{$eco_html['last']}</strong> most recent posts."
				. " {$eco_html['message_which']} on older posts will be <strong>{$eco_html['status_pp']}</strong> each time a new post is published.";
		} else {
			// If there isn't already a schedule in place, put one in place
			// Can't use $auto_check because it's true at this point
			if (false == ctype_digit(wp_next_scheduled('eco_auto'))) {
				wp_clear_scheduled_hook('eco_auto');
				wp_schedule_event(time() + 60, 'daily', 'eco_auto'); // first time to run is 1 minute from now
			}
			$eco_message = 
				"<strong>{$eco_html['message_which']}</strong> on posts older than <strong>{$eco_html['last']} {$eco_html['units']}</strong> will be ".
				" <strong>{$eco_html['status_pp']}</strong> automatically.";
		}
	}
	
	break;
	
	// IF 'MORE THAN X COMMENTS' SELECTED
	case 'for-comments' :
		default_comments_status();
		eco_query($eco_clean['which'], $eco_clean['status'], 'comments', $eco_clean['number-comments'], $eco_clean['posts']);
		$eco_message =
			"<strong>{$eco_html['message_which']}</strong> for posts with more than "
			. "<strong>{$eco_html['number-comments']} comments and/or pings</strong> are now <strong>{$eco_html['status']}</strong>.";
		if($eco_clean['auto-comments'] == true) {
			update_option('eco_auto_which', $eco_clean['which']);
			update_option('eco_auto_status', $eco_clean['status']);
			update_option('eco_auto_last', $eco_clean['number-comments']);
			update_option('eco_auto_units', 'comments');
			$eco_message =
				"<strong>{$eco_html['message_which']}</strong> will be <strong>{$eco_html['status_pp']}</strong> "
				. " on posts once <strong>{$eco_html['number-comments']}</strong> comments and/or pings have been left on it.";
		}
		break;

} //end switch

// -------------------------------- SHOW THE RESULTS OF THE QUERY TO THE USER --------------------------------
?>

<div class="wrap">

<h2>Results</h2>

<ul>

<?php

echo "\t<li>$eco_message</li>\n";

if ( $eco_auto_cancel == true) {
	echo "\t<li>Your automatic arrangement has been cancelled.</li>\n";
}

if ( $eco_clean['future'] === true && $eco_clean['for'] != 'for-new') {
	echo "\t<li>The default setting for <strong>" . strtolower($eco_clean['message_which']);
	if ('for-last' == $eco_clean['for'] || 'oneclick-last' == $eco_clean['for'] || 'for-comments' == $eco_clean['for']) {
		echo "</strong> has also been set to <strong>{$eco_html['opposite_status']}</strong>.</li>\n";
	} else {
		echo "</strong> has also been set to <strong>{$eco_html['status']}</strong>.</li>\n";
	}
}

if ( !empty($eco_clean['posts']) && $eco_clean['for'] != 'for-new' ) {
	echo "\t<li>The following posts were not changed: <ul style='margin-top:5px;'>\n";
	$eco_unchanged = explode(',', $eco_html['posts']);
	foreach ($eco_unchanged as $unch_id) {
		$unch_id = trim($unch_id);
		echo '<li><a href="'.get_permalink($unch_id).'">'.get_the_title($unch_id).'</a>'." [<a href='post.php?action=edit&amp;post=$unch_id'>Edit</a>]</li>\n";
	}
	echo '</ul>';
}
	
?>

</ul>

<?php

if (DEBUGGING === TRUE) {
	eco_debugging($eco_html);
}

endif; // end if form submitted ?>

</div>

<?php } // close function comment_conf(), line 33

add_action('admin_menu', 'eco_extended_comments_options');

register_deactivation_hook( __FILE__, 'eco_cancel_auto' );

?>