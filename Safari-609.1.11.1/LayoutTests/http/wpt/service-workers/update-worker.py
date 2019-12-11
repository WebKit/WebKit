import time

script = '''
// Time stamp: %s
// (This ensures the source text is *not* a byte-for-byte match with any
// previously-fetched version of this script.)

self.addEventListener("fetch", async (event) => {
    if (event.request.url.indexOf("mouse") !== -1)
        event.respondWith(new Response("cat"));
});'''


def main(request, response):
  return [('Content-Type', 'application/javascript'), ('Cache-Control', 'np-cache, no-store')], script % time.time()
