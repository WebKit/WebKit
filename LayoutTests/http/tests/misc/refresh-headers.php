<?php
  $gotMaxAge=false;
  $gotNoCache=false;
  $headers = getallheaders(); 

  foreach ($headers as $name => $content) {
    if (0 == strcasecmp($name, "Cache-Control") && 0 == strcasecmp($content, "max-age=0"))
    {
      $gotMaxAge = true;
    }
    
    if ((0 == strcasecmp($name, "Cache-Control") && 0 == strcasecmp($content, "no-cache")) ||
        (0 == strcasecmp($name, "Pragma") && 0 == strcasecmp($content, "no-cache")))
    {
      $gotNoCache = true;
    }
  }
  
  if ($gotNoCache) {
    echo '<p>Got a no-cache directive; FAILURE!</p>';
    echo '<script>if (window.layoutTestController) { layoutTestController.notifyDone(); }</script>';
  } else if ($gotMaxAge) {
    echo '<p>SUCCESS</p>';
    echo '<script>if (window.layoutTestController) { layoutTestController.notifyDone(); }</script>';
  } else {
    echo '<body onload="window.location.reload();">';
    echo '<p>No cache control headers, reloading...</p>';
    echo '<script>if (window.layoutTestController) { layoutTestController.waitUntilDone(); }</script>';
    echo '<script>function test() {window.location.reload();}</script>';
  }

  echo '<script>if (window.layoutTestController) { layoutTestController.dumpAsText(); }</script>';
  echo '<p>Test for <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5499">bug 5499</a>: Page reload does not send any cache control headers.</p>';
?>
