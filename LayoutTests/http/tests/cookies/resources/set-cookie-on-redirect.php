<?php
if ($_GET['step'] == 1) {
  header("HTTP/1.0 302 Found");
  header('Location: http://localhost:8000/cookies/resources/set-cookie-on-redirect.php?step=2');
} else if ($_GET['step'] == 2) {
  header("HTTP/1.0 302 Found");
  setcookie("test_cookie", "1");
  header('Location: http://localhost:8000/cookies/resources/set-cookie-on-redirect.php?step=3');
} else if ($_GET['step'] == 3) {
  header("HTTP/1.0 200 OK");
  if (isset($_COOKIE['test_cookie'])) {
    /* Clear cookie in order not to affect other tests. */
    setcookie("test_cookie", "", time() - 1);

    echo "PASSED: Cookie successfully set\n";
  } else {
    echo "FAILED: Cookie not set\n";
  }
  echo "<script> if (window.testRunner) testRunner.notifyDone();</script>\n";
}
?>
