<?php
  // Test for various syntaxes of Refresh header, 
  // <http://bugzilla.opendarwin.org/show_bug.cgi?id=9854>.

  header('Content-type: text/html');
  header('Refresh: 0, URL 	= 	"resources/redirect-step2.php"');
?>

<body>
<script>
   if (window.layoutTestController) {
       layoutTestController.waitUntilDone();
       layoutTestController.dumpAsText();
   }
</script>
   
<p>FAILURE - should redirect (1)<p>
</body>
