<?php
// Basic plugins
// A bunch of simple plugin classes, all lumped into one single file

class sk2_user_level_plugin extends sk2_plugin
{
	var $name = "User Level";
	var $description = "";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_BasicChecks_Plugin";
	var $filter = true;
	var $settings_format = array ("min_level" => array("type" => "text", "value"=> 1, "caption" => "Automatically approve logged-in users above or equal to level:", "size" => 3));
	var $skip_under = -50;
	var $skip_above = 20;
	
	
	function filter_this(&$cmt_object)
	{
		if (! $cmt_object->is_comment())
			return;
		
		$min_level = $this->get_option_value('min_level');
		
		if ($cmt_object->user_id > 0)
		{
			if ($cmt_object->user_level < $min_level)
				$bonus = $cmt_object->user_level + 1; // should give a little bonus no matter what
			else
				$bonus = 25;
			$log = sprintf(__("Commenter logged in. ID: %d, Level: %d", 'sk2'), $cmt_object->user_id, $cmt_object->user_level);
			$this->log_msg($log , 2);
			$this->raise_karma($cmt_object, $bonus, $log);
		}
	}
}

class sk2_entities_plugin extends sk2_plugin
{
	var $name = "Entities Detector";
	var $description = "Detect improper use of HTML entities (used by spammers to foil keyword detection).";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_BasicChecks_Plugin";
	var $filter = true;	
	
	function filter_this(&$cmt_object)
	{
		$this->look_for_entities($cmt_object, "author");
		$this->look_for_entities($cmt_object, "content");
	}
	
	function look_for_entities(&$cmt_object, $part)
	{
		$hit = $letter_entities = 0;
		if ($total = preg_match_all('|&#([0-9]{1,5});|', $cmt_object->$part, $matches))
			foreach($matches[1] as $match)
				if ( (($match >= 65) && ($match <= 90))
					|| (($match >= 97) && ($match <= 122)))
						$letter_entities++;

		if ($double_entities = preg_match_all('|&amp;#[0-9]{1,2};|', $cmt_object->$part, $matches))
		{
			$log = sprintf(__ngettext("Comment %s contains %d <em>double</em> entity", "Comment %s contains %d <em>double</em> entities ", $double_entities, 'sk2'), $part, $double_entities) . " " . sprintf(__ngettext("and one regular entity coding for a letter (%d total).", " and %d regular entities coding for a letter (%d total).", $letter_entities, 'sk2'), $letter_entities, $total);
			$hit = $double_entities * 5 + $letter_entities *2;
		}
		elseif($letter_entities)
		{
			$log = sprintf(__ngettext("Comment %s contains %d entity coding for a letter (%d total).", "Comment one contains %d entities coding for a letter (%d total).", $letter_entities), $part, $letter_entities, $total, 'sk2');
			$hit = 1+ $letter_entities * 2;
		}

		if ($hit)
		{
			$this->log_msg($log , 2);
			$this->hit_karma($cmt_object, $hit, $log);
		}
	
	}
}


class sk2_link_count_plugin extends sk2_plugin
{
	var $name = "Link Counter";
	var $description = "";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_BasicChecks_Plugin";
	var $filter = true;
	var $settings_format = array ("too_many_links" => array("type" => "text", "value"=>2, "caption" => "Penalize if there are more than ", "size" => 3, "after" => "links in the comment content."));
	var $skip_under = -30;
	var $skip_above = 10;
	
	
	function filter_this(&$cmt_object)
	{
		$url_count = count($cmt_object->content_links) + (0.75 * count($cmt_object->content_url_no_links));
		if (! $url_count)
		{
			if (empty($cmt_object->author_url['href']))
			{
				$log = "Comment contains no URL at all.";
				$this->raise_karma($cmt_object, 2, $log); // only possible abuse might be to try and get many comments approved in abuse to use snowball effect
				$this->log_msg($log , 1);
			}
			else
			{
				$log = "Comment has no URL in content (but one author URL)";
				$this->raise_karma($cmt_object, 0.5, $log); // verrrry light bonus
				$this->log_msg($log , 1);
			}		
			
			return;
		}
		
		$threshold = max($this->get_option_value('too_many_links'), 1);
		$log = sprintf(__("Comment contains: %d linked URLs and %d unlinked URLs: total link coef: %d", 'sk2'), count($cmt_object->content_links), count($cmt_object->content_url_no_links), $url_count);

		if ($url_count < $threshold)
		{
			$log .= __(" < threshold", 'sk2') . " ($threshold).";
			$this->log_msg($log , 1);
		}
		else
		{
			$len = strlen($cmt_object->content_filtered);
			$chars_per_url = 150;
			$hit = pow($url_count / $threshold, 2) * max(0.20, ($url_count * $chars_per_url / ($len + $chars_per_url)));
			$log .= __(" >= threshold", 'sk2') . " ($threshold). " . sprintf(__("Non-URL text size: %d chars.", 'sk2'), $len);
			$this->hit_karma($cmt_object, 
							$hit, 
							$log);
			$this->log_msg($log . " " . sprintf(__("Hitting for: %d karma points.", 'sk2'), round($hit, 2)), 2);
		}
	}

}

