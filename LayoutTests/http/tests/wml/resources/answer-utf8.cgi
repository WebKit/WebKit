#!/usr/bin/perl -w

use CGI;

$query = new CGI;
$method = $query->request_method();
$queryString = $query->query_string;
$var1 = $query->param("var1");
$var2 = $query->param("var2");

print "Content-type: text/html;charset=utf-8\r\n";
print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
print "\r\n";

print <<HERE_DOC_END;
<html>
<body>
This page was requested with an HTTP $method<br/><br/>
The query parameters are: $queryString<br/>
Form content var1: '$var1'<br/>
Form content var2: '$var2'<br/>
</body>
</html>
HERE_DOC_END
