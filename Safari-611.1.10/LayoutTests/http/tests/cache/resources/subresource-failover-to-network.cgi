#!/usr/bin/perl -w

print "content-type: text/html\n";
print "\n";
print <<EOF
<script>
try {
  var result = document.cookie.split(';')[0].split('=')[1];

  var r = new XMLHttpRequest();
  r.open('POST', 'echo-no-store.cgi', false);
  r.send(result);
  document.write(r.responseText);
} catch (e) {
  document.write(e);
}
if (!sessionStorage.subresourceFailoverToNetworkCGILoadedBefore) {
  sessionStorage.subresourceFailoverToNetworkCGILoadedBefore = true;
  document.cookie = "subresourceFailoverToNetwork=PASS"; // for next time
  history.back();
} else {
  sessionStorage.removeItem("subresourceFailoverToNetworkLoadedBefore");
  sessionStorage.removeItem("subresourceFailoverToNetworkCGILoadedBefore");
  document.cookie = "subresourceFailoverToNetwork=; expires=Thu, 01 Jan 1970 00:00:01 GMT;";
  if (window.testRunner)
    testRunner.notifyDone();
}
</script>
EOF
