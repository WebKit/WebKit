<?php

class sk2_comment
{
	var $ID;
	var $type;
	var $post_proc;
	
	var $author;
	var $author_email;
	var $author_url;

	var $post_ID;
	var $post_date;
		
	var $author_ip;
	var $proxy_ip;
	
	var $cmt_date;
	var $now_sql;

	var $content;
	var $content_links = array(); // array with url, title and linked text of every <a> tags 
	var $content_url_no_links = array(); // urls that are found *outside* of <a> tags
	var $content_filtered; // content with all tags, urls and entities removed 
	
	var $approved;
	var $user_id;	
	var $user_level;
		
	var $cmt_array; // contains all SQL values.

	// private:
	var $karma = 0.0;
	var $karma_cmts;
	var $unlock_keys;
	var $remaining_attempts;
	var $ip_listed;
// Diagnostic data:
	
	function __construct($comment_id, $post_proc = false, $comment_sk_info = 0)
	{
		$this->{__CLASS__}($comment_id, $post_proc, $comment_sk_info);
	}

	
	function sk2_comment($comment_id, $post_proc = false, $comment_sk_info = 0)
	{
		global $wpdb;
		// SAFE WAY:
		// $cmt_array = $wpdb->get_values ("SELECT `". $wpdb->comments . "`.*, `". $wpdb->posts . "`.`post_date`, `". $wpdb->posts . "`.`post_modified`, `". $wpdb->users . "`.`user_level` FROM `". $wpdb->comments . "` LEFT JOIN `". $wpdb->posts . "` ON `". $wpdb->posts . "`.`ID` = `". $wpdb->comments . "`.`post_ID`, LEFT JOIN `". $wpdb->users . "` ON `". $wpdb->users . "`.`ID` = `". $wpdb->comments . "`.`user_id` WHERE `comment_id` = $comment_id");

		// LAZY WAY:
		if (! $cmt_array = $wpdb->get_row ("SELECT `comment_table`.*, `posts_table`.*, `users_table`.*, `spam_table`.*, `spam_table`.`id` AS `spam_table_id`, NOW() AS `now_sql` FROM `". $wpdb->comments . "` AS `comment_table` LEFT JOIN `". $wpdb->posts . "` AS `posts_table` ON `posts_table`.`ID` = `comment_table`.`comment_post_ID` LEFT JOIN `". $wpdb->users . "` AS `users_table` ON `users_table`.`ID` = `comment_table`.`user_id` LEFT JOIN `". sk2_kSpamTable ."` AS `spam_table` ON `spam_table`.`comment_ID` = `comment_table`.`comment_ID` WHERE `comment_table`.`comment_ID` = $comment_id"))
		{
			$this->log_msg(__("sk2_comment: Cannot fetch comment record from table."), 9, true);
			return false;
		}	
		$this->ID = $comment_id;
		$this->type = $cmt_array->comment_type;
		if (empty($this->type))
			$this->type = "comment";
		$this->post_proc = $post_proc;
		
		$this->author = $cmt_array->comment_author;
		$this->author_email = $cmt_array->comment_author_email;

		if (! $this->author_url = $this->extract_domain($cmt_array->comment_author_url))
			$this->author_url = array();
		$this->author_url['href'] = $cmt_array->comment_author_url;
			
		$this->post_ID = $cmt_array->comment_post_ID;
		$this->post_date = $cmt_array->post_date_gmt;
		
		$this->author_ip = $cmt_array->comment_author_IP;
		//###TODO grab proxy IP if any
		
		$this->cmt_date = $cmt_array->comment_date_gmt;
		$this->now_sql = $cmt_array->now_sql;

		$this->content = $cmt_array->comment_content;
		// grab URIs
		$this->parse_URIs();
		
		$this->approved = $cmt_array->comment_approved;
		$this->user_id = $cmt_array->user_id;
		if ((get_bloginfo('version') >= "2.0") && ($this->user_id > 0))
		{
			if ($my_user = new WP_User($this->user_id))
			{
				$i = 10;
				while (!$my_user->has_cap($i) && $i)
					$i--;
				$this->user_level = $i;
			}
		}
		else
			$this->user_level = $cmt_array->user_level;

		$this->cmt_array = $cmt_array;

		if ($comment_sk_info)
		{
			$this->karma = $comment_sk_info['karma'];
			$this->karma_cmts = $comment_sk_info['karma_cmts'];
			$this->unlock_keys = $comment_sk_info['unlock_keys'];
			$this->remaining_attempts = $comment_sk_info['remaining_attempts'];
		}
		elseif ($cmt_array->spam_table_id)
		{
			$this->karma = $cmt_array->karma;
			$this->karma_cmts = unserialize($cmt_array->karma_cmts);
			$this->unlock_keys = unserialize($cmt_array->unlock_keys);
			$this->remaining_attempts = $cmt_array->remaining_attempts;
		}
		else
		{
			global $sk2_settings;
			$this->karma = 0.0;
			$this->karma_cmts = array();
			$this->unlock_keys = array();
			$this->remaining_attempts = $sk2_settings->get_core_settings('max_attempts');
		}

		return true;
	}

