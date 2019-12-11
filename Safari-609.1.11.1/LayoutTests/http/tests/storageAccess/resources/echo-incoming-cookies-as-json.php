<?php
if(empty($_COOKIE)) {
    echo json_encode('No cookies');
} else {
    echo json_encode($_COOKIE);
}
?>