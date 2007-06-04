<?php
/*	
	Simple Digest Plugin
	For Spam Karma 2
	Version 1.02 // minor revs. by dr Dave
	Copyright 2005 Peter Westood 

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

class sk2_pjw_simpledigest extends sk2_plugin
{
	var $name = "Simple Digest";
	var $plugin_version = 1.0;
	var $release_string ="";
	var $show_version =true;
	var $author = "Peter Westwood";
	var $author_email = "peter.westwood@ftwr.co.uk";
	var $plugin_help_url = "http://www.ftwr.co.uk/blog/wordpress/sk2-simple-digest-plugin/";
	var $description = "Emails a spam summary. (Sent first comment past interval)";
	var $treatment = true;
	var $weight_levels = array("0" => "Disabled", "1.0" => "Enabled");
	var $settings_format = array (
								"interval" => array("type" => "text", 
				   								"value"=> 24, 
				   								"caption" => "Send digest every ",
													"after" => "hours.",
													"size" => 3
													),
								"last_run" => array("type"=>"text",
													"value"=> 0,
													"caption" => "Last run unix timestamp :",
													"size" => 24,
													"advanced" => true
													),
													
								"digest_threshold" => array("type" => "text", 
				   								"value"=> -20, 
				   								"caption" => "Skip spam with karma under ",
													"size" => 3
													),
								);

	function treat_this(&$cmt_object)
	{
		global $wpdb;

		$interval = $this->get_option_value('interval');
		$last_run = $this->get_option_value('last_run');
		$threshold = $this->get_option_value('digest_threshold');
		
		if ($last_run + ($interval * 3600) < time())
		{
			$this->log_msg(sprintf(__("Generating mail digest (Last digest was %s).", 'sk2'), sk2_time_since($last_run)));
			$new_spams = $wpdb->get_var(
							"SELECT COUNT(*) FROM `$wpdb->comments` WHERE "
							."(`comment_approved`= '0' OR `comment_approved` = 'spam') AND "
							."`comment_date_gmt` > " 
							. gmstrftime("'%Y-%m-%d %H:%M:%S'", (int) $last_run));

			$cur_moderated = $wpdb->get_var("SELECT COUNT(*) FROM `$wpdb->comments` WHERE `comment_approved`= '0'");	
			
			$mail_subj = "[". get_settings('blogname') ."] Spam Karma 2: " . __("Simple Digest Report", 'sk2');
			$mail_content = sprintf(__ngettext("There is currently one comment in moderation", "There are currently %d comments in moderation", $cur_moderated, 'sk2'), $cur_moderated)."\r\n";
			$mail_content .= get_settings('siteurl') . "/wp-admin/edit.php" 
							 ."?page=spamkarma2&sk2_section=spam\r\n\r\n";
			//### l10n Add
			$mail_content .= sprintf(__ngettext("There has been one comment spam caught since the last digest report %s ago.", "There have been %s comment spams caught since the last digest report %s ago.", $new_spams, 'sk2'), $new_spams, sk2_time_since($last_run)) ."\r\n";
			$mail_content .= "\r\n";
			$mail_content .= sprintf(__("Spam summary report (skipping karma under %d): ", 'sk2'), $threshold) . "\r\n\r\n";

			/* Only do the query if there are new spams
			  Props to MCIncubus.
			*/
			if ((int) $new_spams > 0 )
			{
				//Query stolen from SK2 core.
				$spam_rows = $wpdb->get_results(
							"SELECT `posts_table`.`post_title`, `spam_table`.`karma`, "
							."`spam_table`.`id` as `spam_id`,`spam_table`.`karma_cmts`, "
							."`comments_table`.* FROM `"
							.$wpdb->comments ."` AS `comments_table` LEFT JOIN `" 
							.$wpdb->posts ."` AS `posts_table` ON "
							."`posts_table`.`ID` = `comments_table`.`comment_post_ID` LEFT JOIN `"
							. sk2_kSpamTable . "` AS `spam_table` ON "
							."`spam_table`.`comment_ID` = `comments_table`.`comment_ID` WHERE "
							."(`comment_approved`= '0' OR (`comment_approved` = 'spam' AND `spam_table`.`karma` >= $threshold)) AND "
							."`comment_date_gmt` > " 
							.gmstrftime("'%Y-%m-%d %H:%M:%S'", (int) $last_run)
							." ORDER BY `comment_date_gmt`");

				if (is_array($spam_rows))
				{
					$comment_count = 0;
					foreach ($spam_rows as $row)
					{
						$comment_count += 1;
						sk2_clean_up_sql($row);
					
						if (!$row->spam_id)
						{
							$row->karma = "[?]";
						}
				
						$mail_content .= "=======================================================\r\n";
						$mail_content .= "Report on comment number " . $comment_count 
										 ." (id=".$row->comment_ID.")" ."\r\n";
						$mail_content .= __("Comment Author: ", 'sk2') . $row->comment_author ."\r\n";
						$mail_content .= __("Comment Type: ", 'sk2');
						if(empty($row->comment_type))
							$mail_content .= __("Comment", 'sk2');
						else 
							$mail_content .= $row->comment_type;
						
						$mail_content .= "\r\n";
						$mail_content .= __("Comment Content: ", 'sk2') . "\r\n"
										 ."-----------------------------------------\r\n"
										 .strip_tags($row->comment_content) ."\r\n";

						if (!empty($row->karma_cmts))
						{
							$karma_cmts = $this->generate_karma_report(unserialize($row->karma_cmts));
							$karma_cmts .= "========\r\n";
							$karma_cmts .= sprintf("%8.2f",$row->karma) . __(" - Overall Karma", 'sk2') . "\r\n";
							$karma_cmts .= "========\r\n";
						}
						else
						{
							//Check to see if it's the current comment
							if($row->comment_ID == $cmt_object->ID)
							{
								$karma_cmts .= $this->generate_karma_report($cmt_object->karma_cmts);
								$karma_cmts .= "========\r\n";
								$karma_cmts .= sprintf("%8.2f",$cmt_object->karma) . __(" - Overall Karma", 'sk2') . "\r\n";
								$karma_cmts .= "========\r\n";
							}
						}
						$mail_content .= "\r\n" . __("Spam Karma 2 Report: ", 'sk2') . "\r\n" . $karma_cmts ."\r\n";
						$mail_content .= $this->generate_links($row) . "\r\n\r\n";
					}
				}
				else
				{
					$mail_content .= "\r\nAll spams caught recently are under the threshold (go to SK2 Admin screen to see them).\r\n";
				}
			
				$headers = "From: " . get_settings('admin_email') . "\r\n"
      			 ."Reply-To: " . get_settings('admin_email') . "\r\n"
						 ."X-Mailer: PHP/" . phpversion() . "\r\n"
						 ."Content-Type: text/plain; "
					   ."charset=\"".get_settings('blog_charset')."\"\r\n";

				wp_mail(get_settings("admin_email"), $mail_subj, $mail_content, $headers);
				$this->set_option_value('last_run', time());
			}
			else
			{
				$this->log_msg(__("Comment would have caused digest but no new spam recieved since last digest."), 4);
			}
		}
		else
		{
			// Check to see if last_run is in the future - being really paranoid!
			if ($last_run > time())
			{
				$this->log_msg(__("Last run time in the future - resetting to now"), 5);
				$this->set_option_value('last_run', time());
			}
		}
	}

	//Overrides
	function output_plugin_UI()
	{
		echo "<dl>";
		parent::output_plugin_UI(false);
		echo "<dd>";
		echo sprintf(__("Last run was %s ago", 'sk2'), sk2_time_since($this->get_option_value('last_run')));
		echo "</dd>";
		echo "</dl>";
	}

	function version_update($cur_version)
	{
		$this->set_option_value('last_run', time());
		$this->log_msg(__("Stored initial last run time stamp"), 4);
		return true;
	}

	//Private Functions
	function generate_karma_report($karma_cmts_array)
	{
		$karma_report = '';
		
		if (is_array($karma_cmts_array))
		{
			foreach ($karma_cmts_array as $cmt)
			{
				$karma_report .= sprintf("%+8.2f", $cmt['hit']) ." - ";
				$karma_report .= $cmt['plugin'] . ": ";
				$karma_report .= sk2_soft_hyphen(strip_tags($cmt['reason']));
				$karma_report .= "\r\n";	
			}
		}
		
		return $karma_report;
	}

	function generate_links($cmt_row)
	{
		if ($cmt_row->comment_approved == 'spam')
		{
			//Spam
			$links .= __("Rescue comment from spam: ", 'sk2');
			$links .= get_settings('siteurl') . "/wp-admin/edit.php";
			$links .= "?page=spamkarma2&sk2_section=spam";
			$links .= "&recover_selection=Recover%20Selected";
			$links .= "&comment_grp_check%5B" . $cmt_row->comment_ID ."%5D";
			$links .= "=" . $cmt_row->spam_id;
		} else {
			//Moderation
			$links .= __("Spank comment in moderation: ", 'sk2');
			$links .= get_settings('siteurl') . "/wp-admin/edit.php";
			$links .= "?page=spamkarma2&sk2_section=approved";
			$links .= "&recover_selection=Spank%20Selected";
			$links .= "&comment_grp_check%5B" . $cmt_row->comment_ID ."%5D";
			$links .= "=" . $cmt_row->spam_id;
		}
		

		return $links;
	}
}

/* Move the register to end so that the class is declared first - some versions of PHP require this.*/
$this->register_plugin("sk2_pjw_simpledigest", 25); //Run Last after Anubis etc.
?>
