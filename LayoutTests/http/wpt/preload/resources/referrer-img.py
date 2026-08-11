from base64 import decodebytes
import time

png_response = decodebytes(b'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVR4nGNiAAAABgADNjd8qAAAAABJRU5ErkJggg==')

def main(request, response):
    referrer = request.headers.get(b"Referer", b"")
    if not referrer:
        return 200, [], png_response
    return 404

