def main(request, response):
    token = request.GET.first(b'token')
    is_query = request.GET.first(b'query', None) is not None

    count = request.server.stash.take(token) or 0
    if not is_query:
        count += 1
    request.server.stash.put(token, count)

    headers = [(b'Cache-Control', b'no-store'), (b'Access-Control-Allow-Origin', b'*')]
    if is_query:
        return headers + [(b'Content-Type', b'text/plain')], str(count)

    return headers + [(b'Content-Type', b'text/html')], u'''<!doctype html>
<meta charset="utf-8">
<body>
<script>
if (window.parent !== window)
    parent.postMessage("destination-loaded", "*");
else {
    // Leave time for a second request of this document to reach the server before reading the count.
    setTimeout(async () => {
        const response = await fetch(location.pathname + "?query=1&token=" + new URLSearchParams(location.search).get("token"));
        document.body.textContent = "Requests for the destination: " + await response.text();
        testRunner.notifyDone();
    }, 500);
}
</script>
</body>
'''
