<?php
header('Cache-Control: max-age=3600');
?>
<!DOCTYPE html>
<html>
<body>
<script>

if (window.testRunner) {
  fetch('http://localhost:8000/cache/resources/prefetched-main-resource.php').then(function(response) {
    if (internals.fetchResponseSource(response) != "Disk cache") {
      document.getElementById('log').innerText = 'FAIL: resource is not in the disk cache.';
    }
    testRunner.notifyDone();
  });
}

</script>
<div id="log"><?php if ($_SERVER["HTTP_PURPOSE"] == "prefetch") echo 'PASS'; else echo 'FAIL'; ?></div>
</body>
</html>
