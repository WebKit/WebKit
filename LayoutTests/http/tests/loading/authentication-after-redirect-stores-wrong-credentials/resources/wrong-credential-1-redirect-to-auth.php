<?php
// This page was supposed to be loaded using a localhost URL.
// That is important, and the next page has to be loaded using 127.0.0.1.
header("Location: http://127.0.0.1:8000/loading/authentication-after-redirect-stores-wrong-credentials/resources/wrong-credential-2-auth-then-redirect-to-finish.php");
exit;
?>