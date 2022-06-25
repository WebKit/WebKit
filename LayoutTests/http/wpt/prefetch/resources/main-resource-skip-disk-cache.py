from wptserve.utils import isomorphic_decode

def main(request, response):
    headers = [(b"Content-Type", b"text/html")]

    document = """
<!DOCTYPE html>
<script>
  async function test() {
  var result = '%(prefetch)s';
  if (window.testRunner) {
    var response = await fetch('%(url)s');
    if (internals.fetchResponseSource(response) == "Disk cache") {
      result = 'FAIL.';
    }
  }
  window.opener.postMessage(result, '*');
  }
</script>
<body onload="test()">
"""

    return headers, document % {'prefetch': isomorphic_decode(request.headers.get(b"Purpose", b"")), 'url': request.url }
