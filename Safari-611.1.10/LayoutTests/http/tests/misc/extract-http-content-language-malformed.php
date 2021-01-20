<?php
  header("Content-Language: ,es");
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
  debug('==> Value extracted from malformed HTTP "Content-Language" header...');
  // A malformed header may be fixed by the browser ("es") or left bad and so remain unset ("auto").
  // Chrome does the former; Safari does the latter.
  shouldBeTrue('window.getComputedStyle(document.getElementById("x")).webkitLocale == "es" || window.getComputedStyle(document.getElementById("x")).webkitLocale == "auto"')
  debug('==> Value set by div "lang" tag...');
  shouldBe('window.getComputedStyle(document.getElementById("y")).webkitLocale', '"ar"')
  debug('==> All done...');
</script>
<script src="../../js-test-resources/js-test-post.js"></script>
</body>
</html>
