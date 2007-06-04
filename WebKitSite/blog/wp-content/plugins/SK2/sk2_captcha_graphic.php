<?php
header("Content-type: image/png");

global $sk2_log, $wpdb;
require_once('../../../wp-config.php');
include_once(dirname(__FILE__) . "/sk2_core_class.php");

$comment_ID = (int) @$_REQUEST['c_id'];
$author_email = @$_REQUEST['c_author'];

$sk2_log->live_output = false;
$this_cmt = new sk2_comment ($comment_ID);

if (@$this_cmt->ID && ($author_email == $this_cmt->author_email))
{
	foreach($this_cmt->unlock_keys as $key)
		if ($key['class'] == "sk2_captcha_plugin")
			$string = strtoupper($key['key']);
}
else
	$string = __("Invalid ID", 'sk2');

$im  = imagecreate(150, 50);
$bg = imagecolorallocate($im, 0, 0, 0);
$red = imagecolorallocate($im, 255, 0, 0);
$px  = (imagesx($im) - 7.5 * strlen($string)) / 2;
imagestring($im, 6, 10, 10, $string, $red);
imagepng($im);
imagedestroy($im);
?>