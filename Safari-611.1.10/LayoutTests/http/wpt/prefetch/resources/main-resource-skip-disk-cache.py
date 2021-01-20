def main(request, response):
    headers = [("Content-Type", "text/html")]

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

    return headers, document % {'prefetch': request.headers.get("Purpose", ""), 'url': request.url }
