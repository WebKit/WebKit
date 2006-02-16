<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
	<meta http-equiv="Content-Type" content="text/html;charset=utf-8" />

	<title>The WebKit Open Source Project<?php if (isset($title)) { echo " - " . $title; } ?></title>

	<link rel="stylesheet" type="text/css" href="/css/main.css" />
	<link rel="stylesheet" type="text/css" href="/css/green.css" title="green" />

<?php
    $msie = '/msie\s(5\.[5-9]|[6]\.[0-9]*).*(win)/i';
    $agent = $_SERVER['HTTP_USER_AGENT'];
    if( isset($agent) && preg_match($msie, $agent) && ! preg_match('/opera/i', $agent)) {
?>
	<link rel="stylesheet" type="text/css" href="/css/ie.css" />
<?php } else { ?>
	<link rel="alternate stylesheet" type="text/css" href="/css/blue.css" title="blue" />
	<link rel="alternate stylesheet" type="text/css" href="/css/yellow.css" title="yellow" />
	<link rel="alternate stylesheet" type="text/css" href="/css/pink.css" title="pink" />
	<link rel="alternate stylesheet" type="text/css" href="/css/purple.css" title="purple" />
	<link rel="alternate stylesheet" type="text/css" href="/css/gray.css" title="gray" />
<?php } ?>

	<script type="text/javascript">
		pic1 = new Image(8,9);
		pic1.src="/images/green-bullet.png";
		pic2 = new Image(8,9);
		pic2.src="/images/blue-bullet.png";
		pic3 = new Image(8,9);
		pic3.src="/images/yellow-bullet.png";
		pic4 = new Image(8,9);
		pic4.src="/images/pink-bullet.png";
		pic5 = new Image(8,9);
		pic5.src="/images/purple-bullet.png";
	</script>
	
<?php
    if (isset($extra_head_content)) {
        echo $extra_head_content;
    }
?>
</head>
<body>

<div id="title">
<h1>The WebKit Open Source Project</h1>
</div>

<div id="icon"></div>

<div id="content">