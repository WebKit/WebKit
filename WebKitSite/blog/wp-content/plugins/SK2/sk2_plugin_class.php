<?php

class sk2_plugin
{
// public:
	var $name;
	var $author = "unknown";
	var $description;
	var $author_url;
	var $author_email;
	var $plugin_help_url;
	var $settings_format;
	var $filter = false;
	var $treatment = false;
	var $version = 1;
	var $release_string = "";
	var $skip_under = -15;
	var $skip_above = 10;
	var $show_version = false;
	
	var $no_UI = false;
	
// private
	var $settings = 0;
	var $weight_levels = array("0.25" => "Weak", "0.75" => "Moderate", "1.0" => "Normal", "1.25" => "Strong", "2" => "Supastrong");
	
// abstract (override as needed):

	function sk2_plugin()
	{
		
		if (empty($this->name))
			$this->name = get_class($this);
			
		if (!$this->is_filter() && !$this->is_treatment())
			$this->log_msg (sprintf(__("Plugin %s is registered with both <code>filter</code> and <code>treatment</code> set to false. It probably won't do anything: please contact plugin author.", 'sk2'), "<i>" . $this->name . "</i>" . (empty($this->author_email) ? "." : (": <a href=\"mailto:" . $this->author_email . "\">" . $this->author_email ."</a>."))) , 5);

		if (! isset($this->settings_format["weight"]))
			$this->settings_format["weight"] = array("value" => "1.0");
		
		global $sk2_settings;
		$settings = $sk2_settings->get_plugin_settings(get_class($this));
	}
	
	function __construct()
	{
		if (! defined("__CLASS__"))
			$this->sk2_plugin();
		else
			$this->{__CLASS__}();
	}
	
	function filter_this(&$cmt_object)
	{ // override this to do your own filtering
		log_msg (__("Default filter (no action) called for plugin: ", 'sk2') . $name, 3, $cmt_object->ID);
	}
	
	function treat_this(&$cmt_object)
	{ // override this to do your own treatment
		log_msg (__("Default treatment (no action) called for plugin: ", 'sk2') . $name, 3, $cmt_object->ID);
	}

// public

	function log_msg($msg, $level = 0, $cmt_id = 0)
	{
		global $sk2_log;
		$sk2_log->log_msg($msg, $level, $cmt_id, get_class($this));
	}
	
	function log_msg_mysql($msg, $level = 0, $cmt_id = 0)
	{
		global $sk2_log;
		$sk2_log->log_msg_mysql($msg, $level, $cmt_id, get_class($this));
	}
	
	function hit_karma(&$cmt_object, $karma_diff, $reason = "")
	{
		$cmt_object->modify_karma(- $this->get_option_value('weight') * $karma_diff, $this->name, $reason);
	}
	
	function raise_karma(&$cmt_object, $karma_diff, $reason = "")
	{
		$cmt_object->modify_karma($this->get_option_value('weight') * $karma_diff, $this->name, $reason);
	}

	function modify_karma(&$cmt_object, $karma_diff, $reason = "")
	{
		$cmt_object->modify_karma($this->get_option_value('weight') * $karma_diff, $this->name, $reason);
	}
	
	function is_enabled()
	{
		return ($this->get_option_value('weight') > 0);
	}
	
	function form_insert($post_ID)
	{
		// do nothing: only there as a placeholder for plugins
		return true;
	}
	
// 	private
	
