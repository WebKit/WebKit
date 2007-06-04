<?php
// Captcha Backup
// Displays a captcha as a backup solution for borderline karma


class sk2_captcha_plugin extends sk2_plugin
{
	var $name = "Captcha Check";
	var $description = "If (and only if) the comment's karma is within a certain error margin, provide the commenter with a chance to clear himself by solving a Captcha.";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/index.php/?title=SK2_Captcha_Plugin";
	var $treatment = true;

	var $weight_levels = array("0" => "Disabled", "0.5" => "Easy", "1.0" => "Enabled", "2.0" => "Difficult");

	var $settings_format = array ("threshold" => array("advanced" => true, "type" => "text", "value"=> -5, "caption" => "Give the Captcha backup for karma over ", "size" => 3, "after" => " <i>(Note: this value will only be used if the karma doesn't allow the comment to be displayed directly to begin with. See below)</i>."), 
												"expiration" => array("advanced" => true, "type" => "text", "value"=> 3, "caption" => "Expire after ", "size" => 3, "after" => " hours."));
	
	function treat_this(&$cmt_object)
	{
		if ($cmt_object->can_unlock())
			foreach($cmt_object->unlock_keys as $key => $this_key)
				if ($this_key['class'] == get_class($this))
					unset ($cmt_object->unlock_keys[$key]);

		if (!$cmt_object->is_comment() || $cmt_object->is_post_proc())
			return;
			
		if ( ($cmt_object->karma >= 0) || ($cmt_object->karma < $this->get_option_value("threshold")))
			return;
		
		$expiration = max(500, 3600 * $this->get_option_value("expiration"));
		$cmt_object->add_unlock_key(sk2_rand_str(ceil(4 * $this->get_option_value("weight")), true), get_class($this), time() + $expiration);
		$this->log_msg(sprintf(__("Set Captcha unlock key, will expire in %d seconds.", 'sk2'), $expiration) , 3);
	
	}

	function display_second_chance(&$cmt_object, $unlock_key)
	{
		echo "<p><h2>" . __("Kind-a-Captcha", 'sk2') . "</h2></p>";
		echo "<p>" . __("Please type the code below in the input field and click on Submit (characters can only be letters from A to F and digits from 0 to 9).", 'sk2') . "</p>";
		echo "<img src=\"sk2_captcha_graphic.php?c_id=". $cmt_object->ID . "&c_author=". urlencode($cmt_object->author_email) . "\" alt=\"captcha_img\"/>";
				?>
		<p><input type="text" name="captcha_code" id="captcha_code" size="6"></p>
		<input type="submit" name="submit_captcha" id="submit_captcha" value="Submit">
		<?php
	}

	function treat_second_chance(&$cmt_object, $unlock_key)
	{
		if (strtoupper($unlock_key) != strtoupper($_REQUEST['captcha_code']))
			return false;
		else
		{
			$this->raise_karma($cmt_object, 5, "Successfully filled captcha.");
			return true;
		}
	}

	function output_plugin_UI()
	{
		echo "<dl>";
		parent::output_plugin_UI(false); // call default constructor
		if(! function_exists("imagecreate") || ! function_exists('imagepng') )
			echo "<dt><strong><p style=\"color:red;\">". __("Your install of PHP seems to be missing the GD library (or a more recent version is needed). You should ask your host to update the GD module for PHP and disable the Captcha module until then.", 'sk2') . "</p></strong></dt>";
		echo "</dl>";
	}

		
}

$this->register_plugin("sk2_captcha_plugin", 2); 

?>