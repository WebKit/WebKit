def main(request, response):
    headers = [("Content-Type", "text/html")]

    document = """
<!DOCTYPE html>
<script>
  window.opener.postMessage('{result}', '*');
</script>
""".format(result=request.headers.get("Sec-Purpose", ""))

    return headers, document
