<?php
/*
	RBL Filter for Spam Karma 2
	Runs URLs and IPs through each RBL
	(c) 2005 James Seward and DrDave
*/

class sk2_rbl_plugin extends sk2_plugin
{
	var $name = "RBL Check";
	var $author = "James Seward";
	var $author_email = "james@jamesoff.net";
	var $plugin_version = 1;
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_RBL_Plugin";
	var $description = "Checks comment IP and URLs with Realtime Blacklist servers (can be added in the Blacklist panel).";
	var $filter = true;
	var $treatment = true;
	var $skip_under = -5;
	var $skip_above = 5;
	
	var $settings_format = array
	(
		"do_submit" => array
		(
			"type" => "checkbox",
			"caption" => "Submit data from spam comments to RBL",
			"value" => "1"
		),
		
		"submit_limit" => array
		(
			"type" => "text", 
			"value"=> -5, 
			"caption" => "Submit IPs and URIs if karma below:",
			"after" => "",
			"size" => 4,
			"advanced" => true
		),
		
		"submit_ip_target" => array
		(
			"type" => "text",
			"value" => "http://blbl.org/add?data=%s",
			"caption" => "URL for submitting IPs:",
			"size" => "50",
			"advanced" => true
		),
		
		"submit_uri_target" => array
		(
			"type" => "text",
			"value" => "http://blbl.org/add?data=uri:%s",
			"caption" => "URL for submitting URIs:",
			"size" => "50",
			"advanced" => true
		),
		
		"submit_target" => array
		(
			"type" => "text",
			"value" => "http://blbl.org/add2",
			"caption" => "URL for bulk submitting data with CURL:",
			"size" => "50", 
			"advanced" => true
		),
	);	

 /*
  Looks up host in RBL
  Returns the returned RBL host if found, else the empty string
 */
 function rbl_lookup($rbl_host)
 {
  if (empty($rbl_host)) {
   return '';
  }
  
  /* gethostbyname() seems to hate hostnames with "/" in
    if we have a "/" in the host, look it up this way
    instead which works but doesn't give us the returned data.
    dns_get_record() relies on PHP5 so I'm not going to use it */
  if (strpos($rbl_host, '/') !== false)
  {
  	//BUT WAIT, checkdnsrr() is only available on UNIX
  	if (function_exists('checkdnsrr'))
  	{
	  	$this->log_msg(sprintf(__("%s has a slash in it; falling back to checkdnsrr :(", 'sk2'), $rbl_host), 0);
 	 	if (checkdnsrr($rbl_host, "A")) 
 	 	{
 	 		return "127.0.133.7"; //fake a reply (mmm, 1337)
 	 	}
 	 	else 
 	 	{
 	 		//not listed
 	 		return '';
 	 	}
 	 }
 	 else
 	 {
 	 	//no checkdnsrr :( we'll have to fail-safe
 	 	return '';
 	 }
  }
  
  //no "/" in the host, so we'll do it the usual way
  $get_host = gethostbyname($rbl_host);
  $this->log_msg(sprintf(__("rbl_lookup: lookup for %s returned %s", 'sk2'), $rbl_host, $get_host), 2);
  if ($get_host
   && ($get_host != $rbl_host)
   && ($rbl_host != $_SERVER['HTTP_HOST'])
   && ($rbl_host != $_SERVER['SERVER_ADDR'])
   && (strncmp($rbl_host, "127.", 4) == 0))
  {
   return $get_host;
  }
  else
  {
   return '';
  }
 }
	
	
	function filter_this(&$cmt_object)
	{					
		global $sk2_blacklist; 

		$check_list[] = implode( '.', array_reverse( explode( '.', $cmt_object->author_ip ) ) );
		$used_ids = array();

		if ($rbl_list = $sk2_blacklist->get_list('rbl_server'))
		{
			foreach ($rbl_list as $rbl_row)
			{
				$rbl_server = $rbl_row->value;
	
				foreach($check_list as $this_addr)
				{
					$rbl_host = $this_addr . "." . $rbl_server . "."; // adding a final dot to solve small bug...
	
					if (($get_host = $this->rbl_lookup($rbl_host))
						&& ($get_host != ''))
					{
						$this->hit_karma($cmt_object, 5, sprintf(__("IP (%s) matched RBL server %s (returned: %s)", 'sk2'), $cmt_object->author_ip, $rbl_server , "<em>$get_host</em>"));
						$used_ids[] = $rbl_row->id;
					}
					else //DEBUG
						$this->log_msg(sprintf(__("IP ($s) not listed by RBL %s (response: %s)", 'sk2'), $cmt_object->author_ip, $rbl_server, $get_host), 0);
					
				}
				if ($cmt_object->karma < -10)
					break;
			}
		}

		// now let's go though the URIs against the URL RBLs
		if ($rbl_list = $sk2_blacklist->get_list('rbl_server_uri'))
		{
			foreach ($rbl_list as $rbl_row)
			{
				$rbl_server = $rbl_row->value;
				
				//check author's URL on this RBL
				$author_url = $cmt_object->author_url['url'];
				$rbl_host = $author_url . "." . $rbl_server . ".";
				if (($get_host = $this->rbl_lookup($rbl_host))
					&& ($get_host != ''))
				{
					$this->hit_karma($cmt_object, 5, sprintf(__("Author's URL (%s) appears in RBL at %s (returned: %s)", 'sk2'), $author_url, $rbl_server, "<em>$get_host</em>"));
					$used_ids[] = $rbl_row->id;
				}
				else
					$this->log_msg(sprintf(__("Author's URL (%s) does not appear in RBL %s", 'sk2'), $author_url, $rbl_server),3);				

				$found_uris = array_merge($cmt_object->content_links, $cmt_object->content_url_no_links);

				foreach ($found_uris as $url_info)
				{
					$rbl_host = trim($url_info['url']) . "." . $rbl_server . ".";
					if (($get_host = $this->rbl_lookup($rbl_host))
						&& ($get_host != ''))
					{
						$this->hit_karma($cmt_object, 5, sprintf(__("URI (%s) matched RBL server %s (returned: %s)", 'sk2'), $url_info['url'], $rbl_server, "<em>$get_host</em>"));
						$used_ids[] = $rbl_row->id;
					}
					else 
					{
						//debug
						$this->log_msg(sprintf(__("URI %s not listed by RBL %s", 'sk2'), $rbl_host, $rbl_server), 3);
					}
				}
				if ($cmt_object->karma < -10)
					break;
			}
		}
		
		if (count ($used_ids))
			$sk2_blacklist->increment_used ($used_ids);

	}

