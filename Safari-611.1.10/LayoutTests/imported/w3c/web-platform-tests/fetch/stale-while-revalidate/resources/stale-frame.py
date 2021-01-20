import random, string, datetime, time

def id_token():
   letters = string.ascii_lowercase
   return ''.join(random.choice(letters) for i in range(20))

def main(request, response):
    is_revalidation = request.headers.get("If-None-Match", None)
    token = request.GET.first("token", None)
    is_query = request.GET.first("query", None) != None
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
      headers = [("Count", count), ("Test", str(request.raw_headers))]
      content = ""
      return 200, headers, content
    else:
      unique_id = id_token()
      headers = [("Content-Type", "text/html"),
                 ("Cache-Control", "private, max-age=0, stale-while-revalidate=60"),
                 ("ETag", '"swr"'),
                 ("Unique-Id", unique_id)]
      content = "<body>{}</body>".format(unique_id)
      return 200, headers, content
