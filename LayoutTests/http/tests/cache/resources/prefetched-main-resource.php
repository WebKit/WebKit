<?php
header('Cache-Control: max-age=3600');
?>
<!DOCTYPE html>
<html>
<body>
<script>

if (window.testRunner)
   testRunner.notifyDone();

</script>
<?php
if ($_SERVER["HTTP_PURPOSE"] == "prefetch") {
    print('PASS');
} else {
    print('FAIL');
}
?>
</body>
</html>
