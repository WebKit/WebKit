import time

def main(request, response):
    delay = 0.05
    headers = []
    if "delay" in request.GET:
        delay = float(request.GET.first("delay"))
    response.headers.set("Content-type", "text/javascript")
    response.headers.append("Access-Control-Allow-Origin", "*")
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
