<?php
header('Content-type: multipart/x-mixed-replace;boundary=asdf');
?>--asdf
Content-type: text/html

<p>This test passes if it does not crash.</p>
<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>

<?php
# Add some padding because CFNetwork merges small multipart segments together.
echo str_pad('', 5000);
echo "\r\n";

ob_flush();
flush();
?>--asdf
Content-type: text/rtf

This chunk has an unsupported text mime type, which can cause the policy
for this load to be ignored. This causes the request to be canceled.

<?php
# Add some padding because CFNetwork merges small multipart segments together.
echo str_pad('', 5000);
echo "\r\n";
?>--asdf--
