<?php 

/*
Plugin Name: Extended Comment Options
Plugin URI: http://wordpress.org/extend/plugins/extended-comment-options/
Description: This plugin allows you to switch comments and/or pings on or off for batches of existing posts.
Version: 2.5
Author: Mark Kenny
Author URI: http://beingmrkenny.co.uk/
*/

define ('DEBUGGING', false); // Change to TRUE if you want to display information about the query.
define ('ECOVERSION', '2.5');

// -------------------------------- Functions --------------------------------

$eco_db_version = get_option('eco_version');

if ($eco_db_version != ECOVERSION) {
	eco_plugin_update();
}

function eco_plugin_update() {
	global $wpdb;
	$old_options = array(
		'which' => get_option('eco_auto_which'),
		'status' => get_option('eco_auto_status'),
		'last' => get_option('eco_auto_last'),
		'units' => get_option('eco_auto_units')
	);
	update_option('eco_version', ECOVERSION);
	update_option('eco_discussion', $old_options['which']);
	update_option('eco_status', $old_options['status']);
	if ('comments' == $old_options['units']) {
		update_option('eco_for', 'for-comments');
		update_option('eco_number-comments', $old_options['last']);
	} else {
		update_option('eco_for', 'for-last');
		update_option('eco_last', $old_options['last']);
		update_option('eco_units', $old_options['units']);
	}
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_which'");
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_status'");
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_last'");
	$wpdb->query("DELETE FROM $wpdb->options WHERE option_name = 'eco_auto_units'");
}

$auto_check = ctype_digit(wp_next_scheduled('eco_auto'));

if (false == $auto_check) {
	$auto_check = ( 'auto' == get_option('eco_auto') ) ? true : false;
}

//if ($auto_check == true) {
	$options = array(
		'auto' => get_option('eco_auto'),
		'which' => get_option('eco_discussion'),
		'status' => get_option('eco_status'),
		'for' => get_option('eco_for'),
		'last' => get_option('eco_last'),
		'units' => get_option('eco_units'),
		'day' => get_option('eco_day'),
		'month' => get_option('eco_month'),
		'year' => get_option('eco_year'),
		'beforeafter' => get_option('eco_beforeafter'),
		'db_excluded_posts' => get_option('eco_excluded_posts'),
		'number-comments' => get_option('eco_number-comments'),
		'next_time' => wp_next_scheduled('eco_auto')
	);
	$options['message_which'] = ($options['which'] == 'both') ? 'Comments and pings' : ucwords($options['which']);
	$options['opposite_status'] = ($options['status'] == 'open') ? 'closed' : 'open';
	$options['status_pp'] = ($options['status'] == 'open') ? 'opened' : 'closed';
	$options['opp_pp'] = ($options['opposite_status'] == 'open') ? 'opened' : 'closed';
//}

function eco_extended_comments_options() {
	if (function_exists('add_options_page') )
		add_submenu_page('edit-comments.php', 'Extended Comment Options', 'Batch Status', 8, basename(__FILE__), 'comment_conf');
		// The first argument does into the <title>, the second goes on the tab in options
}

function eco_input($name, $type, $value, $default=false) {
	global $wpdb;
	$value = htmlentities($value, ENT_QUOTES, 'UTF-8');
	$id = ('radio' == $type) ? "$name-$value" : $name;
	$checked = '';
	$db = get_option('eco_'.$wpdb->escape($name));
	if ($value == $db) {
		// If value matches that in DB
		$checked =  'checked=\'checked\'';
	} else {
		// If the value doesn't match that in DB
		if ('' == $db && true == $default) {
			// If DB is empty (i.e. no entry) and this is the default option
			$checked =  'checked=\'checked\'';
		}
	}
	
	echo "<input type='$type' value='$value' name='$name' id='$id' $checked />\n";
}

function eco_text_input($name, $default, $max, $size, $title='') {
	global $wpdb;
	$value = htmlentities($value, ENT_QUOTES, 'UTF-8');
	$title = htmlentities($title, ENT_QUOTES, 'UTF-8');
	$db = get_option('eco_'.$wpdb->escape($name));
	$value = ('' == $db) ? $default : $db;
	echo "<input type='text' value='$value' name='$name' id='$name' title='$title' maxlength='$max' size='$size' />\n";
}

