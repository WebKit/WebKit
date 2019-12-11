<?
function contentType($path)
{
    if (preg_match("/\.html$/", $path))
        return "text/html";
    if (preg_match("/\.manifest$/", $path))
        return "text/cache-manifest";
    if (preg_match("/\.js$/", $path))
        return "text/javascript";
    if (preg_match("/\.xml$/", $path))
        return "application/xml";
    if (preg_match("/\.xhtml$/", $path))
        return "application/xhtml+xml";
    if (preg_match("/\.svg$/", $path))
        return "application/svg+xml";
    if (preg_match("/\.xsl$/", $path))
        return "application/xslt+xml";
    if (preg_match("/\.gif$/", $path))
        return "image/gif";
    if (preg_match("/\.jpg$/", $path))
        return "image/jpeg";
    if (preg_match("/\.png$/", $path))
        return "image/png";
    return "text/plain";
}

$path = $_GET['path'];
$expectedReferer = $_GET['expected-referer'];
$referer = $_SERVER["HTTP_REFERER"];

if ($expectedReferer == $referer && file_exists($path)) {
    header('HTTP/1.1 200 OK');
    header("Cache-control: no-store");
    header("Content-Type: " . contentType($path));
    print file_get_contents($path);
} else {
    header('HTTP/1.1 404 Not Found');
}

?>
