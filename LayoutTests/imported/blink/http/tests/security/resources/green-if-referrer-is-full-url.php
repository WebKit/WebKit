<?php
header('Content-Type: image/png');
if ($_SERVER['HTTP_REFERER'] != 'http://127.0.0.1:8000/security/referrer-policy-attribute-img-unsafe-url.html') {
    $img = 'red200x100.png';
} else {
    $img = 'green250x50.png';
}
echo file_get_contents($img);
?>