	function can_unlock()
	{
		if ($this->remaining_attempts && $this->unlock_keys && count($this->unlock_keys))
			return true;
		else
			return false;
	}
	
	function add_unlock_key($key, $class, $expire)
	{
		if (! is_array($this->unlock_keys))
			$this->unlock_keys = array();	
		$this->unlock_keys[] = array("key" => $key, "class" => $class, "expire" => $expire);
	}
	
	function is_post_proc()
	{
		return $this->post_proc;
	}

	function is_pingback()
		{ return ($this->type == "pingback"); }
	function is_trackback()
		{ return ($this->type == "trackback"); }
	function is_comment()
		{ return ($this->type == "comment" || $this->type == "" ); }

	function log_msg($msg, $level = 0, $mysql = false, $plugin = 'cmt_class')
	{
		global $sk2_log;
		if ($mysql)
			$sk2_log->log_msg_mysql($msg, $level, $this->ID, $plugin);
		else
			$sk2_log->log_msg($msg, $level, $this->ID, $plugin);
	}

	function modify_karma($karma_diff, $plugin_name, $reason = "")
	{
		$karma_diff = round($karma_diff, 2); // let's not get overly picky...
		$this->karma += $karma_diff;
		$this->karma_cmts[] = array("ts" => time(), "hit" => $karma_diff, "plugin" => $plugin_name, "reason" => __($reason, 'sk2'));
	}
	
	function set_karma($new_karma, $plugin_name, $reason = "")
	{
		$this->modify_karma($new_karma - $this->karma, $plugin_name, $reason);
	}

	function set_DB_status($new_status, $plugin = "", $update_karma = false, $id = 0)
	{
		global $wpdb;
		
		if (! $id)
			$id = $this->ID;
		
		switch ($new_status)
		{
			case '0':
			case 'moderated':
				$wp_status = '0';
			break;
			case '1':
			case 'approved':
				$wp_status = '1';
				if ($update_karma)
					$this->set_karma(15, $plugin, "Rescued comment's ass.");
			break;
			case 'spam':
				$wp_status = 'spam';
				if ($update_karma)
					$this->set_karma(-15, $plugin, "Kicked comment's ass.");
			break;
		}
		
		$wpdb->query("UPDATE `$wpdb->comments` SET `comment_approved` = '$wp_status' WHERE `comment_ID` = $id");
		if (! mysql_error())
		{
			global $sk2_settings;

			$this->log_msg(sprintf(__("Successfully updated comment entry ID: %d to status: %s.", 'sk2'), $id, $new_status), 4, false, $plugin);
			$this->approved = $wp_status;
			if ($sk2_settings->is_wp20())
			{
				$c = $wpdb->get_row( "SELECT count(*) as c FROM {$wpdb->comments} WHERE comment_post_ID = '$this->post_ID' AND comment_approved = '1'" );
				if( is_object( $c ) ) 
					$wpdb->query( "UPDATE $wpdb->posts SET comment_count = '$c->c' WHERE ID = '$this->post_ID'" );				
				else 
					$this->log_msg(sprintf(__(" Comment count update for comment_id %d failed", "sk2"), $cmt_object->comment_id), 7); 
			}
			return true;
		}
		else
		{
			$this->log_msg(sprintf(__("Error: cannot update comment entry ID: %d to status: %s.", 'sk2'), $id, $new_status), 7, true, $plugin);
			return false;
		}
	}
	
// private functions: 	
	function parse_URIs ($str = "")
	{
		if (empty($str))
			$str = $this->content;
		$str = $this->remove_entities($str);
		$stri = strtolower($str);
		$matches = array();
		$count = 0;
		foreach (array("<a ", "http://", "www.", "http://www.") as $needle)
		{
			$offset = 0;
			while (($pos = strpos($stri, $needle, $offset)) !== false)
			{
				$matches[(int) $pos] = $needle;
				$offset = $pos + strlen($needle);
				$count++;
			}
		}
		
		ksort($matches);
		$i = $j = $cur_pos = 0;
		$raw_text = "";
		
		if ($count)
		{
			foreach($matches as $pos => $marker)
			{
				if ($pos >= $cur_pos)
				{
					$raw_text .= substr($str, $cur_pos, $pos-$cur_pos-1);
					if ($marker == "<a ")
					{
						if ($tag_end = strpos($str, ">", $pos))
						{
							$this->content_links[$i]['tag'] = @substr($str, $pos+3, $tag_end-$pos-3);
							if (preg_match("/title=(?:\"([^\"]+)\"|'([^']+)')/i", $this->content_links[$i]['tag'], $found))
							{
								if (empty($found[1])) // kind of a php regex bug... should only be one match either way
									$this->content_links[$i]['title'] = $found[2]; 
								else
									$this->content_links[$i]['title'] = $found[1]; 
							}
							
							if ($tag_close = strpos($stri, "</a>", $tag_end))
							{
								$this->content_links[$i]['text'] = @substr($str, $tag_end+1, $tag_close - $tag_end - 1);
								$raw_text .= $this->content_links[$i]['text'];
								$cur_pos = $tag_close + 4;
							}
		
							if (preg_match("/href=(?:\"([^\"]+)\"|'([^']+)')/i", $this->content_links[$i]['tag'], $found))
							{
								if (empty($found[1])) // kind of a php regex bug... should only be one match either way
									$this->content_links[$i]['href'] = $found[2]; 
								else
									$this->content_links[$i]['href'] = $found[1]; 
							}

							if (!empty($this->content_links[$i]['href'] ) 
								&& ($url_domain = $this->extract_domain($this->content_links[$i]['href'])))
							{
								$this->content_links[$i]['url'] = $url_domain['url'];
								$this->content_links[$i]['domain'] = $url_domain['domain'];
								$i++;
							}
							else
								unset($links[$i]);
						}
					}
					else
					{
						if ($this->content_url_no_links[$j] = $this->extract_domain(substr($str, $pos + strlen($marker))))
						{
							$j++;
							$cur_pos = $pos + strlen($marker) + strlen($this->content_url_no_links[$j]['url']);
						}
						else
						{
							unset($this->content_url_no_links[$j]);
							$cur_pos = $pos + strlen($marker);
						}
					}
				}
			}
		}
				
		$this->content_filtered = $raw_text . substr($str, $cur_pos);
	}
	

