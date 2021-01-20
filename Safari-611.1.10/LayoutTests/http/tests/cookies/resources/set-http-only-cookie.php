<?php
if ($_GET['cookieName']) {
  setcookie($_GET['cookieName'], "1", time() + 86400, "/", "", FALSE, TRUE);
  echo "Yeah";
} else {
  header("HTTP/1.0 200 OK");
  echo "FAILED: No cookieName parameter found.\n";
}

?>
