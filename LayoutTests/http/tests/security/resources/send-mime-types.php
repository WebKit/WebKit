<?php
    $mime_type = $_GET["mt"];
    
    header("Content-Type: ".$mime_type);

    echo "<script>";
    echo "alert('FAIL: ".$mime_type."');";
    echo "</script>";
?>
