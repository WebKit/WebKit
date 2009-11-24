#!/usr/bin/perl -w

print "content-type: text/html\n";
print "\n";
print <<EOF
<script>
try {
  var r = new XMLHttpRequest();
  r.open('POST', 'echo-no-store.cgi', false);
  r.send('PASS');
  document.write(r.responseText);
} catch (e) {
  document.write(e);
}
</script>
EOF
