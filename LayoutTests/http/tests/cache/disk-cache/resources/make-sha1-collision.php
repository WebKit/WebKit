<?
$path = $_GET['path'];
header('HTTP/1.1 200 OK');
header("Cache-control: max-age=10000");
header("Content-Type: application/pdf");
$content = file_get_contents($path);
$collidingContent = str_replace("SVN is the best!", "SHA-1 is dead!!!", $content);
echo $collidingContent;
?>
