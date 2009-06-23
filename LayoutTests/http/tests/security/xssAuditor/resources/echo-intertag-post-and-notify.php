<?php
# Silly magic quotes.  We're using an old version of PHP.
if (get_magic_quotes_gpc()) {
    function stripslashes_gpc(&$value) {
        $value = stripslashes($value);
    }
    array_walk_recursive($_GET, 'stripslashes_gpc');
    array_walk_recursive($_POST, 'stripslashes_gpc');
    array_walk_recursive($_COOKIE, 'stripslashes_gpc');
    array_walk_recursive($_REQUEST, 'stripslashes_gpc');
}
?>
<!DOCTYPE html>
<html>
<body>
<?php
echo $_POST['q'];
?>
<script>
if (window.layoutTestController)
    layoutTestController.notifyDone();
</script>
</body>
</html>
