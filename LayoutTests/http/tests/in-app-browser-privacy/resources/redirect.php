<?php
if ($_GET['step'] == 1) {
  header("HTTP/1.0 302 Found");
  header('Location: http://localhost:8000/in-app-browser-privacy/resources/redirect.php?step=2');
} else if ($_GET['step'] == 2) {
    header("HTTP/1.0 302 Found");
    header('Location: http://localhost:8000/in-app-browser-privacy/resources/redirect.php?step=3');
} else if ($_GET['step'] == 3) {
  header("HTTP/1.0 200 OK");
  echo "FAILED: Should not have loaded\n";
  echo "<script> if (window.testRunner) testRunner.notifyDone();</script>\n";
}
?>
