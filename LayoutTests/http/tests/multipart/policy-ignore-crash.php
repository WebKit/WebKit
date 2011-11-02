<?php
header('Content-type: multipart/x-mixed-replace;boundary=asdf');
?>--asdf
Content-type: text/html

<p>This test passes if it does not crash.</p>
<script>
if (window.layoutTestController)
    layoutTestController.dumpAsText();
</script>

<?php
# Add some padding because CFNetwork merges small multipart segments together.
echo str_pad('', 5000);

ob_flush();
flush();
?>
--asdf
Content-type: text/random

This chunk doesn't have a content type, which can cause the policy
for this load to be ignored. This causes the request to be canceled.

<?php
# Add some padding because CFNetwork merges small multipart segments together.
echo str_pad('', 5000);
?>

--asdf--
