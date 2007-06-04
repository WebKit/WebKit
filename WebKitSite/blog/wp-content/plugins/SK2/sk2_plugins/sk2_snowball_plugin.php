<?php
// Snowball Filter
// use comment's granularity...


class sk2_snowball_plugin extends sk2_plugin
{
	var $name = "Snowball Effect";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_Snowball_Plugin";
	var $description = "Granularity check (regular commenters get rewarded, unknown commenters are kept on watch).";
	var $filter = true;
	var $skip_under = -20;
	var $skip_above = 5;
	var $retro_spanked = false;
	
	var $settings_format = array ("old_enough" => array("type" => "text", "value"=> 3, "caption" => "On an average you check new comments every", "size" => 2, after => "days."), 
											"threshold" => array("type" => "text", "value"=> 2, "caption" => "Trigger when somebody posts more than", "size" => 5, after => "comments over the above time-period."),
											"coef" => array("advanced" => true, "type" => "text", "value"=> 3, "caption" => "Old comments/new comments coefficient: ", "size" => 3),
											"good_karma" => array("advanced" => true, "type" => "text", "value"=> 5, "caption" => "Good/bad karma average: ", "size" => 3));
	
	var $ID;
	
	function filter_this(&$cmt_object)
	{
	
		$this->ID = $cmt_object->ID;

		$this->snowball_by($cmt_object, "IP", "AND `comments`.`comment_author_IP` = '". $cmt_object->author_ip ."'", 1, 1);

		if (! empty($cmt_object->author_url['domain']))
		{
			if (count($cmt_object->content_links))
				$this->snowball_by($cmt_object, "URL", "AND `comments`.`comment_author_url` LIKE '%". sk2_escape_string($cmt_object->author_url['domain']) ."%'", 1, 0.02);
			else
				$this->snowball_by($cmt_object, "URL", "AND `comments`.`comment_author_url` LIKE '%". sk2_escape_string($cmt_object->author_url['domain']) ."%'", 1.5, 1);			
		}

		if (! empty($cmt_object->author_email))
			$this->snowball_by($cmt_object, "email", "AND `comments`.`comment_author_email` = '". sk2_escape_string($cmt_object->author_email) ."'", 0.5, 2);
	}
	
