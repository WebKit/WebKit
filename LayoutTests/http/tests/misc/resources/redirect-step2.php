<?php
  // Test for various syntaxes of Refresh header, 
  // <http://bugzilla.opendarwin.org/show_bug.cgi?id=9854>.

  header('Content-type: text/html');
  header('Refresh: 	0	;	url= 	\'redirect-step3.php\' 	');
?>

<p>FAILURE - should redirect (2)<p>
