#!/usr/bin/perl
# This script relies on its nph- filename to invoke the CGI non-parsed-header facility.

print <<'EOF';
<!DOCTYPE html>
<html>
<body>
<p>This content should be visible, but script should not execute.</p>
<script>alert("FAIL")</script>
</body>
</html>
EOF
