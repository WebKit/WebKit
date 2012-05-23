<?php
    setcookie("httpOnlyCookie", "value", time()+36000000, "", "", 0, 1);
?>
<!DCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<link rel="stylesheet" href="resources/cookies-test-style.css">
<script src="resources/cookies-test-pre.js"></script>
</head>
<body>
<p id="description"></p>
<div id="console"></div>
<script src="script-tests/js-get-and-set-http-only-cookie.js"></script>
<script src="resources/cookies-test-post.js"></script>
</body>
</html>
