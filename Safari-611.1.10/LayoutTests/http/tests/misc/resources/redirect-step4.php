<?php
  // Test for various syntaxes of Refresh header, 
  // <http://bugzilla.opendarwin.org/show_bug.cgi?id=9854>.

  header('Content-type: text/html');
?>
<head>
  <meta http-equiv="Refresh" content='	0	 ;	200.html	'>
</head>

<p>FAILURE - should redirect (4)<p>
