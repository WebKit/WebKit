<?php
/*
Plugin Name: CJD Spam Nuke
Plugin URI: http://chrisjdavis.org/category/wp-hacks/
Description: This plugin allows you to interact with the comments flagged as 'spam' in WordPress 1.5.
Version: 1.5.3
Author: Chris J. Davis, Scott (skippy) Merill
Author URI: http://chrisjdavis.org/
*/

/*
Version 1.5.1 Changes:
	* Changed Name to CJD Spam Nuke.
	* Added number of spam to menu item ala the Awaiting Moderation menu item.
	* Move to a friendlier UI for Selective Nuking.
	* Added ability to "unspam" comments marked as spam, just in case.
	
Version 1.5.2 Changes:
	* Bug Fixes by Skippy, http://skippy.net to address servers without register_globals on.
	* After some thought, I removed the selective nuke.  Now you use the mass delete to nuke your spam, and the selective unspam to save a comment.

Version 1.5.3 Changes:
	* minor tweaks
*/

if (! function_exists('cjd_get_spam_count'))
{
	function cjd_get_spam_count()
	{
		global $wpdb, $comments;
		if (isset($_POST['action']) && ('nuked' == $_POST['action']))
		{
			// this is a lame cheat
			$comments = 0;
		} else {
			$comments = $wpdb->get_var("SELECT COUNT(comment_ID) FROM $wpdb->comments WHERE comment_approved = 'spam'");
		}
		return $comments;
	}
}

if (! function_exists('cjd_delete_add_manage_page'))
{
    function cjd_delete_add_manage_page()
    {
	global $wpdb;
        $spam_count = "Spam (" . cjd_get_spam_count() . ")";
        if (function_exists('add_management_page'))
            add_management_page("Spam", $spam_count, 1, __FILE__);
    }
}

if (is_plugin_page())
{
if ( (isset($_POST['submit'])) && ('deleted' == $_POST['action']) ) {
	$i = 0;
	foreach ($_POST['not_spam'] as $comment) : 
		$comment = (int) $comment;
		$wpdb->query("UPDATE $wpdb->comments SET comment_approved = '1' WHERE comment_ID = '$comment' AND comment_approved = 'spam'");
			++$i;
	endforeach;
	echo '<div class="updated"><p>' . sprintf(__('%s comments unspammed.'), $i) . "</p></div>";
}
if ('nuked' == $_POST['action']) {
	$nuked = $wpdb->query("DELETE FROM $wpdb->comments WHERE comment_approved = 'spam'");
	if (isset($nuked)){
		echo '<div class="updated"><p>';
		if ($nuked) {
			echo __("All spam Nuked! Rowr!");
		}
		echo "</p></div>";
	}
}
?>
<div class="wrap">
<h2><?php _e('Mass Spam Nuke') ?></h2>
<?php
$spam_count = cjd_get_spam_count();
if (0 == $spam_count)
{
	_e('<p align="center"><strong>Congratulations</strong> -- you are spam free!</p>');
	echo '</div>';
} else {
	_e('<p>Mass Spam Nuke allows you to delete every comment in your database that is flagged as spam with one click. Be warned this is undoable, if you are sure you don\'t have some legitimate comments flagged as spam then go for it.</p>');
?>
<form method="post" action="admin.php?page=cjd_delete.php&amp;action=nuked" name="form1">
<input type="hidden" name="action" value="nuked" /> 
There are currently <?php echo $spam_count; ?> comments identified as spam.&nbsp; &nbsp; <input type="submit" name="Submit" value="Nuke em, nuke em all!" />
</form>
</div>
<div class="wrap">
<h2><?php _e('Unspammer') ?></h2>
		<?php _e('<p>With the amazing technocolor unspammer, you can choose to rescue a comment from the bowels of spam hades.  Simply select the comments you wish the rescue and click Unspam Me! and you\'re off.</p>')?>
<?php
		$comments = $wpdb->get_results("SELECT *, COUNT(*) AS count FROM $wpdb->comments WHERE comment_approved = 'spam' GROUP BY comment_author_IP");
		if ($comments) {
?>
<form method="post" action="admin.php?page=cjd_delete.php" name="form2">
<input type="hidden" name="action" value="deleted" />
<input type="submit" name="submit" value="Unspam me!" />
<table width="100%" cellpadding="3" cellspacing="3"> 
  <tr>
	<th scope="col"><?php _e('Unspam?') ?></th>
    <th scope="col"><?php _e('Name') ?></th> 
    <th scope="col"><?php _e('Email') ?></th> 
    <th scope="col"><?php _e('URI') ?></th>
	<th scope="col"><?php _e('IP') ?></th>
	<th scope="col"><?php _e('Comments') ?></th> 
  </tr>
<?php
    foreach($comments as $comment) {
			$comment_date = mysql2date(get_settings("date_format") . " @ " . get_settings("time_format"), $comment->comment_date);
			$post_title = $wpdb->get_var("SELECT post_title FROM $wpdb->posts WHERE ID='$comment->comment_post_ID'");
?>
<?php
$bgcolor = '';
$class = ('alternate' == $class) ? '' : 'alternate';
?> 
<tr class='<?php echo $class; ?>'> 
   	<td align="center"><input type="checkbox" name="not_spam[]" value="<?php comment_ID(); ?>" /></td>	
	<td><?php comment_author() ?></td>
    <td align="center">
    <?php comment_author_email_link() ?> 
    </td> 
    <td align="center"><?php comment_author_url_link() ?></td>
    <td align="center"><a href="http://ws.arin.net/cgi-bin/whois.pl?queryinput=<?php comment_author_IP() ?>"><?php comment_author_IP() ?></a></td> 
    <td align="center"><?php echo $comment->count ?></td> 
  </tr>
	<?php
		}
	}
	?>
	</table>
	<input type="submit" name="submit" value="Unspam me!" />
	</form> 
	</div>
<?php
}
}
add_action('admin_menu', 'cjd_delete_add_manage_page');
?>
