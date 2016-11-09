#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

print <<'EOF';
<!DOCTYPE html>
<html>
<body>
<img src="http://127.0.0.1:8000/security/http-0.9/resources/nph-image.pl">
</body>
</html>
EOF