	function output_plugin_UI ($output_dls = true) // you can override this function if you *really* know what you are doing
	{
		if ($this->no_UI)
			return;
			
		if ($output_dls)
			echo "<dl>";
		if (! isset($this->weight_levels["0.0"]) && ! isset($this->weight_levels["0"]))
			$this->weight_levels["0"] = __("Disabled", 'sk2');
		ksort($this->weight_levels);	

		echo " <strong>" . __($this->name, 'sk2') . "</strong> - " . __("Strength: ", 'sk2');
		$this->output_UI_menu("weight", $this->weight_levels, $this->get_option_value('weight'));

		if (! empty($this->author))
		{
			echo " - " . __("Author: ", 'sk2');
			if (empty($this->author_url))
				echo $this->author;
			else
				echo "<a href=\"$this->author_url\" title=\"". __("Visit author's webpage", 'sk2') . "\">" . $this->author . "</a>";
		}

		if ($this->show_version && !empty($this->plugin_version))
			echo " - Version: " . $this->plugin_version . $this->release_string;
		
		if (! empty($this->author_email))
			echo " <a href=\"mailto:$this->author_email\">[@]</a>";
		
		if (! empty($this->plugin_help_url))
			echo " <a href=\"$this->plugin_help_url\" target=\"sk2_help\">[?]</a>";

		if (! empty($this->description))
			echo "<dd>" . __($this->description, 'sk2') . "</dd>\n";
			
		if (count($this->settings_format) > 1) // not counting weight in there
		{
			echo "<dd class=\"sk2_filter_option\"><dl>\n";
			foreach($this->settings_format as $name => $format)
			{
				if ($name == "weight")
					continue;
					
				echo "<dt";
				if (@$format['advanced'])
					 echo " class=\"advanced\"";
				echo ">\n";
				if ($format['type'] == "check" || $format['type'] == "checkbox")
				{
					$this->output_UI_input($name, "checkbox", $this->get_option_value($name));
					{
						echo "<label for=\"sk2_filter_options[". get_class($this) . "][$name]\">";
						echo " " . __($format['caption'], 'sk2');
						echo "</label>";
					}
				}
				else
				{
					if (! empty($format['caption']))
						echo __($format['caption'], 'sk2') . " ";
					if ($format['type'] == "menu" || $format['type'] == "select")
						$this->output_UI_menu($name, $format['options'], $this->get_option_value($name));
					else 
						$this->output_UI_input($name, "text", $this->get_option_value($name), @$format['size']);
					
					if (! empty($format['after']))
						echo " " . __($format['after'], 'sk2');
				}
				
				echo "</dt>\n";
			}
			echo "</dl></dd>\n";
		}

		if ($output_dls)
			echo "</dl>";
	}
	
	function output_UI_menu($name, $choices, $select_val, $component=0)
	{
		if (! $component)
			$component = get_class($this);

		echo "<select name=\"sk2_filter_options[". $component . "][$name]\" id=\"sk2_filter_options[". $component . "][$name]\">\n";
		foreach($choices as $value => $label)
		{
			$value = addslashes($value);
			if ($select_val == $value)
				echo "<option value=\"$value\" selected>";
			else
				echo "<option value=\"$value\">";
			 echo __($label, 'sk2') . "</option>";
		}
		echo "</select>\n";
	}
	
	function output_UI_input($name, $type, $value, $size = 16, $component = 0)
	{
		if (! $component)
			$component = get_class($this);
		if (! $size)
			$size = 16;

		echo "<input type=\"$type\" name=\"sk2_filter_options[". get_class($this) . "][$name]\" id=\"sk2_filter_options[". get_class($this) . "][$name]\" ";
		if (($type == "checkbox" ) && $value)
		{
			$value = "1";
			echo "checked";
		}
		elseif ($type == "text")
			echo "size=\"$size\"";
		if ($type != "checkbox")
			echo " value=\"" . str_replace("\"", "&#34;", $value) . "\"";
		echo " />";
		if (($type == "checkbox" ) && $value)
		{
			echo "<input type=\"hidden\" name=\"sk2_filter_checkboxes[". get_class($this) . "]\" name=\"sk2_filter_checkboxes[". get_class($this) . "]\" value=\"$name\" />\n";
		}
	}
	
	function update_SQL_schema($cur_version)
	{
		// plugin can perform their SQL updates here
		return true;
	}

	function version_update($cur_version)
	{
		// plugin can perform one-time initializations here
		return true;
	}
	
	function get_option_value($name)
	{
		global $sk2_settings;
		$settings = $sk2_settings->get_plugin_settings(get_class($this));

		if (isset($settings[$name]))
			return $settings[$name];
		else
			return @$this->settings_format[$name]['value'];
	}
	
	function set_option_value($name, $value)
	{
		global $sk2_settings;
		$settings = $sk2_settings->get_plugin_settings(get_class($this));
		$settings[$name] = $value;
		$sk2_settings->set_plugin_settings($settings, get_class($this));
	}
	
	function display_second_chance(&$cmt_object, $unlock_key)
	{
		log_msg (__("Default Second Chance function (empty) called for plugin: ", 'sk2') . $this->name, 7, $comment_ID);
		
		echo __("This is an empty placeholder...", 'sk2');
	}
	
	function treat_second_chance(&$cmt_object, $unlock_key)
	{
		return false;	
	}

	
	function is_treatment()
	{ return $this->treatment; }

	function is_filter()
	{ return $this->filter; }
	
}

?>