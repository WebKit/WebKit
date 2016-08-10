<?php
# See the HLS ITEF spec: <http://tools.ietf.org/id/draft-pantos-http-live-streaming-12.txt>
header("Status: 200 OK");
header("HTTP/1.1 200 OK");
header("Connection: close");
header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");
header("Pragma: no-cache");
header("Etag: " . '"' . filesize(__FILE__) . "-" . filemtime(__FILE__) . '"');
header("Content-Type: application/x-mpegurl");

$chunkDuration = 6.0272;
$chunkCount = 5;
if (array_key_exists("duration", $_GET)) 
    $chunkCount = intval($_GET["duration"] / $chunkDuration);

function println($string) { return print($string . PHP_EOL); }
println("#EXTM3U");
println("#EXT-X-TARGETDURATION:7");
println("#EXT-X-VERSION:4");
println("#EXT-X-MEDIA-SEQUENCE:0");
println("#EXT-X-PLAYLIST-TYPE:VOD");

for ($i = 0; $i < $chunkCount; ++$i) {
    println("#EXTINF:" . $chunkDuration . ",");
    println("test.ts");
}

println("#EXT-X-ENDLIST");
