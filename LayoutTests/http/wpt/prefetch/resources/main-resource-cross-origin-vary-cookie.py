def main(request, response):
    headers = [("Content-Type", "text/html"), ("Vary", "Cookie")]

    document = """
<!DOCTYPE html>
<script>
  function test() {
    var result = '%(cookie)s';
    window.opener.postMessage(result, '*');
  }
</script>
<body onload="test()">
"""

    return headers, document % {'cookie': request.headers.get("Cookie", "") }
