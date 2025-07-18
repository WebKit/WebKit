import time


def main(request, response):
    delay = 0.1
    headers = []
    if b"delay" in request.GET:
        delay = float(request.GET.first(b"delay"))
    response.headers.set(b"Content-type", b"text/javascript")
    response.headers.append(b"Access-Control-Allow-Origin", b"*")
    response.write_status_headers()
    time.sleep(delay)
    response.writer.write_content("self.onfetch = ")
    time.sleep(delay)
    response.writer.write_content("e")
    time.sleep(delay)
    response.writer.write_content(" => ")
    time.sleep(delay)
    response.writer.write_content(" { };")
