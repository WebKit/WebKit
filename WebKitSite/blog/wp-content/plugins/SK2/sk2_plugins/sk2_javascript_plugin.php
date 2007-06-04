<?php
// Javascript Plugin
// Uses JS to test browser

class sk2_javascript_plugin extends sk2_plugin
{
	var $name = "Javascript Payload";
	var $author = "";
	var $plugin_help_url = "http://wp-plugins.net/wiki/?title=SK2_Javascript_Plugin";
	var $description = "Embed a few Javascript commands in comment form (most browsers withtout Javascript abilities are usually spambots). If the browser does not support Javascript, it only receives a small penalty.";
	var $filter = true;
	var $skip_under = -20;
	var $skip_above = 10;
	var $settings_format = array ("no-penalty" => array("advanced" => true, "type" => "checkbox", "value" => false, "caption" => "Do not hit browsers with no Javascript support (only positive karma for JS-enabled browsers)."));
	
	function form_insert($post_ID)
	{
		$seed = $this->get_option_value('secret_seed');
		if (empty ($seed))
		{
			$seed = sk2_rand_str(10);
			$this->set_option_value('secret_seed', $seed);
			$this->log_msg(__("Resetting secret Javascript seed to: ", 'sk2') . $seed, 5);
		}
		
		$max = rand(5, 9);
		$tot = $str = 1;
		
		for ($i = 0; $i < $max; $i++)
		{
			$op = rand(0, 8);
			$num = rand(1, 42);

			switch ($op)
			{
				case 0:
				case 8:
					$str = "(" . $str . " + " . $num . ")";
					$tot += $num;
				break;
				case 1:
					$str = "(" . $str . " - " . $num . ")";
					$tot -= $num;
				break;
				case 2:
					$str = "(" . $str . " * " . $num . ")";
					$tot *= $num;
				break;
				case 3:
					$str = "Math.round ( Math.abs(" . $str . " / " . $num . "))";
					$tot = round(abs($tot / $num));
				break;
				case 4:
					$str = "Math.min(" . $str . ", " . $num . ")";
					$tot = min($tot, $num);
				break;
				case 5:
					$str = "Math.max(" . $str . ", " . $num . ")";
					$tot = max($tot, $num);
				break;
				case 6:
					$str = "Math.round ( Math.abs(" . $str . " % " . $num . "))";
					$tot = round(abs($tot % $num));
				break;
				case 7:
					$str = "(" . $str . " + Math.round( Math.abs(100*Math.sin(" . $num . ")) ) )";
					$tot = $tot + round(abs(100*sin($num)));
				break;
			}
		}
		
		$js_command = "Math.round ( Math.abs(" . $str . "))" ;
		$tot = round(abs($tot));
		
		$check1 = sk2_rand_str(10);
		$check2 = md5($tot . $check1 . $seed);

?>
<input type="hidden" id="sk2_my_js_check1" name="sk2_my_js_check1" value="<?php echo $check1; ?>" />
<input type="hidden" id="sk2_my_js_check2" name="sk2_my_js_check2" value="<?php echo $check2; ?>" />
<script type="text/javascript">
<!--
	document.write('<input type="hidden" id="sk2_my_js_payload" name="sk2_my_js_payload" value="');
	document.write(<?php echo $js_command; ?>);
	document.write('" />');
-->
</script>
<?php
		//echo ("<!--#". $time . "#". $seed . "#". $ip ."#". $post_ID . "#-->");
	}

	function version_update($cur_version)
	{
		$seed = sk2_rand_str(10);
		$this->set_option_value('secret_seed', $seed);
		$this->log_msg(__("Resetting secret Javascript seed to: ", 'sk2') . $seed, 5);
		return true;
	}

	function filter_this(&$cmt_object)
	{
		$karma_diff = 0;
		if ($cmt_object->is_post_proc())
		{
			$log = __("Cannot check Javascript payload in post_proc mode.");
			$this->log_msg($log, 4);
			return;
		}	

		if (! $cmt_object->is_comment())
			return;
			
		if (empty($_REQUEST['sk2_my_js_payload']) || empty($_REQUEST['sk2_my_js_check1']))
		{
			if ($this->get_option_value("no-penalty"))
			{
				$this->log_msg(__("Browser doesn't support Javascript. Penalty disabled"), 4);
			}
			else
			{
				$log = __("Browser doesn't support Javascript");
				$karma_diff = -2;				
			}
		}
		else
		{
			$seed = $this->get_option_value('secret_seed');
		
			if ($_REQUEST['sk2_my_js_check2'] != md5($_REQUEST['sk2_my_js_payload'] . $_REQUEST['sk2_my_js_check1'] . $seed))
			{
				$log = __("Fake Javascript Payload.");
				$karma_diff = -10;
				$this->log_msg($log, 6);
			}
			else
			{
				$log = __("Valid Javascript payload (can be fake).");
				$karma_diff = 0.5;
			}
		}
		if ($karma_diff)
			$this->modify_karma($cmt_object, $karma_diff, $log);
	}
}

$this->register_plugin("sk2_javascript_plugin", 2);

?>