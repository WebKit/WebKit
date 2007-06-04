<?php
define ("sk2_kBlacklistTable", "sk2_blacklist");

global $sk2_blacklist;
if (! isset($sk2_blacklist))
	$sk2_blacklist = new sk2_blacklist;

class sk2_blacklist
{
	function sk2_blacklist()
	{
	
	}
	
	function add_entry($type, $value, $score = 100, $user_reviewed = "no", $added_by = "unknown", $trust = 100)
	{
		global $wpdb;
		
		if (($type == "domain_black" || $type == "domain_white") 
				&& ($grey_rslt = $wpdb->get_results("SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE `type` = 'domain_grey' AND `value` = '$value'")))
		{
			$this->log_msg(__("Greylist match. Skipping blacklist entry insertion: ", 'sk2') . "<em>$type</em> - <em> $value</em>.", 7);
			return 0;
		}
		
		$score = min(100, max($score, 0));
		
		$value = trim($value);
		if (empty($value))
		{
			$this->log_msg(__("Cannot add blacklist entry. Please fill in a value."), 7);
			return false;
		}
		elseif ($wpdb->get_var("SELECT COUNT(*) FROM `". sk2_kBlacklistTable . "` WHERE `type`='$type' AND `value`='" . sk2_escape_string($value) . "' LIMIT 1"))
		{
			$this->log_msg(__("Skipping duplicate blacklist entry: ", 'sk2') . "<em>$type</em> - <em> $value</em>.", 7);
		}
		else
		{
			if ($wpdb->query("INSERT INTO `". sk2_kBlacklistTable . "` SET `type`='$type', `value`='" . sk2_escape_string($value) . "', `added` = NOW(), `last_used` = NOW(), `score` = $score, `trust` = $trust, `user_reviewed` = '$user_reviewed', `added_by` = '$added_by', `comments` = ''"))
					$this->log_msg(__("Successfully inserted blacklist entry: ", 'sk2') . "<em>$type</em> - <em>$value</em>.", 3);
			else
					$this->log_msg(__("Failed to insert blacklist entry: ", 'sk2') . "<em>$type</em> - <em>$value</em>.", 8, true);
		}
		
		return $wpdb->insert_id;
	}

	function auto_add($type, $value, $score = 100, $user_reviewed = "no", $added_by = "unknown", $trust = 100)
	{
		global $wpdb;
		
		$score = min(100, max($score, 0));
		
		if (empty($value))
			$this->log_msg(__("Cannot add blacklist entry. Please fill in a value."), 7);
		elseif	 (($type == "domain_black" || $type == "domain_white")
			&& ($grey_rslt = $wpdb->get_results("SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE `type` = 'domain_grey' AND `value` = '$value'")))
		{
			$this->log_msg(__("Greylist match. Skipping blacklist entry insertion: ", 'sk2') . "<em>$type</em> - <em> $value</em>.", 6);
			return;
		}
		elseif ($row = $wpdb->get_row("SELECT `id`, `score` FROM `". sk2_kBlacklistTable . "` WHERE `type`='$type' AND `value`='" . sk2_escape_string($value) . "' LIMIT 1"))
		{
			if (($old_score = $row->score) >= 100)
				return true;
			$query = "UPDATE `". sk2_kBlacklistTable . "` SET ";
			$query_where = " WHERE `id` = " . $row->id;
		}
		else
		{
			$query = "INSERT INTO `". sk2_kBlacklistTable . "` SET `type`='$type', `value`='" . sk2_escape_string($value) . "', `added` = NOW(), `last_used` = NOW(), `trust` = $trust, `user_reviewed` = '$user_reviewed', `added_by` = '$added_by', `comments` = '',";
			$query_where = "";
			$old_score = 0;
		}
		
		$score = round(max($old_score, (3 * $old_score + $score) / 4));
		
		$wpdb->query($query . "`score` = $score" . $query_where);
		
		if (! mysql_error())
		{
			$this->log_msg(__("Successfully inserted/updated blacklist entry: ", 'sk2') . "<em>$type</em> - <em>$value</em>. " . __("Current score: ", 'sk2') . $score, 3);
			return true;
		}
		else
			$this->log_msg(__("Failed to insert blacklist entry: ", 'sk2') . "<em>$type</em> - <em>$value</em>.", 8, true);
	}



