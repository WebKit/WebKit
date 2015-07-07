<?php
  if (!isset($_REQUEST['uid'])) {
    // Step 1 - navigate to a page that will make us remember credentials.
    echo "<script>\n";
    echo "if (!window.testRunner) {\n";
    echo "    document.write('This test only works as an automated one');\n";
    echo "    throw 0;\n";
    echo "}\n";
    echo "testRunner.waitUntilDone();\n";
    echo "testRunner.dumpAsText();\n";
    echo "testRunner.setHandlesAuthenticationChallenges(true)\n";
    echo "testRunner.setAuthenticationUsername('username')\n";
    echo "testRunner.setAuthenticationPassword('password')\n";
    echo "location = 'http://127.0.0.1:8000/security/401-logout/401-logout.php?uid=username';\n";
    echo "</script>\n";
  } else if (!isset($_SERVER['PHP_AUTH_USER']) || ($_REQUEST['uid'] != $_SERVER['PHP_AUTH_USER'])) {
    if (isset($_REQUEST['laststep'])) {
      // Step 4 - Credentials are no longer being sent
      echo "PASS";
      echo "<script>\n";
      echo "if (window.testRunner) {\n";
      echo "    testRunner.notifyDone();\n";
      echo "}\n";
      echo "</script>\n";
    } else {
      // Ask for credentials if there are none
      header('WWW-Authenticate: Basic realm="401-logout"');
      header('HTTP/1.0 401 Unauthorized');
    }
  } else {
    if (!isset($_REQUEST['logout'])) {
      // Step 2 - navigate to a page that will make us forget the credentials
      echo "<script>\n";
      echo "testRunner.setHandlesAuthenticationChallenges(false)\n";
      echo "location = 'http://127.0.0.1:8000/security/401-logout/401-logout.php?uid=username&logout=1';\n";
      echo "</script>\n";
    } else {
      // Step 3 - logout
      header('WWW-Authenticate: Basic realm="401-logout"');
      header('HTTP/1.0 401 Unauthorized');
      echo "<script>\n";
      echo "location = 'http://127.0.0.1:8000/security/401-logout/401-logout.php?uid=username&laststep=1';\n";
      echo "</script>\n";
    }
  }
?>
