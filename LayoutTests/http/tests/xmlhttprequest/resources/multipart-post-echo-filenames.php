<?php
$first = True;
foreach ($_FILES as $file) {
    if (!$first)
        echo ",";
    echo $file['name'];
    $first = False;
}
?>