function eco_option($value, $label, $name) {
	global $wpdb;
	$value = htmlentities($value, ENT_QUOTES, 'UTF-8');
	$label = htmlentities($label, ENT_QUOTES, 'UTF-8');
	if ('month' == $name) {
		$checked = ($value == date('m')) ? 'selected=\'selected\'' : '';
	}
	$db_value = get_option('eco_'.$wpdb->escape($name));
	if ($db_value) {
		$checked = ($value == $db_value) ? 'selected=\'selected\'' : '';
	}
	echo "<option value='$value' $checked>$label</option>\n";
}

function eco_error($err_message) {
	global $eco_error;
	echo '<div class="error">';
	echo '<p>An error has occurred:</p>';
	echo "<p><strong>$err_message</strong></p>";
	echo '<p>No comments or pings have been opened or closed.</p>';
	echo '</div><!-- end error -->';
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

function default_comments_status($which, $status) {

	switch($which) {
		case 'both':
			update_option('default_comment_status', $status);
			update_option('default_ping_status', $status);
			break;
		case 'comments':
			update_option('default_comment_status', $status);
			break;
		case 'pings':
			update_option('default_ping_status', $status);
			break;
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
	
	/*
	Horizon:
	before/after -- just works with a date
	last -- orders by post_date and then LIMITs by the number supplied
	comments -- counts by comment!
	
	If it's null, $sql is not appended, and it applies to all posts
	*/
	
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
	
	if ('for-last' == $options['for']) {
		if ( $options['units'] == 'posts' ) {
			// close all, then open the right ones
			// Make sure that ALL is done first, then LAST
			eco_query($options['which'], $options['status'], null, null, $options['db_excluded_posts']);
			eco_query($options['which'], $options['opposite_status'], 'last', $options['last'], $options['db_excluded_posts']);
		} else {
			$date = date_from_last($options['last'], $options['units']);
			eco_query($options['which'], $options['status'], 'after', $date, $options['db_excluded_posts']);
			eco_query($options['which'], $options['opposite_status'], 'before', $date, $options['db_excluded_posts']);
		}
	} elseif ('for-comments' == $options['for']){
		// close all, then open the right ones
		eco_query($options['which'], $options['opposite_status'], null, null, $options['db_excluded_posts']);
		eco_query($options['which'], $options['status'], 'comments', $options['number-comments'], $options['db_excluded_posts']);
	}
	
}

function eco_cancel_auto() {
	global $wpdb;
	wp_clear_scheduled_hook('eco_auto');
	update_option('eco_auto', 'no');
}

// Do comments thing every time a new post/comment is published, but only if this has been requested
if ('auto' == $options['auto']) {
	switch($options['for']) {
		case 'for-last' :
			if ('posts' == $options['units']) {
				add_action('publish_post', 'eco_auto');
			}
			break;
		case 'for-comments' :
			add_action('comment_post', 'eco_auto');
			add_action('pingback_post', 'eco_auto');
			break;
	}
}

// Initialise $eco_clean;
// Set $eco_clean['submitted'] to true if the form has been submitted;
$eco_clean = array('auto' => false);
$eco_clean['db_excluded_posts'] = get_option('eco_excluded_posts');
$eco_clean['submitted'] = false;
if ( $_POST['submit'] == 'Update' || $_POST['for'] == 'cancel-auto') {
	$eco_clean['submitted'] = true;
}

$eco_html = array();
$eco_html['db_excluded_posts'] = wp_specialchars($eco_clean['db_excluded_posts']);

// This inserts the page's content
function comment_conf() {
global $wpdb, $eco_clean, $eco_message, $options, $eco_html, $auto_check, $options;

?>

<div class="wrap">
	<div id="icon-edit-comments" class="icon32"><br /></div>
	<h2>Extended Comment Options</h2>
	<?php if ($eco_clean['submitted'] === true) : ?>

		<div id="message" class="updated fade">
			<p>Comment options received.</p>
		</div>

	<?php endif; ?>

	<?php if (true == DEBUGGING) : ?>
		<div class="wrap">
			<?php pre($options, '$options'); ?>
		</div>
	<?php endif; ?>



<?php if ( empty($eco_clean['submitted']) ) : // Before form submitted
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

	<h3><?php echo $options['message_which']; ?> are being managed automatically</h3>
	
	<?php
	
	switch ($options['for']) {
		case 'for-all':
			echo "<p>{$options['message_which']} are {$options['status']} on this blog.</p>";
			break;
		case 'for-bora':
			$date = date('l jS F, Y', strtotime($options['year'].'-'.$options['month'].'-'.$options['day']));
			echo "<p>{$options['message_which']} on posts made before $date are {$options['status']} on this blog.</p>";
			break;
		case 'for-last':
			if ('posts' == $options['units']) {
				echo "<p>{$options['message_which']} are {$options['status']} on all but the last {$options['last']} posts.</p>";
			} else {
				$display_units = (1 == $options['last']) ? rtrim($options['units'], 's') : $options['units'];
				echo "<p>{$options['message_which']} are {$options['status']} on posts older than {$options['last']} $display_units.</p>";
			}
			break;
		case 'for-comments':
			echo "<p>{$options['message_which']} are {$options['status']} on posts with more than {$options['number-comments']} comments.</p>";
			break;
	}
	
	if ( !empty($options['db_excluded_posts']) ) {
		echo "\t<p>The following posts won&rsquo;t be changed</p> <ul>\n";
		$eco_unchanged = explode(',', $options['db_excluded_posts']);
		foreach ($eco_unchanged as $unch_id) {
			$unch_id = trim($unch_id);
			echo '<li><a href="'.get_permalink($unch_id).'">'.get_the_title($unch_id).'</a>'." [<a href='post.php?action=edit&amp;post=$unch_id'>Edit</a>]</li>\n";
		}
		echo '</ul>';
	}
	
	?>
	
	<form action="" method="post">
		<?php
			if ( function_exists('wp_nonce_field') ) {
				wp_nonce_field('eco-submitform');
			}
		?>
		<input type="hidden" name="for" value="cancel-auto" />
		<p><input type="submit" name="cancel-arrangement" value="Switch to manual control &raquo;" class="button"></p>
	</form>
	
<?php else: ?>
	
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
	
	<form action="" method="post" name="advanced">

		<?php if ( function_exists('wp_nonce_field') ) { wp_nonce_field('eco-submitform'); } ?>

		<table class="form-table">

		<p>
			I want to 
			<select name="status">
				<?php eco_option('closed', 'close', 'status'); ?>
				<?php eco_option('open', 'open', 'status'); ?>
			</select>

			<select name="discussion">
				<?php eco_option('both', 'comments and pings', 'discussion'); ?>
				<?php eco_option('comments', 'comments', 'discussion'); ?>
				<?php eco_option('pings', 'pings', 'discussion'); ?>
			</select>
			on
		</p>

		<tr valign="top">
			<th width="20%" scope="row">Which Posts</th>
			<td>
				<!-- Option 2 -->
				<p>
					<?php eco_input('for', 'radio', 'for-all', true); ?>
					<label for="for-for-all">All posts</label>
				</p>
				<!-- Option 3 -->
				<p>
					<?php eco_input('for', 'radio', 'for-bora')?> <label for="for-for-bora">All posts made</label>

					<span onfocus="setCheckedValue(document.forms['advanced'].elements['for'], 'for-bora')">

						<select name="beforeafter">
							<?php eco_option('before', 'Before', 'beforeafter'); ?>
							<?php eco_option('after', 'After', 'beforeafter'); ?>
						</select> :

						<?php eco_text_input('day', date('d'), '2', '2', 'Enter the day (2 digits)'); ?>

						<select name="month">
							<?php
								eco_option('01', 'January', 'month');
								eco_option('02', 'February', 'month');
								eco_option('03', 'March', 'month');
								eco_option('04', 'April', 'month');
								eco_option('05', 'May', 'month');
								eco_option('06', 'June', 'month');
								eco_option('07', 'July', 'month');
								eco_option('08', 'August', 'month');
								eco_option('09', 'September', 'month');
								eco_option('10', 'October', 'month');
								eco_option('11', 'November', 'month');
								eco_option('12', 'December', 'month');
							?>
						</select>
						<?php eco_text_input('year', date('Y'), '4', '4', 'Enter the year (4 digits)'); ?>

					</span>
				</p>

				<!-- Option 4 -->
				<p>
					<?php eco_input('for', 'radio', 'for-last');  ?> <label for="for-for-last">All but the last</label>
					<span onfocus="setCheckedValue(document.forms['advanced'].elements['for'], 'for-last')">
					<?php eco_text_input('last', '5', '5', '5');  ?>
					<select name="units">
						<?php
						eco_option('posts', 'Posts', 'units');
						eco_option('days', 'Days', 'units');
						eco_option('weeks', 'Weeks', 'units');
						eco_option('months', 'Months', 'units');
						eco_option('years', 'Years', 'units');
						?>
					</select>
					</span>

				</p>

				<!-- Option 5 -->
				<p>
					<?php eco_input('for', 'radio', 'for-comments'); ?> <label for="for-for-comments">Posts with more than</label>
					<span onclick="setCheckedValue(document.forms['advanced'].elements['for'], 'for-comments')">
					<?php eco_text_input('number-comments', '10', '5', '5'); ?>
					</span>
					<label for="for-for-comments">comments and pings</label>
				</p>
			</td>
		</tr>

		<tr valign="top">
			<th scope="row">Auto</th>
			<td>
				<?php eco_input('auto', 'checkbox', 'auto', true); ?>
				<label for="auto">Apply this automatically from now on.</label>
			</td>
		</tr>

		<tr valign="top">
			<th scope="row">
				Exclude Posts
			</th>
			<td>
				<p>
					If you want to manage comments and pings on certain posts yourself, check the appropriate post in the box below.
					The posts you select won&rsquo;t be touched.
				</p>
				<div style="height: 15em; overflow: auto; border: 1px solid black; padding: 5px;">
					<?php
						$excluded_posts = explode(',', get_option('eco_excluded_posts'));
						$posts = $wpdb->get_results("SELECT ID, post_title, post_date FROM $wpdb->posts WHERE post_status = 'publish' AND post_type = 'post' ORDER BY post_date", ARRAY_A);
						foreach($posts as $post) {
							$checked = (in_array($post['ID'], $excluded_posts)) ? 'checked="checked"' : '' ;
							$post['post_date'] = date('d-M-Y', strtotime($post['post_date']));
							$post['post_title'] = wp_specialchars($post['post_title']);
							echo "<input type='checkbox' name='exclude-post-{$post['ID']}' value='{$post['ID']}' id='post-{$post['ID']}'$checked> <label for='post-{$post['ID']}'>{$post['post_date']}: {$post['post_title']}</label><br />";
						}
					?>
				</div>
			</td>
		</tr>

		</table>

		<p class="submit">
			<input name="submit" value="Update" type="submit" />
		</p>

	</form>

<?php endif; // end if auto_check


// OTHERWISE IF FORM HAS BEEN SUBMITTED :
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

switch($_POST['discussion']) {
	case 'both':
		$eco_clean['which'] = 'both';
		$eco_clean['message_which'] = 'Comments and pings';
		break;
	case 'comments':
		$eco_clean['which'] = 'comments';
		$eco_clean['message_which'] = 'Comments';
		break;
	case 'pings':
		$eco_clean['which'] = 'pings';
		$eco_clean['message_which'] = 'Pings';
		break;
}

$eco_clean['for'] = null;

// Which posts?
switch ($_POST['for']) {
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
		$eco_clean['status'] = 'open';
		$eco_clean['opposite_status'] = 'closed';
		break;
	case 'closed' :
		$eco_clean['status'] = 'closed';
		$eco_clean['opposite_status'] = 'open';
		break;
}

// BEFOREAFTER SETTINGS
if ( $eco_clean['for'] == 'for-bora') {
	
	if ( $_POST['beforeafter'] == 'before' || $_POST['beforeafter'] == 'after' ) {
		$eco_clean['beforeafter'] = $_POST['beforeafter'];
	}
	
	// Could I use this alone to check for valid date information?
	if ( checkdate($_POST['month'], $_POST['day'], $_POST['year']) ) {
		$eco_clean['date'] = date('Y-m-d',  strtotime("{$_POST['year']}-{$_POST['month']}-{$_POST['day']}"));
	} else {
		$eco_clean['date'] = 'error';
	}
	
}

// LAST 'X' SETTINGS
elseif ( $eco_clean['for'] == 'for-last' ) {

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
	
	if (ctype_digit($_POST['number-comments'])) {
		$eco_clean['number-comments'] = $_POST['number-comments'];
	} else {
		$eco_clean['number-comments'] = 'error';
	}
	
}

// Has the auto setting been applied?
if ( $_POST['auto'] == 'auto' ) {
	$eco_clean['auto'] = true;
}

// IF POSTS FOR EXCLUSION HAVE BEEN ENTERED ...

$excluded_posts = array();
foreach($_POST as $key => $value) {
	if (strpos($key, 'exclude-post-') !== false) {
		if (preg_match('/^[0-9]*$/', $value)) {
			$excluded_posts[] = $value;
		}
	}
}

if (empty($excluded_posts)) {
	$eco_clean['posts'] = '';
} else {
	$eco_clean['posts'] = implode(',', $excluded_posts);
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
if ($eco_clean['for'] == 'for-last' && $eco_clean['last'] == 'error') {
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

if ('cancel-auto' != $eco_clean['for']) {
	// If excluded posts have changed since they were last entered AND if the one-click options haven't been used
	if ( $eco_clean['posts'] != get_option('eco_excluded_posts') ) {
		update_option('eco_excluded_posts', $eco_clean['posts']);
	}
	update_option('eco_for', $eco_clean['for']);
}
update_option('eco_status', $eco_clean['status']);
update_option('eco_discussion', $eco_clean['which']);

$eco_auto_cancel = false;


switch ($eco_clean['for']) {

// IF CANCEL ARRANGEMENT SELECTED
case 'cancel-auto' :
	eco_cancel_auto();
	$eco_message = 'Your automatic arrangment has been cancelled. No comments or pings have been opened or closed.';
	break;


// IF 'ALL EXISTING POSTS' SELECTED
case 'for-all' :
	if (true == $eco_clean['auto']) {
		default_comments_status($eco_clean['which'], $eco_clean['status']);
	}
	eco_query($eco_clean['which'], $eco_clean['status'], null, null, $eco_clean['posts']);
	$eco_message =
		"<strong>{$eco_html['message_which']}</strong> for <strong>all existing posts</strong> have been set to "
		. " <strong>{$eco_html['status']}</strong>.";
	break;


// IF 'BEFORE / AFTER' SELECTED
case 'for-bora' :
	
	if ('before' == $eco_clean['beforeafter']) {
		// We apply the status to the posts made before the date
		eco_query($eco_clean['which'], $eco_clean['status'], 'before', $eco_clean['date'], $eco_clean['posts']);
		// Then we apply the opposite to the posts made after the date, and change the default to opposite.
		eco_query($eco_clean['which'], $eco_clean['opposite_status'], 'after', $eco_clean['date'], $eco_clean['posts']);
		if (true == $eco_clean['auto']) {
			default_comments_status($eco_clean['which'], $eco_clean['opposite_status']);
		}
		
	} elseif ('after' == $eco_clean['beforeafter']) {
		// We apply the opposite status to the posts made before the date
		eco_query($eco_clean['which'], $eco_clean['opposite_status'], 'before', $eco_clean['date'], $eco_clean['posts']);
		// Then we apply the status to the posts made after the date, and change the default to status
		eco_query($eco_clean['which'], $eco_clean['status'], 'after', $eco_clean['date'], $eco_clean['posts']);
		if (true == $eco_clean['auto']) {
			default_comments_status($eco_clean['which'], $eco_clean['status']);
		}
		
	}
	
	$day = date('d', strtotime($eco_clean['date']));
	$month = date('m', strtotime($eco_clean['date']));
	$year = date('Y', strtotime($eco_clean['date']));
	update_option('eco_day', $day);
	update_option('eco_month', $month);
	update_option('eco_year', $year);
	update_option('eco_beforeafter', $eco_clean['beforeafter']);
	
	$eco_message =
		"<strong>{$eco_html['message_which']}</strong> for posts made "
		. "<strong>{$eco_html['beforeafter']} " . date('jS F, Y', strtotime($eco_clean['date'])) . "</strong>"
		. " are now <strong>{$eco_html['status']}</strong>.";
	break;

// IF 'LAST X' SELECTED
case 'for-last' :

	if (true == $eco_clean['auto']) {
		default_comments_status($eco_clean['which'], $eco_clean['opposite_status']);
	}
	
	update_option('eco_status', $eco_clean['status']);
	
	// Last X posts
	if ( $eco_clean['units'] == 'posts' ) {
		$count = $wpdb->get_var("SELECT COUNT(ID) FROM $wpdb->posts WHERE `post_status` = 'publish' AND `post_type` = 'post'");
		if ( $eco_clean['last'] > $count ) {
			if ( $count == '1' ) {
				eco_error("You can't change the last {$eco_html['last']} posts because there is only 1 post!");
			} else {
				eco_error("You can't change the last {$eco_html['last']} posts because there are only $count posts!");
			}
		} else {
			// do the selected to all posts, then apply the opposite to the right ones
			// This is because of the wording of the admin panel (STATUS all but the last ... means do STATUS to all, then OPPOSITE to the last ...)
			eco_query($eco_clean['which'], $eco_clean['status'], null, null, $eco_clean['posts']);
			eco_query($eco_clean['which'], $eco_clean['opposite_status'], 'last', $eco_clean['last'], $eco_clean['posts']);
			update_option('eco_units', 'posts');
			update_option('eco_last', $eco_clean['last']);
		}
	// Last X days/weeks/months/years
	} else {
		$eco_clean['date'] = date_from_last($eco_clean['last'], $eco_clean['units']);
		eco_query($eco_clean['which'], $eco_clean['status'], 'before', $eco_clean['date'], $eco_clean['posts']);
		eco_query($eco_clean['which'], $eco_clean['opposite_status'], 'after', $eco_clean['date'], $eco_clean['posts']);
		update_option('eco_units', $eco_clean['units']);
		update_option('eco_last', $eco_clean['last']);
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
	if (true == $eco_clean['auto'] || true == $auto_check) {
		
	}
	
	break;
	
	// IF 'MORE THAN X COMMENTS' SELECTED
	case 'for-comments' :
		if (true == $eco_clean['auto']) {
			default_comments_status($eco_clean['which'], $eco_clean['opposite_status']);
		}
		update_option('eco_number-comments', $eco_clean['number-comments']);
		eco_query($eco_clean['which'], $eco_clean['status'], 'comments', $eco_clean['number-comments'], $eco_clean['posts']);
		$eco_message =
			"<strong>{$eco_html['message_which']}</strong> for posts with more than "
			. "<strong>{$eco_html['number-comments']} comments and/or pings</strong> are now <strong>{$eco_html['status']}</strong>.";
		
		break;

} //end switch

if (true == $eco_clean['auto']) {
	
	update_option('eco_auto', 'auto');
	
	if ('for-last' == $eco_clean['for']) {
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
				wp_schedule_event(strtotime('midnight'), 'daily', 'eco_auto'); // first time to run is midnight
			}
			$eco_message = 
				"<strong>{$eco_html['message_which']}</strong> on posts older than <strong>{$eco_html['last']} {$eco_html['units']}</strong> will be ".
				" <strong>{$eco_html['status_pp']}</strong> automatically.";
		}
		
	} elseif ('for-comments' == $eco_clean['for']) {
		$eco_message =
			"<strong>{$eco_html['message_which']}</strong> will be <strong>{$eco_html['status_pp']}</strong> "
			. " on a post once <strong>{$eco_html['number-comments']}</strong> comments and/or pings have been left on it.";
	}	

} else {
	eco_cancel_auto();
}

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
	if ('for-last' == $eco_clean['for'] || 'for-comments' == $eco_clean['for']) {
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

</div> <!-- end wrap (results) -->

<?php } // close function comment_conf(), line 33

add_action('admin_menu', 'eco_extended_comments_options');

register_deactivation_hook( __FILE__, 'eco_cancel_auto' );

?>
