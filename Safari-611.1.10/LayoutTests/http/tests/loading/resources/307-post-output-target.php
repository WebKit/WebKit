<?php
$sortedKeys = array_keys($_POST);
sort($sortedKeys);
?>
<html><body>
<?php
if ( sizeof($_POST) == 0)
    echo "There were no POSTed form values.";
else
    echo "Form values are:<br>";

foreach ($sortedKeys as $value) {
    echo "$value : $_POST[$value]";
    echo "<br>";
}
?>
<script>
if (window.testRunner)
   testRunner.notifyDone();
</script>
</body>
</html>
