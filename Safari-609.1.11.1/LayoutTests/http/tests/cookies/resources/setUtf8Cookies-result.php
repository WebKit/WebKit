<?php header('Content-Type: text/html; charset=UTF-8'); ?>
<html>
<body>
<p>
<?php
    echo "<p>some utf8 characters: UTF-8 æøå 中国</p>";
    echo "<p>php_cookie: ".$_COOKIE["php_cookie"]."</p>";
?>
<script>
    if (window.testRunner)
        testRunner.dumpAsText();
    document.cookie="js_cookie=UTF-8 æøå 中国; expires=Monday, 04-Apr-2020 05:00:00 GMT";
    document.write("cookies read through js: "+document.cookie);
</script>
</body>
</html>
