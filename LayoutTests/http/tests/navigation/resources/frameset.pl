#!/usr/bin/perl
# Simple script to generate a frameset, where the main frame is 
# determined by a URL query parameter.
#
# Note we depend on the "main" frame being on top, so mouse coords
# are the same for that view whether it is in a frame or on its
# own, which makes the other tests that generate events work within
# this frameset wrapper.

use CGI;
$query = new CGI;
$frameURL = $query->param('frameURL');

print "Content-type: text/html\r\n";
print "\r\n";

print <<HERE_DOC_END
<html>
<script type="text/javascript" src="testcode.js"></script>
<frameset rows="90%,10%">
<frame src="$frameURL" name="main">
<frame src="otherpage.html" name="footer">
</frameset>
</html>
HERE_DOC_END

