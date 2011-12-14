<?php
    header('Content-type: text/html; charset="koi8-r"');
?>

<p>Should say SUCCESS: SUÛÛÂSS</p>
<p>The latter has some Cyrillic characters that look like Latin ones. This test verifies that decoding
is correctly performed.</p>

<script>
if (window.layoutTestController)
    layoutTestController.dumpAsText();
</script>
