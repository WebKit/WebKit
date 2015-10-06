<?php
header('Content-Type: image/png');
if ($_SERVER['HTTP_REFERER'] != '') {
    $img = 'red200x100.png';
} else {
    $img = 'green250x50.png';
}
echo file_get_contents($img);
?>
