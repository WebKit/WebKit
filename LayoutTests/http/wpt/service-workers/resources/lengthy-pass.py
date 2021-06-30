import time

def main(request, response):
    delay = 0.05
    headers = []
    if b"delay" in request.GET:
        delay = float(request.GET.first(b"delay"))
    response.headers.set(b"Content-type", b"text/javascript")
    response.headers.append(b"Access-Control-Allow-Origin", b"*")
    response.write_status_headers()
    time.sleep(delay);
    response.writer.write_content("document")
    time.sleep(delay)
    response.writer.write_content(".body")
    time.sleep(delay)
    response.writer.write_content(".innerHTML = ")
    time.sleep(delay)
    response.writer.write_content("'PASS'")
    time.sleep(delay)
