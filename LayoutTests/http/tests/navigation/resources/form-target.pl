#!/usr/bin/perl
# Simple script to that dumps the HTTP request method and all input parameters.

use CGI;
$query = new CGI;

print "Content-type: text/html\r\n";
print "\r\n";

$method = $query->request_method();

print <<HEADER;
<body>
<p>This page was requested with the HTTP method $method.</p>

<p>Parameters:</p>
<ul>
HEADER

@paramNames = $query->param;

foreach $paramName (@paramNames)
{
    print "<li>" . $paramName . " = " . $query->param($paramName) . "</li>"
}

print <<FOOTER
</ul>
<script>
var isDone = true;
if (sessionStorage.formTargetShouldNavAndGoBack) {
  if (sessionStorage.didNav) {
      delete sessionStorage.didNav;
      delete sessionStorage.formTargetShouldNavAndGoBack;
  } else {
      isDone = false;
      sessionStorage.didNav = true;
      onload = function() {
          setTimeout(function() {window.location.href = 'go-back.html'}, 0);
      };
  }
}

if (isDone && window.layoutTestController)
    layoutTestController.notifyDone();

</script>
</body>
FOOTER
