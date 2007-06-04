<?php
// Blacklist Filter
// Runs URLs and IPs through each blacklist

class sk2_payload_plugin extends sk2_plugin
{
	var $name = "Encrypted Payload";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_Payload_Plugin";
	var $description = "Embed an encrypted payload in comment form. Ensures that the form has been loaded before a comment is submitted (and more).";
	var $filter = true;
	
	function form_insert($post_ID)
	{
		$seed = $this->get_option_value('secret_seed');
		if (empty ($seed))
		{
			$seed = sk2_rand_str(10);
			$this->set_option_value('secret_seed', $seed);
			$this->log_msg(__("Resetting secret seed to: $seed."), 5);
		}
		$time = time();
		$ip = $_SERVER['REMOTE_ADDR'];
		//echo ("<!--#". $time . "#". $seed . "#". $ip ."#". $post_ID . "#-->"); // debug
		$payload = md5($time . $seed . $ip . $post_ID); 
		echo "<input type=\"hidden\" id=\"sk2_time\" name=\"sk2_time\" value=\"$time\" />";
		echo "<input type=\"hidden\" id=\"sk2_ip\" name=\"sk2_ip\" value=\"$ip\" />";
		echo "<input type=\"hidden\" id=\"sk2_payload\" name=\"sk2_payload\" value=\"$payload\" />";
	}

	function version_update($cur_version)
	{
		$seed = sk2_rand_str(10);
		$this->set_option_value('secret_seed', $seed);
		$this->log_msg(__("Resetting secret seed to: ", 'sk2') . $seed, 5);
		return true;
	}

	function filter_this(&$cmt_object)
	{					
		if ($cmt_object->is_post_proc())
		{
			$log = __("Cannot check encrypted payload in post_proc mode.");
			$this->log_msg($log, 4);
			return;
		}	

		if (! $cmt_object->is_comment())
			return;
		
		if (empty($_REQUEST['sk2_payload']))
		{
			$log = __("Encrypted Payload missing from form.");
			$karma_diff = -20;
			$this->log_msg($log, 1);
		}
		elseif($cmt_object->post_ID != $_REQUEST['comment_post_ID'])
		{
			$log = sprintf(__("Error: Submitted Post_ID variable (%d) not matching ours (%d).", 'sk2'), $_REQUEST['comment_post_ID'], $cmt_object->post_ID);
			$this->log_msg($log, 9);
			$karma_diff = -8;
		}
		else
		{
			$seed = $this->get_option_value('secret_seed');
		
			if ($_REQUEST['sk2_payload'] != md5($_REQUEST['sk2_time'] . $seed . $_REQUEST['sk2_ip'] . $cmt_object->post_ID))
			{
				$log = __("Fake Payload.");
				$karma_diff = -20;
				$this->log_msg($log, 2);
			}
			elseif ($_REQUEST['sk2_ip'] == $_SERVER['REMOTE_ADDR'])
			{
				$log = __("Encrypted payload valid: IP matching.");
				$karma_diff = 0;
			}
			else
			{
				$log = __("Encrypted payload valid: IP <strong>not</strong> matching.");
				$karma_diff = - 2.5;
			}
		}
		$this->modify_karma($cmt_object, $karma_diff, $log);
	}
}

$this->register_plugin("sk2_payload_plugin", 2);

?>