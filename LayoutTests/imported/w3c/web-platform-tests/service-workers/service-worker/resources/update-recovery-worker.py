def main(request, response):
    # Keep the state per key, not in a cookie, so that fetches of the script
    # left over from another run cannot change what this run gets.
    key = request.GET[b'key']
    already_requested = request.server.stash.take(key)
    request.server.stash.put(key, True)

    # no-cache itself to ensure the user agent finds a new version for each update.
    headers = [(b'Cache-Control', b'no-cache, must-revalidate'),
               (b'Pragma', b'no-cache')]

    extra_body = b''

    if already_requested is None:
        # Install a bad service worker that will break the controlled
        # document navigation.
        extra_body = b"addEventListener('fetch', function(e) { e.respondWith(Promise.reject()); });"
    # Otherwise the update is pulling the script again; update to a
    # worker that does not break document navigation. Serve the same
    # script from then on.

    headers.append((b'Content-Type', b'application/javascript'))
    return headers, b'%s' % (extra_body)