	function snowball_by(&$cmt_object, $criterion, $query_where, $coef_hit, $coef_raise)
	{
		$coef = $this->get_option_value("coef");
		$good_karma = $this->get_option_value("good_karma");
		$karma_diff = 0;
				
		if (($old = $this->get_granularity($query_where, "<", $cmt_object->cmt_date))
			&& ($recent = $this->get_granularity($query_where, ">", $cmt_object->cmt_date)))
		{
			$threshold = $this->get_option_value("threshold");
			if ($recent->cmt_count > $threshold) // more than X recent comments
			{
				if (! $old->cmt_count) // no old comments
				{
					if ($recent->karma_avg < 0) // bad average for recent comments: kick in the nuts
					{
						$karma_diff = - ($coef_hit * pow($recent->cmt_count - $threshold, 2));
						
						global $wpdb;
						//$now_gmt = gmstrftime("'%Y-%m-%d %H:%M:%S'");						
						if (!$this->retro_spanked && ($retro_cmts = $wpdb->get_results("SELECT `comment_ID` FROM `$wpdb->comments` WHERE `comment_ID` != $this->ID AND `comment_author_IP` = '". $cmt_object->author_ip . "' AND `comment_date_gmt` > DATE_SUB('" . $cmt_object->cmt_date . "', INTERVAL ". $this->get_option_value("old_enough") . " DAY)")))
						{
							// Unleash all minions of Hell on that bad boy's company...

							$log = sprintf(__ngettext("Retro-spanked one comment. ID: ", "Retro-spanked %d comments. IDs: ", count($retro_cmts), 'sk2'), count($retro_cmts));
							$this->retro_spanked = true;
							$retro_spanking_core = new sk2_core(0, true, true);
							//$retro_spanking_core->load_plugin_files($);
							
							foreach($retro_cmts as $retro_cmt)
							{
								$retro_spanking_core->load_comment($retro_cmt->comment_ID);
								$retro_spanking_core->cur_comment->modify_karma($karma_diff, get_class($this), __("Retro-spanking triggered by comment ID: ", 'sk2') . $this->ID);
								$retro_spanking_core->treat_comment();
								$retro_spanking_core->set_comment_sk_info();
								$log .= $retro_cmt->comment_ID . ", ";
							}
							$log = substr($log, 0, -2) . ". " . __("Karma hit: ", 'sk2') . $karma_diff;
							$this->log_msg($log, 5);
						}
						if (mysql_error())
							$this->log_msg_mysql(__("Retro-spanking sql query failed.", 'sk2'), 7, $this->ID);

					}
					else // decent average: small penalty
						$karma_diff = - ($coef_hit * $recent->cmt_count + $threshold);
					//DdV TODO: check if this is a flood and retro-moderate other comments
				}
				elseif($coef * $old->cmt_count < $recent->cmt_count) // much less old than new
				{
					if ($old->karma_avg < $good_karma) // bad average in the past: spank hard
						$karma_diff = - ($coef_hit * pow($recent->cmt_count - ($old->cmt_count * 3), 2));
					else
						$karma_diff = $coef_hit * (- $recent->cmt_count - ($old->cmt_count * 3)); // otherwise, spank soft
				}
				elseif ($old->karma_avg > $good_karma) // old comments with good average
				{
					if ($recent->karma_avg > $good_karma)
						$karma_diff = min($coef_raise * $recent->karma_avg * $old->karma_avg / pow($coef * $good_karma, 2) * $old->cmt_count, 1000); // little push
					elseif ($recent->karma_avg > 0)
						$karma_diff = min($coef_raise * $recent->karma_avg * $old->karma_avg / pow($coef * $good_karma, 2) * $old->cmt_count, 1000); // smaller push
				}
			}
			elseif($old->cmt_count > $threshold)
			{
				$karma_diff = min($coef_raise * $old->karma_avg * $old->cmt_count / ($coef * $good_karma), 1000); // the better the average, the more comments: the nicer the gift...
			//$this->log_msg($coef_raise . " # " . $old->karma_avg . " # " . $old->cmt_count  . " # " . $coef . " # " . $good_karma . " ## " . $karma_diff, 8);		
			}
		}



		$this->log_msg("Snowball karma_diff : $karma_diff", 3);


		if ($karma_diff)
		{
			$log = sprintf(__("Commenter granularity (based on %s): %d old comment(s) (karma avg: %f), %d recent comment(s) (karma avg: %f).", 'sk2'), $criterion, $old->cmt_count, round($old->karma_avg, 2), $recent->cmt_count, round($recent->karma_avg, 2));
			$this->modify_karma($cmt_object, $karma_diff, $log);
			$this->log_msg($log, 3);		
		}
	}
	
	function get_granularity($query_where, $before_after, $now)
	{
		global $wpdb;
		//$now_gmt = gmstrftime("'%Y-%m-%d %H:%M:%S'");
			
		$query = "SELECT COUNT(*) AS `cmt_count`, AVG(`spams`.`karma`) AS `karma_avg` FROM `$wpdb->comments` AS `comments` LEFT JOIN `". sk2_kSpamTable ."` AS `spams` ON `spams`.`comment_ID` = `comments`.`comment_ID` WHERE `comments`.`comment_ID` != $this->ID ";
		if ($before_after == "<")
			$query .= "AND `comments`.`comment_approved` = '1' ";
		$query .= "AND `comments`.`comment_date_gmt` $before_after DATE_SUB('$now', INTERVAL ". $this->get_option_value("old_enough") . " DAY) $query_where";

		if (! $counts = $wpdb->get_row($query))
		{
			$this->log_msg_mysql(__("get_granularity: query failed.", 'sk2') . "<br/> " . __("Query: ", 'sk2') . $query, 7, $this->ID);
			return false;
		}		
		
		$this->log_msg("DEBUG LOG:<br/><code>$query</code><br/>" . print_r($counts, true), 2);
	
		return $counts;
	}
	
}

$this->register_plugin("sk2_snowball_plugin", 6);

?>