{
    "uri": "<?php echo $_SERVER['REQUEST_URI'] ?>",
    "method": "<?php echo $_SERVER['REQUEST_METHOD']; ?>",
    "headers": {
<?php foreach (getallheaders() as $name => $value) { ?>
        "<?php echo $name; ?>": "<?php echo $value; ?>",
<?php } ?> },
    "get": {
<?php foreach ($_GET as $name => $value) { ?>
        "<?php echo $name; ?>": "<?php echo $value; ?>",
<?php } ?>
    },
    "post": {
<?php foreach ($_POST as $name => $value) { ?>
        "<?php echo $name; ?>": "<?php echo $value; ?>",
<?php } ?>
    },
}
