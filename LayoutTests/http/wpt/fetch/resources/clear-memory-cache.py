import time


def main(request, response):
    headers = [("Content-Type", "text/plain")]
    headers.append(("Access-Control-Allow-Origin", "*"))
    headers.append(("Access-Control-Allow-Methods", "GET"))
    headers.append(("Access-Control-Allow-Headers", "header"))

    uuid = request.GET[b"uuid"]
    count = request.server.stash.take(uuid)
    if count is None:
        count = 0

    if request.method == "OPTIONS":
        headers.append(("Cache-Control", "max-age=100000"))
        request.server.stash.put(uuid, count + 1)
        return 200, headers, ""

    request.server.stash.put(uuid, count)

    headers.append(("Cache-Control", "no-cache"))
    return 200, headers, "" + str(count)
