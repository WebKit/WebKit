<p>Test that null values in XHR login/password parameters are treated correctly.</p>
<p>No auth tokens should be sent with this request.</p>
<pre id='syncResult'> </pre>
<script>
if (window.testRunner)
  testRunner.dumpAsText();

req = new XMLHttpRequest;
req.open('POST', '<?php echo 'http://foo:bar@' . $_SERVER['HTTP_HOST'] . '/xmlhttprequest/resources/echo-auth.php' ?>', false, null, null);
req.send();
document.getElementById('syncResult').firstChild.nodeValue = req.responseText;
</script>
