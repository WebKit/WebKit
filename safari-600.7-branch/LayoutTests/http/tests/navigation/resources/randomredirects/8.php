<?php

if (array_key_exists("If-Modified-Since", apache_request_headers())) {
	header('HTTP/1.1 304 Not Modified');
	header('Cache-Control: private, s-maxage=0, max-age=0, must-revalidate');
	exit;
} else {
	header('Content-Type: text/html; charset=utf-8');
	header('Cache-Control: private, s-maxage=0, max-age=0, must-revalidate');
	header('Expires: Thu, 01 Jan 1970 00:00:00 GMT');
	header('Vary: Accept-Encoding, Cookie');
	header('Last-Modified: Mon, 1 Apr 2013 12:12:12 GMT');
}

?>
You're on page 8.php
