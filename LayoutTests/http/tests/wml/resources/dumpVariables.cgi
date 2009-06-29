#!/usr/bin/perl -w

use CGI;

$query = new CGI;
$method = $query->request_method();
$queryString = $query->query_string;
$cat = $query->param("category");
$title = $query->param("title");
$url = $query->param("url");
$desc = $query->param("desc");

print "Content-type: text/html\r\n";
print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
print "\r\n";

print <<HERE_DOC_END;
<html>
<body>
This page was requested with an HTTP $method<br/><br/>
The query parameters are: $queryString<br/>
Form content category: '$cat'<br/>
Form content title   : '$title'<br/>
Form content url     : '$url'<br/>
Form content desc    : '$desc'<br/>
</body>
</html>
HERE_DOC_END
