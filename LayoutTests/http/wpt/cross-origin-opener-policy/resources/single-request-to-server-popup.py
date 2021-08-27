def main(request, response):
    token = request.GET[b'token']
    value = request.server.stash.take(token)
    already_loaded_string = "not_already_loaded"
    if value is not None:
        already_loaded_string = "already_loaded"
    else:
        request.server.stash.put(token, True)
    return ([
        (b'Cache-Control', b'no-store'),
        (b'Cross-Origin-Opener-Policy', b'same-origin'),
        (b'Content-Type', b'text/html')],
        u'<script>var bc = new BroadcastChannel("single-request-to-server"); bc.postMessage("%s");</script>\n' % already_loaded_string)
