<?php

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
		$comment_ids = $wpdb->get_col( "SELECT comment_id FROM $wpdb->comments WHERE comment_approved = 'spam' AND '$delete_time' > comment_date_gmt" );
		if ( !empty( $comment_ids ) ) {
			do_action( 'delete_comment', $comment_ids );
			$wpdb->query( "DELETE FROM $wpdb->comments WHERE comment_id IN ( " . implode( ', ', $comment_ids ) . " )");
			wp_cache_delete( 'akismet_spam_count', 'widget' );
		}
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

function redirect_old_akismet_urls( ) {
	global $wp_db_version;
	$script_name = array_pop( split( '/', $_SERVER['PHP_SELF'] ) );

	$page = '';
	if ( !empty( $_GET['page'] ) )
		$page = $_GET['page'];

	// 2.7 redirect for people who might have bookmarked the old page
	if ( 8204 < $wp_db_version && ( 'edit-comments.php' == $script_name || 'edit.php' == $script_name ) && 'akismet-admin' == $page ) {
		$new_url = esc_url( 'edit-comments.php?comment_status=spam' );
		wp_redirect( $new_url, 301 );
		exit;
	}
}
add_action( 'admin_init', 'redirect_old_akismet_urls' );

// For WP <= 2.3.x
global $pagenow;

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

// This option causes tons of FPs, was removed in 2.1
function akismet_kill_proxy_check( $option ) { return 0; }
add_filter('option_open_proxy_check', 'akismet_kill_proxy_check');
