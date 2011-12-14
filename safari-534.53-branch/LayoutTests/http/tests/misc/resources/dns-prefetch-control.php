<?php
  if ($_GET['value'] == 'off')
    header('X-DNS-Prefetch-Control: off');
  else if ($_GET['value'] == 'on')
    header('X-DNS-Prefetch-Control: on');
  else if ($_GET['value'] == 'foo')
    header('X-DNS-Prefetch-Control: foo');

$fake_domain = "www." . rand() . ".invalid";
?>
<html>
<body>
<a href='http://<?php echo $fake_domain ?>/'><?php echo $fake_domain ?></a>
</body>
</html>
