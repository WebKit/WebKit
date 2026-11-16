import json
from wptserve.utils import isomorphic_decode

def respondToCORSPreflight(request, response):
  headers = [(b"Content-Type", b"text/plain")]
  allow_cors = int(request.GET.first(b"allowCors", 0)) != 0;
  
  if not allow_cors:
    response.set_error(400, u"Not allowed")
    return headers, u"ERROR: Not allowed"
  
  if not b"Access-Control-Request-Method" in request.headers:
    response.set_error(400, u"No Access-Control-Request-Method header")
    return headers, u"ERROR: No access-control-request-method in preflight!"
  
  headers.append((b"Access-Control-Allow-Origin", request.headers.get(b"Origin", b"*")))
  headers.append((b"Access-Control-Allow-Credentials", b"true"))
  requested_method = request.headers.get(b"Access-Control-Request-Method", None)
  headers.append((b"Access-Control-Allow-Methods", requested_method))
  requested_headers = request.headers.get(b"Access-Control-Request-Headers", None)
  headers.append((b"Access-Control-Allow-Headers", requested_headers))
  headers.append((b"Access-Control-Max-Age", b"60"))
  return headers, u""

def main(request, response):
  command = request.GET.first(b"cmd").lower();
  test_id = request.GET.first(b"id")
  stashed_data = request.server.stash.take(test_id)
  if stashed_data is None:
    stashed_data = { u'preflight': 0, u'beacon': 0, u'preflight_requested_method': u'', u'preflight_requested_headers': u'', u'preflight_referrer': u'', u'preflight_cookie_header': u'', u'beacon_cookie_header': u'' }

  if command == b"put":
    if request.method == u"OPTIONS":
      stashed_data[u'preflight'] = 1;
      stashed_data[u'preflight_requested_method'] = isomorphic_decode(request.headers.get(b"Access-Control-Request-Method", b""))
      stashed_data[u'preflight_requested_headers'] = isomorphic_decode(request.headers.get(b"Access-Control-Request-Headers", b""))
      stashed_data[u'preflight_cookie_header'] = isomorphic_decode(request.headers.get(b"Cookie", b""))
      stashed_data[u'preflight_referer'] = isomorphic_decode(request.headers.get(b"Referer", b""))
      stashed_data[u'preflight_origin'] = isomorphic_decode(request.headers.get(b"Origin", b""))
      request.server.stash.put(test_id, stashed_data)
      return respondToCORSPreflight(request, response)
    elif request.method == u"POST":
      stashed_data[u'beacon'] = 1;
      stashed_data[u'beacon_cookie_header'] = isomorphic_decode(request.headers.get(b"Cookie", b""))
      stashed_data[u'beacon_origin'] = isomorphic_decode(request.headers.get(b"Origin", b""))
      request.server.stash.put(test_id, stashed_data)
    return [(b"Content-Type", b"text/plain")], u""

  if command == b"get":
    return [(b"Content-Type", b"text/plain")], json.dumps(stashed_data)

  response.set_error(400, u"Bad Command")
  return [(b"Content-Type", b"text/plain")], u"ERROR: Bad Command!"
