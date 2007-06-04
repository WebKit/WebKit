<?php
// Blacklist Filter
// Runs URLs and IPs through each blacklist

class sk2_blacklist_plugin extends sk2_plugin
{
	var $name = "Blacklist";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_Blacklist_Plugin";
	var $filter = true;
	var $treatment = true;
	var $description = "";
	var $settings_format = array ("auto_blacklist" => array("advanced" => true, "type" => "text", "value"=> -10, "caption" => "Automatically blacklist URLs and IP when karma under", "size" => 3), "auto_whitelist" => array("advanced" => true, "type" => "text", "value"=> 10, "caption" => "Automatically whitelist URLs and IP when karma over", "size" => 3));
	
	var $blackmatch_count = 0;
	var $whitematch_count = 0;
	var $karma_hit = 0;
	var $karma_bonus = 0;
	var $black_ids;
	var $white_ids;
	var $min_score = 30;
	var $skip_under = -25;

	var $version = 2;

	function sk2_blacklist_plugin()
	{
		parent::__construct();
		$this->description = "<a href=\"options-general.php?page=" . $_REQUEST['page'] . "&sk2_section=blacklist\">Click here</a> to manage your blacklist.";
	}
	
	function filter_this(&$cmt_object)
	{
		global $sk2_blacklist;
	//	echo "<pre>";
	//	print_r($cmt_object->content_links);
	//	print_r($cmt_object->author_url);
	//	echo "</pre>";
		$this->blackmatch_count = 0;
		$this->whitematch_count = 0;
		$this->karma_hit = 0;
		$this->karma_bonus = 0;
		$this->black_ids = array();
		$this->white_ids = array();
		
		$this->log_msg(__("Running sk2_blacklist plugin on comment ID: ", 'sk2') . $cmt_object->ID, 0);
		
		// matching IP
		if (! empty($cmt_object->author_ip))
			$cmt_object->ip_listed = $this->find_matches("ip", $cmt_object->author_ip, 10, 15);	

		// matching author URL
		if ($cmt_object->author_url)
		{
			//echo "<pre>";
			//print_r($cmt_object->author_url);
			//echo"</pre>";
			$listed = false;
			if (! empty($cmt_object->author_url['domain']))
				$listed = ($this->find_matches("domain", $cmt_object->author_url['domain'], 12, 4)
										|| $this->find_matches("regex_match", $cmt_object->author_url['url'], 10, 4));
			elseif(! empty($cmt_object->author_url['url']))
				$listed = $this->find_matches("url", $cmt_object->author_url['url'], 7, 2);

			$cmt_object->author_url['listed'] = $listed;
		}

		// matching content links
		if ($cmt_object->content_links)
			foreach($cmt_object->content_links as $id => $link)
			{
				//echo "<pre>";
				//print_r($link);
				//echo"</pre>";
				$listed = false;
				if (! empty($link['domain']))
				{
					$listed = ($this->find_matches("domain", $link['domain'], 15, 5)
										|| $this->find_matches("regex_match", $link['url'], 10, 5));
				}
				elseif(! empty($link['url']))
					$listed = $this->find_matches("url", $link['url'], 7, 3);

				$cmt_object->content_links[$id]['listed'] = $listed;
			}
		
		// matching content links outside of <a> tags
		if ($cmt_object->content_url_no_links)
			foreach($cmt_object->content_url_no_links as $id => $link)
			{
				$listed = false;
				if (! empty($link['domain']))
				{
					$listed = ($this->find_matches("domain", $link['domain'], 5, 0)
										|| $this->find_matches("regex_match", $link['url'], 5, 0));
				}
				elseif(! empty($link['url']))
					$listed = $this->find_matches("url", $link['url'], 3, 0);

				$cmt_object->content_url_no_links[$id]['listed'] = $listed;
			}
			
		// matching content
		if (!empty ($cmt_object->content_filtered))
			$this->find_matches("regex_content_match", $cmt_object->content_filtered, 5, 3);

		foreach (array("white", "black") as $color)
			if($this->{$color . 'match_count'})
			{
				$str = $sk2_blacklist->increment_used($this->{$color . '_ids'});
				
				$log = $this->{$color . 'match_count'} . " ${color}list match" . (($this->{$color . 'match_count'} > 1) ? "es" : "") . ". ";
	
				if ($color == "black")
					$this->hit_karma($cmt_object, $this->karma_hit, $log . " $str");
				else
					$this->raise_karma($cmt_object, $this->karma_bonus, $log . " $str");
				$log .= " for comment ID ". $cmt_object->ID .". " . ($color == "white" ? "Adding " . $this->karma_bonus : "Removing " . $this->karma_hit) . " karma points. $str";
				
				$this->log_msg($log, 5);
				
			}
	}
	
