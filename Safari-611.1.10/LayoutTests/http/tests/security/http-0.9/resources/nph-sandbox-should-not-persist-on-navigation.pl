#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

print <<'EOF';
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="refresh" content="0; url=sandbox-should-not-persist-on-navigation.html">
</head>
</html>
EOF
