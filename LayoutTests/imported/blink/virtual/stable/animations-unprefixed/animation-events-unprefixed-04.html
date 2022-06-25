<!DOCTYPE html>
<html>
<head>
  <title>Tests that custom events with unprefixed animations names are correctly dispatched.</title>
  <script>
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    document.addEventListener('animationstart', function(e) {
      document.getElementById('result').innerHTML += 'PASS: animationstart event listener has been called.<br>';
    }, false);

    document.addEventListener('webkitAnimationStart', function(e) {
      document.getElementById('result').innerHTML += 'FAIL: webkitAnimationStart event listener should not have been called.<br>';
    }, false);

    document.addEventListener('animationiteration', function(e) {
      document.getElementById('result').innerHTML += 'PASS: animationiteration event listener has been called.<br>';
    }, false);

    document.addEventListener('webkitAnimationIteration', function(e) {
      document.getElementById('result').innerHTML += 'FAIL: webkitAnimationIteration event listener should not have been called.<br>';
    }, false);

    document.addEventListener('animationend', function(e) {
      document.getElementById('result').innerHTML += 'PASS: animationend event listener has been called.';
      if (window.testRunner)
        testRunner.notifyDone();
    }, false);

    document.addEventListener('webkitAnimationEnd', function(e) {
      document.getElementById('result').innerHTML += 'FAIL: webkitAnimationEnd event listener should not have been called.';
      if (window.testRunner)
        testRunner.notifyDone();
    }, false);

  </script>
</head>
<body>
Tests that custom events with unprefixed animations names are correctly dispatched.
<pre id="result"></pre>
</body>
<script>
  var custom = document.createEvent('CustomEvent');
  custom.initCustomEvent('animationstart', true, true, null);
  document.dispatchEvent(custom);
  custom = document.createEvent('CustomEvent');
  custom.initCustomEvent('animationiteration', true, true, null);
  document.dispatchEvent(custom);
  custom = document.createEvent('CustomEvent');
  custom.initCustomEvent('animationend', true, true, null);
  document.dispatchEvent(custom);
</script>
</html>