	function match_entries($match_type, $match_value, $strict = true, $min_score = 0)
	{
		global $wpdb;
		
		if ($strict)
			$sql_match = "= '" . sk2_escape_string($match_value) . "'";
		else
			$sql_match = "LIKE '%". sk2_escape_string($match_value) . "%'";

		 switch ($match_type)
		 {
			case 'url':
			case 'url_black':
			case 'url_white':
				if ($match_type == 'url_black')
				{
					$query_where = "(`value` " . strtolower($sql_match) . " AND (`type` = 'domain_black')) OR (`id` IN(";
					$query_where_regex = "`type` = 'regex_black'";
				}
				elseif($match_type == 'url_white')
				{
					$query_where = "(`value` " . strtolower($sql_match) . " AND `type` = 'domain_white') OR (`id` IN(";
					$query_where_regex = "`type` = 'regex_white'";
				}
				else
				{
					$query_where = "(`value` " . strtolower($sql_match) . " AND (`type` = 'domain_black' OR `type` = 'domain_white' OR `type` = 'domain_grey')) OR (`id` IN(";
					$query_where_regex = "`type` = 'regex_white' OR `type` = 'regex_black'";
				}
				
				if ($regex_recs = $wpdb->get_results("SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE $query_where_regex"))
					foreach($regex_recs as $regex_rec)
					{
						//echo $regex_rec->value, " ?match? " , $match_value;
						if (preg_match($regex_rec->value, $match_value))
							$query_where .= $regex_rec->id . ", ";
					}
				$query_where .= "-1))";
			break;
						
			case 'regex_match':
			case 'regex_content_match':
				if ($match_type == 'regex_match')
					$type = 'regex';
				else
					$type = 'regex_content';
				$query_where = "`id` IN(";
				if ($regex_recs = $wpdb->get_results("SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE `type` = '${type}_white' OR `type` = '${type}_black'"))
					foreach($regex_recs as $regex_rec)
					{
						//echo $regex_rec->value, " ?match? " , $match_value;
						$res = @preg_match($regex_rec->value, $match_value);
						if ($res === FALSE)
							$this->log_msg(sprintf(__("Regex ID: %d (<code>%s</code>) appears to be an invalid regex string! Please fix it in the Blacklist control panel.", 'sk2'), $regex_rec->id, $regex_rec->value), 7);
						elseif ($res)
							$query_where .= $regex_rec->id . ", ";
					}
				$query_where .= "-1)";
			break;

			case 'domain_black':
			case 'ip_black':
			case 'domain_white':
			case 'ip_white':
				if (($match_type == 'domain_black' || $match_type == 'domain_white')
					&& ($grey_rslt = $wpdb->get_results("SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE `type` = 'domain_grey' AND `value` $sql_match")))
				{
					$query_where = "";
					$this->log_msg(__("Grey blacklist match: ignoring."), 6);				
				}
				else
					$query_where = "(`value` $sql_match AND `type` = '" . $match_type . "')";
			break;

			
			case 'domain':
			case 'ip':
			case 'regex':
				if (($match_type == 'domain')
					&& ($grey_rslt = $wpdb->get_results("SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE `type` = 'domain_grey' AND `value` $sql_match")))
				{
					$query_where = "";
					$this->log_msg(__("Grey blacklist match: ignoring."), 6);					
				}
				else
				{
					//$this->log_msg("BLAAAAA: $sql_match. ". "SELECT * FROM `" . sk2_kBlacklistTable . "` WHERE `type` = 'domain_grey' AND `value` $sql_match", 7);					

					$query_where = "(`value` $sql_match AND (`type` = '" . $match_type . "_black' OR `type` = '" . $match_type . "_white'))";
				}
			break;
			
			case 'all':
				 $query_where = "`value` $sql_match";
			break;
			
			case 'kumo_seed':
			case 'rbl_server':
			default:
				$query_where = "`value` $sql_match AND `type` = '$match_type'";
			break;
		 }
	
		if (empty($query_where))
		{
				return false;
		}
		else
		{
			if ($min_score)
				$query_where .= " AND `score` > $min_score";
			if ($min_trust)
				$query_where .= " AND `trust` > $min_trust";
	
			$query = "SELECT * FROM `". sk2_kBlacklistTable . "` WHERE $query_where ORDER BY `score` DESC";
			//echo $query;
			$blacklist_rows = $wpdb->get_results($query);
			if (mysql_error())
			{
				$this->log_msg(__("Failed to query blacklist: ", 'sk2') . "<em>$match_type</em> - <em>$match_value</em>. ". __("Query: ", 'sk2') . $query, 8, true);
				return false;
			}
			return $blacklist_rows;
		}
	}
	
	function get_list($type, $limit = 0)
	{
		global $wpdb;
		$query = "SELECT * FROM `". sk2_kBlacklistTable. "` WHERE `type` = '$type'";
		if ($limit)
			$query .= " LIMIT $limit";
		$list = $wpdb->get_results($query);
		if (mysql_error())
		{
			$this->log_msg(__("get_list: Failed to get blacklist entries of type: ", 'sk2') . "<em>$type</em>. " . __("Query: ", 'sk2'). $query, 8, true);
			return false;
		}
	
		return ($list);
	}
	
	function increment_used ($ids)
	{
		global $wpdb;
		$str2 = $str = "(";
		foreach($ids as $id => $val_array)
		{
			$str .= $id . ", ";
			$str2 .= $id . " = " . $val_array['value'] . " [x". $val_array['used'] ."], ";
		}
		$str = substr($str, 0, strlen($str) - 2) . ")";
		$str2 = substr($str2, 0, strlen($str2) - 2) . ")";
		
		$query = "UPDATE `". sk2_kBlacklistTable . "` SET `used_count` = `used_count` + 1, `last_used` = NOW() WHERE `id` IN $str";
		$wpdb->query($query);
		if (mysql_error())
			$this->log_msg(__("Failed to update blacklist used count.", 'sk2') . "</br>" . __("Query: ", 'sk2') . $query, 8, true);
		
		return $str2;
	}

	function downgrade_entries ($ids)
	{
		global $wpdb;
		$str2 = $str = "(";
		foreach($ids as $id => $val_array)
		{
			$str .= $id . ", ";
			$str2 .= $id . " = " . $val_array['value'] . " [x". $val_array['used'] ."], ";
		}
		$str = substr($str, 0, strlen($str) - 2) . ")";
		$str2 = substr($str2, 0, strlen($str2) - 2) . ")";
		
		$query = "UPDATE `". sk2_kBlacklistTable . "` SET `score` = 0, `last_used` = NOW() WHERE `id` IN $str";
		$wpdb->query($query);
		if (mysql_error())
			$this->log_msg(__("Failed to downgrade blacklist scores.", 'sk2') . "</br> " . __("Query: ", 'sk2') . $query, 8, true);
		
		return $str2;
	}

	function log_msg($msg, $level = 0, $mysql = false)
	{
		global $sk2_log;
		if ($mysql)
			$sk2_log->log_msg_mysql($msg, $level, 0, "blacklist");
		else
			$sk2_log->log_msg($msg, $level, 0, "blacklist");
	}

}

?>