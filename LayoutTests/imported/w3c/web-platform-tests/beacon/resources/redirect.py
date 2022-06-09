from urllib.parse import urlencode, urlparse
from wptserve.utils import isomorphic_decode, isomorphic_encode

def main(request, response):
    stashed_data = {'count': 0, 'preflight': "0"}
    status = 302
    headers = [(b"Content-Type", b"text/plain"),
               (b"Cache-Control", b"no-cache"),
               (b"Pragma", b"no-cache"),
               (b"Access-Control-Allow-Credentials", b"true")]
    headers.append((b"Access-Control-Allow-Origin", request.headers.get(b"Origin", b"*")))
    token = None

    if b"token" in request.GET:
        token = request.GET.first(b"token")
        data = request.server.stash.take(token)
        if data:
            stashed_data = data

    if request.method == "OPTIONS":
        requested_method = request.headers.get(b"Access-Control-Request-Method", None)
        headers.append((b"Access-Control-Allow-Methods", requested_method))
        requested_headers = request.headers.get(b"Access-Control-Request-Headers", None)
        headers.append((b"Access-Control-Allow-Headers", requested_headers))
        stashed_data['preflight'] = "1"
        #Preflight is not redirected: return 200
        if not b"redirect_preflight" in request.GET:
            if token:
              request.server.stash.put(request.GET.first("token"), stashed_data)
            return 200, headers, ""

    if b"redirect_status" in request.GET:
        status = int(request.GET[b'redirect_status'])

    stashed_data['count'] += 1

    if b"location" in request.GET:
        url = isomorphic_decode(request.GET[b'location'])
        scheme = urlparse(url).scheme
        if scheme == "" or scheme == "http" or scheme == "https":
            url += "&" if '?' in url else "?"
            #keep url parameters in location
            url_parameters = {}
            for item in request.GET.items():
                url_parameters[item[0]] = item[1][0]
            url += urlencode(url_parameters)
            #make sure location changes during redirection loop
            url += "&count=" + str(stashed_data['count'])
        headers.append((b"Location", isomorphic_encode(url)))

    if b"redirect_referrerpolicy" in request.GET:
        headers.append((b"Referrer-Policy", request.GET[b'redirect_referrerpolicy']))

    if token:
        request.server.stash.put(request.GET.first(b"token"), stashed_data)
        if b"max_count" in request.GET:
            max_count =  int(request.GET[b'max_count'])
            #stop redirecting and return count
            if stashed_data['count'] > max_count:
                # -1 because the last is not a redirection
                return str(stashed_data['count'] - 1)

    return status, headers, ""
