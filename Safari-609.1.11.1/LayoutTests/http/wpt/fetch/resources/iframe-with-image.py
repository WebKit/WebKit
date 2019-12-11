ETAG = '"123abc"'


def main(request, response):
    test_id = request.GET.first("uuid")
    response.status = (200, "OK")
    response.headers.set("Content-Type", 'text/html')
    return "<!doctype html><image src='image-load.py?uuid=" + test_id + "'></image>"
