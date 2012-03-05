<?
$file = "checkerboard.jpg";
$size = filesize($file);
$end_marker_size = 2; // Strip the end marker (ff d9)
$contents = file_get_contents($file, false, NULL, 0, $size - $end_marker_size);
header("Content-Type: image/jpeg");
header("Content-Length: " . $size);
flush();
echo $contents;
?>