	function treat_this(&$cmt_obj)
	{
		global $sk2_blacklist;

		$auto_blacklist = -($cmt_obj->karma - min (-5, $this->get_option_value("auto_blacklist")));
		$auto_whitelist = ($cmt_obj->karma - max (3, $this->get_option_value("auto_whitelist")));

		if (($auto_blacklist > 0) || ($auto_whitelist > 0))
		{
			if ($auto_whitelist > 0)
			{
				$score = $auto_whitelist * 5 + 10;
				$type = "white";
				$opposite = "black";
			}
			else
			{
				$score = $auto_blacklist * 10 + 10;			
				$type = "black";
				$opposite = "white";
			}
		
			$score *= $this->get_option_value('weight');
			
			if ($cmt_obj->is_post_proc() && ($score > 30))
			{
				$this->find_matches("ip_" . $opposite, $cmt_obj->author_ip);
				if (! empty($cmt_obj->author_url['domain']))
					$this->find_matches("domain_" . $opposite, $cmt_obj->author_url['domain']);
				foreach ($cmt_obj->content_links as $url)
					if (! empty($url['domain']))
						$this->find_matches("domain_" . $opposite, $url['domain']);				
				if ($this->{$opposite . 'match_count'})
				{
					if ($opposite == "black")
						$this->log_msg(__("Downgrading blacklist entries."), 5)	;	
					else
						$this->log_msg(__("Downgrading whitelist entries."), 5);	
					$sk2_blacklist->downgrade_entries($this->{$opposite . '_ids'});
				}
			}
			
			$sk2_blacklist->auto_add("ip_" . $type, $cmt_obj->author_ip, $score, "no", get_class($this));
		
			if (! empty($cmt_obj->author_url['domain']))
				$sk2_blacklist->auto_add("domain_" . $type, $cmt_obj->author_url['domain'], $score, "no", get_class($this));

			foreach ($cmt_obj->content_links as $url)
				if (! empty($url['domain']))
					$sk2_blacklist->auto_add("domain_" . $type, $url['domain'], $score, "no", get_class($this));
		}
	}
	
	
	function find_matches($type, $value, $black_coef = 1, $white_coef = -1)
	{
		global $sk2_blacklist;
		if ($white_coef < 0)
			$white_coef = $black_coef;
			
		if ($blacklist_rows = $sk2_blacklist->match_entries($type, $value, true, $this->min_score))
		{
			foreach($blacklist_rows as $row)
			{
				//echo "<pre>";
				//print_r($row);
				//echo"</pre>";
			
				if ($row->type == "regex_white" || $row->type == "regex_black")
					$coef *= 0.75;
				elseif ($row->type == "regex_content_white" || $row->type == "regex_content_black")
					$coef *= 0.70;
				if ($row->user_reviewed != 'yes')
					$coef *= 0.75;
				
				if($row->type == "domain_grey")
				{
					return 0;
				}
				elseif ($row->type == "ip_black" || $row->type == "domain_black" || $row->type == "regex_black" || $row->type == "regex_content_black")
				{
					if ($black_coef)
					{
						$this->black_ids[$row->id]['value'] = $value;
						if (@$this->black_ids[$row->id]['used'] <= 0)
							$this->black_ids[$row->id]['used'] = 1;
						else
							$this->black_ids[$row->id]['used']++;
						$this->karma_hit += $black_coef * $row->trust * $row->score / 10000;
						$this->blackmatch_count++;
						return "black";
					}
				}
				elseif ($row->type == "ip_white" || $row->type == "domain_white" || $row->type == "regex_white" || $row->type == "regex_content_white")
				{
					if ($white_coef)
					{
						$this->white_ids[$row->id]['value'] = $value;
						if (@$this->white_ids[$row->id]['used'] <= 0)
							$this->white_ids[$row->id]['used'] = 1;
						else
							$this->white_ids[$row->id]['used']++;
						$this->karma_bonus += $white_coef * $row->trust * $row->score / 10000;
						$this->whitematch_count++;
						return "white";
					}
				}
			}
		}

		return 0;
	}
	
	function update_SQL_schema($cur_version)
	{
		global $sk2_blacklist; 

		foreach(array("typepad.com", "blogspot.com", "livejournal.com", "xanga.com") as $grey_domain)
		{
			$sk2_blacklist->add_entry("domain_grey", $grey_domain, 100, "yes", "default", 100);
			$this->log_msg(__("Added default domain_grey entry: ", 'sk2') . $grey_domain, 6);
		}

		return true;
	}	
}

$this->register_plugin("sk2_blacklist_plugin", 2);


?>