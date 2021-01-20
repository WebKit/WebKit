from base64 import decodestring
import time

png_response = decodestring('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVR4nGNiAAAABgADNjd8qAAAAABJRU5ErkJggg==')

def main(request, response):
    referrer = request.headers.get("Referer", "")
    if not referrer:
        return 200, [], png_response
    return 404

