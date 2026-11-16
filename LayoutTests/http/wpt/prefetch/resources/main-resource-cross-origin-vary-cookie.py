from wptserve.utils import isomorphic_decode

def main(request, response):
    headers = [(b"Content-Type", b"text/html"), (b"Vary", b"Cookie")]

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

    return headers, document % {'cookie': isomorphic_decode(request.headers.get(b"Cookie", b"")) }
