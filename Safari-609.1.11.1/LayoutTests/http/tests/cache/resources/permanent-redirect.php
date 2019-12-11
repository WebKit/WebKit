<?
$location = $_GET['location'];

header('HTTP/1.1 301 Permanent Redirect');
header('Location:' . $location);
?>
