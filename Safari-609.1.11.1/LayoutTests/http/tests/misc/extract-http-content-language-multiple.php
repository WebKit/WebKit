<?php
  header("Content-Language:  fr \t , fi ");
?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<script src="../../js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=97929">bug 97929</a>:
Extract HTTP Content-Language header.</p>
<div id="console"></div>
<div id="x"></div>
<div id="y" lang="ar"></div>
<script>
  debug('==> Value extracted singly from HTTP "Content-Language" header...');
  shouldBe('window.getComputedStyle(document.getElementById("x")).webkitLocale', '"fr"')
  debug('==> Value set by div "lang" tag...');
  shouldBe('window.getComputedStyle(document.getElementById("y")).webkitLocale', '"ar"')
  debug('==> All done...');
</script>
<script src="../../js-test-resources/js-test-post.js"></script>
</body>
</html>