class sk2_old_post_plugin extends sk2_plugin
{
	var $name = "Post Age and Activity";
	var $description = "Stricter on old posts showing no recent activity.";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_BasicChecks_Plugin";
	var $filter = true;
	var $settings_format = array ("old_when" => array("type" => "text", "value"=>15, "caption" => "Consider a post old after ", "size" => 3, "after" => "days."), "still_active" => array("type" => "text", "value"=>2, "caption" => "Still active if more than ", "size" => 3, "after" => "comments recently."));
	var $skip_under = -30;
	var $skip_above = 2;
	
	
	function filter_this(&$cmt_object)
	{
		$post_ts = strtotime($cmt_object->post_date . " GMT");
		$post_timesince = sk2_time_since($post_ts);
		$old_when = max($this->get_option_value('old_when'), 1);
		$still_active = max($this->get_option_value('still_active'), 1);

		global $wpdb;
		
		$count_cmts = $wpdb->get_var("SELECT COUNT(*) AS `cmt_count` FROM `$wpdb->comments` AS `comments` WHERE `comments`.`comment_ID` != $cmt_object->ID AND `comment_post_ID` = $cmt_object->post_ID AND `comment_approved` = '1' AND `comment_date_gmt` > DATE_SUB(NOW() , INTERVAL ". $this->get_option_value("old_when") . " DAY) ");

		$log = sprintf(__("Entry posted %s ago. %d comments in the past %d days. Current Karma: %d.", 'sk2'), $post_timesince, $count_cmts, $old_when, $cmt_object->karma);
		
		if ($post_ts + ($old_when * 86400) < time())
		{
			if ($count_cmts < $still_active)
			{
				if ($cmt_object->karma <= 2)
				{
					$tot_cmts = 1 + $wpdb->get_var("SELECT COUNT(*) AS `cmt_count` FROM `$wpdb->comments` AS `comments` WHERE `comments`.`comment_ID` != $cmt_object->ID AND `comment_post_ID` = $cmt_object->post_ID AND `comment_approved` = '1'");
					if ($cmt_object->karma <= 0)
					{
						$hit = ($still_active / $tot_cmts) * min((time() - $post_ts) / ($old_when * 86400), 10) * min ((1 - $cmt_object->karma) / 5, 2);
					}
					else
					{
						$hit = max (($still_active / $tot_cmts) * min((time() - $post_ts) / ($old_when * 86400), 10) * (0.25 / $cmt_object->karma), 5); // trying to stay within captcha threshold...
					}
					$this->hit_karma($cmt_object, $hit, $log);
					$this->log_msg($log . " " . sprintf(__("Hitting for: %d karma points.", 'sk2'), round($hit, 2)), 2);
				}
			}
		}
		elseif (($cmt_object->karma > 0)
				&& ($count_cmts > 2 * $still_active))
		{
			$bonus = min (3, ($cmt_object->karma * $count_cmts / (10 * $still_active)));
			$this->raise_karma($cmt_object, $bonus, $log);
			$this->log_msg($log . " " . sprintf(__("Rewarding with: %d karma points.", 'sk2'), round($bonus, 2)), 2);
		}
	}

}

class sk2_stopwatch_plugin extends sk2_plugin
{
	var $name = "Stopwatch";
	var $description = "Makes sure commenter has been on page for a certain number of seconds before commenting.";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_BasicChecks_Plugin";
	var $filter = true;
	var $settings_format = array ("too_too_fast" => array("type" => "text", "caption" => "Hit hard if posted less than ", "size" => 3, "value" => 3, "after" => "seconds after first load.", "advanced" => true), "too_fast" => array("type" => "text", "caption" => "Hit light if posted less than ", "size" => 3, "value" => 13, "after" => "seconds after first load.", "advanced" => true));
	var $skip_under = -15;
	var $skip_above = 10;
	
	
	function filter_this(&$cmt_object)
	{
		$ts = @$_REQUEST['sk2_time'];
		if ($ts <= 0)
			return;
		if (($delta_ts = time() - $ts) < 0)
			return;
		$too_fast = max($this->get_option_value('too_fast'), 1);
		$too_too_fast = max($this->get_option_value('too_too_fast'), 1);
		
			if ($delta_ts <= $too_fast)
			{
				$log = sprintf(__("Flash Gordon was here (comment posted %d seconds after page load).", 'sk2'), $delta_ts);
				if($delta_ts <= $too_too_fast)
					$this->hit_karma($cmt_object, 6, $log); 
				else
					$this->hit_karma($cmt_object, 2, $log); 
				$this->log_msg($log , 1);
			}
			
			return;
	}

}


$this->register_plugin("sk2_user_level_plugin", 1); // so basic we should go there first
$this->register_plugin("sk2_link_count_plugin", 2); // idem
$this->register_plugin("sk2_stopwatch_plugin", 2); // idem
$this->register_plugin("sk2_entities_plugin", 3); 
$this->register_plugin("sk2_old_post_plugin", 7); 


?>