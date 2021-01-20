#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

print <<'EOF';
<!DOCTYPE html>
<html>
<body>
<embed type="application/x-webkit-test-netscape">Plugin did not load.</embed>
</body>
</html>
EOF
