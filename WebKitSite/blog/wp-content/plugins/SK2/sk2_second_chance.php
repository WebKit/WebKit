<html><head /><body>
<?php
require_once('../../../wp-config.php');
global $sk2_log;
include_once(dirname(__FILE__) . "/sk2_core_class.php");

$comment_ID = (int) @$_REQUEST['c_id'];
$author_email = @$_REQUEST['c_author'];

//DEBUG:
$sk2_log->live_output = false;

$sk2_log->log_msg(__("Second Chance. Comment ID:", 'sk2') . $comment_ID, 4, $comment_ID, "2nd_chance");
$sk2_core = new sk2_core(0, true, false);

if ($sk2_core->load_comment($comment_ID))
{
	//echo "<pre>"; 	print_r($sk2_core->cur_comment);
	if ($sk2_core->cur_comment->author_email != $author_email)
		die(__("Email not matching comment ID", 'sk2'));
		
	$sk2_core->load_plugin_files();	
	$sk2_core->second_chance();
}
else
{
	die(__("Invalid comment", 'sk2'));
}
?>
</body></html>