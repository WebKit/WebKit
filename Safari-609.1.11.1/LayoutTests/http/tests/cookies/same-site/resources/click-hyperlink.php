<!DOCTYPE>
<html>
<head>
<script>
if (window.testRunner)
    testRunner.waitUntilDone();
</script>
</head>
<body>
<?php
    $targetAttribute = "";
    if (!empty($_GET["target"]))
        $targetAttribute = 'target="' . $_GET["target"] . '"';
?>
<a href="<?php echo $_GET['href']; ?>" <?php echo $targetAttribute; ?>>Click</a>
<script>
internals.withUserGesture(() => {
    document.querySelector("a").click();
});
</script>
</body>
</html>
