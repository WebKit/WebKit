<?php
if ($_GET['redirectBackTo']) {
  header("HTTP/1.0 302 Found");
  setcookie("test_cookie", "1", time() + 86400);
  header('Location: ' . $_GET['redirectBackTo']);
} else {
  header("HTTP/1.0 200 OK");
  echo "FAILED: No redirectBackTo parameter found.\n";
}
?>