	function extract_domain($str)
	{
	if(preg_match("/^(?:http[s]?:\\/\\/)?(?:.*@)?((?:[0-9]{1,3}\\.?){4,4})(?::[^\/\\?]*)?([\\/|\\?][^\\s\"']*)?.*/i",
			$str, $matches))
	{
		$parsed = array("domain" => $matches[1], "full_domain" => $matches[1] . $matches[2], "url" => $matches[1] . $matches[2] . @$matches[3]);
	}
	elseif (preg_match("/^(?:http[s]?:\\/\\/)?(?:.*@)?((?:(?:[^\\?\/\\.]*\\.)*)?([^\\?\/\\.]*\\.))([a-zA-Z0-9]{2,5})(\\.[a-zA-Z0-9]{2,5})(?::[^\/\\?]*)?([\\/|\\?][^\\s\"']*)?.*/i",
			$str, $matches))
	{
		$complex_tlds = array ("ac", "ac.cn", "ac.jp", "ac.uk", "ad.jp", "adm.br", "adv.br", "aero", "ag", "agr.br", "ah.cn", "al", "am", "am.br", "arq.br", "at", "au", "art.br", "as", "asn.au", "ato.br", "av.tr", "az", "ba", "be", "bg", "bio.br", "biz", "biz.tr", "bj.cn", "bel.tr", "bmd.br", "br", "by", "ca", "cc", "cd", "ch", "cim.br", "ck", "cl", "cn", "cng.br", "cnt.br", "com", "com.au", "com.br", "com.cn", "com.eg", "com.hk", "com.mx", "com.tr", "com.ru", "com.tw", "conf.au", "co.at", "co.in", "co.jp", "co.uk", "cq.cn", "csiro.au", "cx", "cy", "cz", "de", "dr.tr", "dk", "dz", "ecn.br", "ee", "edu", "edu.au", "edu.br", "edu.tr", "eg", "es", "esp.br", "etc.br", "eti.br", "eun.eg", "eng.br", "far.br", "fi", "fj", "fj.cn", "fm.br", "fnd.br", "fo", "fot.br", "fst.br", "fr", "gb", "gb.com", "gb.net", "g12.br", "gd.cn", "ge", "gen.tr", "ggf.br", "gl", "gr", "gr.jp", "gs", "gs.cn", "gov.au", "gov.br", "gov.cn", "gov.hk", "gov.tr", "gob.mx", "gs", "gz.cn", "gx.cn", "he.cn", "ha.cn", "hb.cn", "hi.cn", "hl.cn", "hn.cn", "hm", "hk", "hk.cn", "hu", "id.au", "ie", "ind.br", "imb.br", "inf.br", "info", "info.au", "info.tr", "it", "idv.tw", "int", "is", "il", "jl.cn", "jor.br", "jp", "js.cn", "jx.cn", "k12.tr", "ke", "kr", "la", "lel.br", "li", "lk", "ln.cn", "lt", "lu", "lv", "ltd.uk", "ma", "mat.br", "mc", "md", "me.uk", "med.br", "mil", "mil.br", "mil.tr", "mk", "mn", "mo.cn", "ms", "mt", "mus.br", "mx", "name", "name.tr", "ne.jp", "net", "net.au", "net.br", "net.cn", "net.eg", "net.hk", "net.in", "net.lu", "net.mx", "net.uk", "net.ru", "net.tr", "net.tw", "nl", "nm.cn", "no", "no.com", "nom.br", "not.br", "ntr.br", "nu", "nx.cn", "nz", "plc.uk", "odo.br", "oop.br", "or.jp", "or.at", "org", "org.au", "org.br", "org.cn", "org.hk", "org.in", "org.lu", "org.ru", "org.tr", "org.tw", "org.uk", "pk", "pl", "pol.tr", "pp.ru", "ppg.br", "pro.br", "psi.br", "psc.br", "pt", "qh.cn", "qsl.br", "rec.br", "ro", "ru", "sc.cn", "sd.cn", "se", "se.com", "se.net", "sg", "sh", "sh.cn", "si", "sk", "slg.br", "sm", "sn.cn", "srv.br", "st", "su", "sx.cn", "tc", "tel.tr", "th", "tj.cn", "tm", "tn", "tmp.br", "to", "trd.br", "tur.br", "tv", "tv.br", "tw", "tw.cn", "ua", "uk", "uk.com", "uk.net", "us", "va", "vet.br", "vg", "id.au", "web.tr", "ws", "xj.cn", "xz.cn", "yn.cn", "yu", "za", "zlg.br", "zj.cn");
	
		if (in_array($matches[3] . $matches[4], $complex_tlds))
			$parsed['domain'] = strtolower($matches[2] . $matches[3] . $matches[4]);
		else
			$parsed['domain'] = strtolower($matches[3] . $matches[4]);
		
		$parsed['full_domain'] = strtolower($matches[1] . $matches[3] . $matches[4]);
		$parsed['url'] = $parsed['full_domain'] . @$matches[5];
	//print_r($matches);
	}
	elseif (preg_match("/^(?:http[s]?:\\/\\/)?(?:.*@)?((?:(?:[^\\?\/\\.]*\\.)*)?([^\\?\/\\.]*\\.?))(\\.[a-zA-Z0-9]{2,5})(?::[^\/\\?]*)?([\\/|\\?][^\\s\"']*)?.*/i",
			$str, $matches))
	{
		$parsed['domain'] = strtolower($matches[2] . $matches[3]);
		$parsed['full_domain'] = strtolower($matches[1] . $matches[3]);	
		$parsed['url'] = $parsed['full_domain'] . @$matches[4];
	}
	elseif (preg_match("/^(?:http[s]?:\\/\\/)?(?:.*@)?((?:(?:[^\\?\/\\.]*\\.)*)?([^\\?\/\\.]*\\.))(..)(?:\:[^\/\\?]*)?([\/|\\?][^\\s\"']*)?.*/i",
			$str, $matches))
		{
		//	return $matches;	
			$parsed = array("domain" => strtolower($matches[2] . $matches[3]), "full_domain" => strtolower($matches[1] . $matches[3]), "url" => $matches[1] . $matches[3] . @$matches[4]);
		}
		else
			return false;
	
	return $parsed;
}
	
	function remove_entities ($str)
	{
	//	if (function_exists('html_entity_decode'))
	//		return html_entity_decode($str, ENT_COMPAT, "UTF-8");
		
	//	$trans_tbl = get_html_translation_table(HTML_ENTITIES);
	//	$trans_tbl = array_flip($trans_tbl);
	//	return strtr($str, $trans_tbl);
		if (function_exists('utf8_encode'))
			$str = utf8_encode($str);
		return preg_replace( '/&#(\\d+);/e', '$this->code2utf($1)', $str);
	}

	 function code2utf($num)
	 {
		 if ($num < 128) 
		 {
		 return chr($num);
		 }
		 if ($num < 2048) 
		 {
		 return chr(($num >> 6) + 192) . chr(($num & 63) + 128);
		 }
		 if ($num < 65536) 
		 {
			 return chr(($num >> 12) + 224) . chr((($num >> 6) & 63) + 128) . chr(($num & 63) + 128);
		 }
		 if ($num < 2097152) 
		 {
		 	return chr(($num >> 18) + 240) . chr((($num >> 12) & 63) + 128) . chr((($num >> 6) & 63) + 128) . chr(($num & 63) + 128);
		 }
		 return '';
	 }
}

?>