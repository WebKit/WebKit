#!/usr/bin/perl -w

use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html\n";
print "\n";

print <<"END";
<form method="POST" enctype="application/x-www-form-urlencoded" action="#foo">
  <input type="text" name="f">
  <input type="submit" value="Submit">
</form>
<div id="result"></div>
<script>
onunload = function() {
  // no page cache
}
onload = function() {
  alert("stage: " + sessionStorage.stage);
  switch (sessionStorage.stage++) {
  case 1:
    document.forms[0].submit();
    break;
  case 2:
    history.back();
    break;
  case 3:
    document.getElementById("result").innerText = "PASS";
    if (window.layoutTestController)
      layoutTestController.notifyDone();
    break;
  }
}
</script>
END
