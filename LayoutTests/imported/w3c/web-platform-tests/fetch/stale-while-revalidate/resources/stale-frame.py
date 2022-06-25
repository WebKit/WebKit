import random, string, datetime, time
from wptserve.utils import isomorphic_decode, isomorphic_encode

def id_token():
   letters = string.ascii_lowercase
   return ''.join(random.choice(letters) for i in range(20))

def main(request, response):
    is_revalidation = request.headers.get(b"If-None-Match", None)
    token = request.GET.first(b"token", None)
    is_query = request.GET.first(b"query", None) != None
    with request.server.stash.lock:
      value = request.server.stash.take(token)
      count = 0
      if value != None:
        count = int(value)
      if is_query:
        if count < 2:
          request.server.stash.put(token, count)
      else:
        if is_revalidation is not None:
          count = count + 1
        request.server.stash.put(token, count)

    if is_query:
      headers = [(b"Count", isomorphic_encode(str(count))), (b"Test", isomorphic_encode(str(request.raw_headers)))]
      content = ""
      return 200, headers, content
    else:
      unique_id = id_token()
      headers = [(b"Content-Type", b"text/html"),
                 (b"Cache-Control", b"private, max-age=0, stale-while-revalidate=60"),
                 (b"ETag", b'"swr"'),
                 (b"Unique-Id", isomorphic_encode(unique_id))]
      content = "<body>{}</body>".format(unique_id)
      return 200, headers, content
