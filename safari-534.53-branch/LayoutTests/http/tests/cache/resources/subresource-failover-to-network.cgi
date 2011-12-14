#!/usr/bin/perl -w

print "content-type: text/html\n";
print "\n";
print <<EOF
<script>
try {
  var result = document.cookie.split(';')[0].split('=')[1];
  document.cookie = "result=PASS"; // for next time

  var r = new XMLHttpRequest();
  r.open('POST', 'echo-no-store.cgi', false);
  r.send(result);
  document.write(r.responseText);
} catch (e) {
  document.write(e);
}
</script>
EOF
