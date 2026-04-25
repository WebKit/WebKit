import time

def main(request, response):
    delay = 0.05
    headers = []
    if b"delay" in request.GET:
        delay = float(request.GET.first(b"delay"))
    response.headers.set(b"Content-type", b"text/html")
    response.headers.append(b"Access-Control-Allow-Origin", b"*")
    response.write_status_headers()
    response.writer.write_content("<script>parent.callback1();</script>")
    time.sleep(delay)
    response.writer.write_content("<script>parent.callback2();</script>")
    time.sleep(delay)
