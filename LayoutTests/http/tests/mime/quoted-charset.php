<?php
    header('Content-type: text/html; charset="koi8-r"');
?>

<p>Should say SUCCESS: SUóóåSS</p>
<p>The latter has some Cyrillic characters that look like Latin ones. This test verifies that decoding
is correctly performed.</p>

<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>
