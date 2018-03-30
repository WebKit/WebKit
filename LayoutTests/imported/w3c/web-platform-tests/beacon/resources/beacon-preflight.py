import json

def respondToCORSPreflight(request, response):
  allow_cors = int(request.GET.first("allowCors", 0)) != 0;

  if not allow_cors:
    response.set_error(400, "Not allowed")
    return "ERROR: Not allowed"

  if not "Access-Control-Request-Method" in request.headers:
    response.set_error(400, "No Access-Control-Request-Method header")
    return "ERROR: No access-control-request-method in preflight!"

  headers = [("Content-Type", "text/plain")]
  headers.append(("Access-Control-Allow-Origin", request.headers.get("Origin", "*")))
  headers.append(("Access-Control-Allow-Credentials", "true"))
  requested_method = request.headers.get("Access-Control-Request-Method", None)
  headers.append(("Access-Control-Allow-Methods", requested_method))
  requested_headers = request.headers.get("Access-Control-Request-Headers", None)
  headers.append(("Access-Control-Allow-Headers", requested_headers))
  headers.append(("Access-Control-Max-Age", "60"))
  return headers, ""

def main(request, response):
  command = request.GET.first("cmd").lower();
  test_id = request.GET.first("id")
  stashed_data = request.server.stash.take(test_id)
  if stashed_data is None:
    stashed_data = { 'preflight': 0, 'beacon': 0, 'preflight_requested_method': '', 'preflight_requested_headers': '', 'preflight_referrer': '', 'preflight_cookie_header': '', 'beacon_cookie_header': '' }

  if command == "put":
    if request.method == "OPTIONS":
      stashed_data['preflight'] = 1;
      stashed_data['preflight_requested_method'] = request.headers.get("Access-Control-Request-Method", "")
      stashed_data['preflight_requested_headers'] = request.headers.get("Access-Control-Request-Headers", "")
      stashed_data['preflight_cookie_header'] = request.headers.get("Cookie", "");
      stashed_data['preflight_referer'] = request.headers.get("Referer", "")
      stashed_data['preflight_origin'] = request.headers.get("Origin", "")
      request.server.stash.put(test_id, stashed_data)
      return respondToCORSPreflight(request, response)
    elif request.method == "POST":
      stashed_data['beacon'] = 1;
      stashed_data['beacon_cookie_header'] = request.headers.get("Cookie", "")
      stashed_data['beacon_origin'] = request.headers.get("Origin", "")
      stashed_data['url'] = request.url
      request.server.stash.put(test_id, stashed_data)
    return [("Content-Type", "text/plain")], ""

  if command == "get":
    if stashed_data is not None:
      return [("Content-Type", "text/plain")], json.dumps(stashed_data)
    return [("Content-Type", "text/plain")], ""

  response.set_error(400, "Bad Command")
  return "ERROR: Bad Command!"
