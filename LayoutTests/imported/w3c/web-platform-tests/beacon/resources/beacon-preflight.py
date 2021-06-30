import json
from wptserve.utils import isomorphic_decode

def respondToCORSPreflight(request, response):
  allow_cors = int(request.GET.first(b"allowCors", 0)) != 0;

  if not allow_cors:
    response.set_error(400, "Not allowed")
    return "ERROR: Not allowed"

  if not b"Access-Control-Request-Method" in request.headers:
    response.set_error(400, "No Access-Control-Request-Method header")
    return "ERROR: No access-control-request-method in preflight!"

  headers = [(b"Content-Type", b"text/plain")]
  headers.append((b"Access-Control-Allow-Origin", request.headers.get(b"Origin", b"*")))
  headers.append((b"Access-Control-Allow-Credentials", b"true"))
  requested_method = request.headers.get(b"Access-Control-Request-Method", None)
  headers.append((b"Access-Control-Allow-Methods", requested_method))
  requested_headers = request.headers.get(b"Access-Control-Request-Headers", None)
  headers.append((b"Access-Control-Allow-Headers", requested_headers))
  headers.append((b"Access-Control-Max-Age", b"60"))
  return headers, ""

def main(request, response):
  command = request.GET.first(b"cmd").lower();
  test_id = request.GET.first(b"id")
  stashed_data = request.server.stash.take(test_id)
  if stashed_data is None:
    stashed_data = { 'preflight': 0, 'beacon': 0, 'preflight_requested_method': '', 'preflight_requested_headers': '', 'preflight_referrer': '', 'preflight_cookie_header': '', 'beacon_cookie_header': '' }

  if command == b"put":
    if request.method == "OPTIONS":
      stashed_data['preflight'] = 1;
      stashed_data['preflight_requested_method'] = isomorphic_decode(request.headers.get(b"Access-Control-Request-Method", b""))
      stashed_data['preflight_requested_headers'] = isomorphic_decode(request.headers.get(b"Access-Control-Request-Headers", b""))
      stashed_data['preflight_cookie_header'] = isomorphic_decode(request.headers.get(b"Cookie", b""));
      stashed_data['preflight_referer'] = isomorphic_decode(request.headers.get(b"Referer", b""))
      stashed_data['preflight_origin'] = isomorphic_decode(request.headers.get(b"Origin", b""))
      request.server.stash.put(test_id, stashed_data)
      return respondToCORSPreflight(request, response)
    elif request.method == "POST":
      stashed_data['beacon'] = 1;
      stashed_data['beacon_cookie_header'] = isomorphic_decode(request.headers.get(b"Cookie", b""))
      stashed_data['beacon_origin'] = isomorphic_decode(request.headers.get(b"Origin", b""))
      stashed_data['url'] = request.url
      request.server.stash.put(test_id, stashed_data)
    return [(b"Content-Type", b"text/plain")], ""

  if command == b"get":
    if stashed_data is not None:
      return [(b"Content-Type", b"text/plain")], json.dumps(stashed_data)
    return [(b"Content-Type", b"text/plain")], ""

  response.set_error(400, "Bad Command")
  return "ERROR: Bad Command!"
