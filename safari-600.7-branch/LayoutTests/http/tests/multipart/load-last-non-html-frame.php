<?php
header('Content-type: multipart/x-mixed-replace;boundary=asdf');
?>--asdf
Content-type: text/plain

This test passes if the last multipart frame is displayed.
FAIL

<?php
# Add some padding because CFNetwork merges small multipart segments together.
echo str_pad('', 5000);
?>
--asdf
Content-type: text/plain

This test passes if the last multipart frame is displayed.
PASS
<?php
# Add some padding because CFNetwork merges small multipart segments together.
echo str_pad('', 5000);
?>--asdf--
