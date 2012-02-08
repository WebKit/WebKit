<?php
if (!isset($_SERVER['PHP_AUTH_USER'])) {
    header('WWW-Authenticate: Basic');
    header('HTTP/1.0 401 Unauthorized');
    exit;
}
?>
<script>
// This page was supposed to be loaded using a 127.0.0.1 URL.
// That is important, and the final page has to be loaded using localhost.
// Plus, the redirect to the final page in this test has to be a new page load to trigger the bug; It cannot be an HTTP redirect.
window.setTimeout("window.location = 'http://localhost:8000/loading/authentication-after-redirect-stores-wrong-credentials/resources/wrong-credential-3-output-credentials-then-finish.php';", 0);
</script>