	function update_SQL_schema($cur_version)
	{
		global $sk2_blacklist; 
		
		foreach (array("opm.blitzed.org", "bl.blbl.org") as $rbl)
		{
			$sk2_blacklist->add_entry("rbl_server", $rbl, 100, "yes", "default", 100);
			$this->log_msg(__("Added default IP RBL server entry: ", 'sk2') . $rbl, 4);
		}
		
		//$cur_version == 1 if plugin lacked version previously; 0 if never run before
		if ((($cur_version == 1) && ($this->plugin_version == 2)) || ($cur_version == 0))
		{
			//Yes, a foreach() for one item is a bit pointless, but it's futar-proof
			foreach (array("uri-bl.blbl.org") as $rbl)
			{
				$sk2_blacklist->add_entry("rbl_server_uri", $rbl, 100, "yes", "default", 100);
				$this->log_msg(__("Added default URI RBL server entry: ", 'sk2') . $rbl, 4);
			}
		}

		return true;
	}
	
	function treat_this($cmt_object)
	{
		if ($this->get_option_value('do_submit') != "on") {
			return;
		}

		//do we need to submit at all?
		if ($cmt_object->karma > $this->get_option_value('submit_limit')) 
		{
			return;
		}

		//check if we have curl
		if (function_exists('curl_init'))
		{
				$this->log_msg(__("Submitting to URL using CURL"), 3);
				
				//new submit logic
				// build a postlist of <ip|url>: <data>
				// submit using curl (needs updated sk2_functions.php)
				$postinfo = array();
				$entries = 0;
				
				//author's IP
				if (!empty($cmt_object->author_ip))
				{
					$postinfo["ip$entries"] = urlencode(trim($cmt_object->author_ip));
					$entries++;
				}
				
				//author URL
				if (!empty($cmt_object->author_url['url']))
				{
					$postinfo["url$entries"] = urlencode(trim($cmt_object->author_url['url']));
					$entries++;
				}
				
				//all the URIs
				$found_uris = array_merge($cmt_object->content_links, $cmt_object->content_url_no_links);
				$done_uris = array();
				foreach ($found_uris as $url_info)
	  	{
	  		if (!in_array($url_info['url'], $done_uris))
	  		{
						$postinfo["url$entries"] = urlencode(trim($url_info['url']));
						$done_urlis[] = $url_info['url'];
					}
				}
				
				//print_r($postinfo);
				//exit();
				
				if (!empty($postinfo))
				{
					//fetch result
					$result = sk2_url_fopen($this->get_option_value('submit_target'), false, $postinfo);
					$this->log_msg(sprintf(__("Submitted %s total entries to RBL using POST; got result %s", 'sk2'), sizeof($postinfo), $result), 4);
				}
				
				return;
		}
			
		//curl doesn't exist, so do it the old way...
		$this->log_msg(__("Submitting to URL using GET"), 3);
		
		$ipcount = $uricount = 0;
		
		
		//first, submit the author's ip
 	$ip = $cmt_object->author_ip;
 	if (!empty($ip)) 
 	{
 		$url = sprintf($this->get_option_value('submit_ip_target'), $ip);
			$this->log_msg(__("Opening URL: ", 'sk2') . $url, 0);
			$result = sk2_url_fopen($url);
 		$this->log_msg(sprintf(__("Adding IP %s to RBL returned %s", 'sk2'), $ip, $result), 2);
 		$ipcount++;
 	}

		//submit the author URI
		if (!empty($cmt_object->author_url['url']))
		{
			$url = sprintf($this->get_option_value('submit_uri_target'), $cmt_object->author_url['url']);
			$this->log_msg(__("Opening URL: ", 'sk2') . $url, 0);
			$result = sk2_url_fopen($url);
			$this->log_msg(sprintf(__("Adding author URI %s to RBL returned %s", 'sk2'), $cmt_object->author_url['url'], $result), 2);
			$uricount++;
		}

		//now, submit all the URIs
 	$found_uris = array_merge($cmt_object->content_links, $cmt_object->content_url_no_links);
 	$done_uris = array();
 	foreach ($found_uris as $url_info)
 	{
 		if (!in_array($url_info['url'], $done_uris))
 		{
 			$done_uris[] = $url_info['url'];
				$url = sprintf($this->get_option_value('submit_uri_target'), trim($url_info['url']));
				$this->log_msg(__("Opening URL: ", 'sk2') . $url, 0);
				$result = sk2_url_fopen($url);	
  		$this->log_msg(sprintf(__("Adding URI %s to RBL returned %s", 'sk2'), $url_info['url'], $result), 2);
  		$uricount++;
  	}
		}
		
		$this->log_msg(sprintf(__("Submitted %d IP(s) and %d URI(s) to RBL using GET", 'sk2'), $ipcount, $uricount), 4);
	}
}

$this->register_plugin("sk2_rbl_plugin", 8);

?>
